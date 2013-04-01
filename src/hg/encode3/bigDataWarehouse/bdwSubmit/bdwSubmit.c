/* bdwSubmit - Submit URL with validated.txt to warehouse.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "net.h"
#include "options.h"
#include "errabort.h"
#include "dystring.h"
#include "errCatch.h"
#include "sqlNum.h"
#include "cheapcgi.h"
#include "net.h"
#include "md5.h"
#include "portable.h"
#include "obscure.h"
#include "hex.h"
#include "bigDataWarehouse.h"
#include "bdwLib.h"

char *licensePlatePrefix = "ENCFF";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bdwSubmit - Submit URL with validated.txt to warehouse.\n"
  "usage:\n"
  "   bdwSubmit validatedUrl user password\n"
  "Generally user is an email address\n"
  "options:\n"
  "   -licensePlatePrefix=prefix (default %s)\n"
  , licensePlatePrefix
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"licensePlatePrefix", OPTION_STRING},
   {NULL, 0},
};

struct fieldedRow
/* An array of strings with a little extra info, can be hung on a list. */
    {
    struct fieldedRow *next;
    char **row; // Array of strings
    int id;	// In the file case this is the line of file row starts in
    };

struct fieldedTable
/* A table with a name for each field. */
    {
    struct fieldedTable *next;
    char *name;	    /* Often the file name */
    struct lm *lm;  /* All allocations done out of this memory pool. */
    int fieldCount; /* Number of fields. */
    char **fields;  /* Names of fields. */
    struct fieldedRow *rowList;  /* list of parsed out fields. */
    struct fieldedRow **cursor;  /* Pointer to where we add next item to list. */
    };

struct fieldedTable *fieldedTableNew(char *name, char **fields, int fieldCount)
/* Create a new empty fieldedTable with given name. */
{
struct fieldedTable *table;
AllocVar(table);
struct lm *lm = table->lm = lmInit(0);
table->name = lmCloneString(lm, name);
table->cursor = &table->rowList;
table->fieldCount = fieldCount;
int i;
char **row = lmAllocArray(lm, table->fields, fieldCount);
for (i=0; i<fieldCount; ++i)
    {
    row[i] = lmCloneString(lm, fields[i]);
    }
return table;
}

void fieldedTableFree(struct fieldedTable **pTable)
/* Free up memory resources associated with table. */
{
struct fieldedTable *table = *pTable;
if (table != NULL)
    {
    lmCleanup(&table->lm);
    freez(pTable);
    }
}

struct fieldedRow *fieldedTableAdd(struct fieldedTable *table,  char **row, int rowSize, int id)
/* Create a new row and add it to table.  Return row. */
{
/* Make sure we got right number of fields. */
if (table->fieldCount != rowSize)
    errAbort("%s starts with %d fields, but at line %d has %d fields instead",
	    table->name, table->fieldCount, id, rowSize);

/* Allocate field from local memory and start filling it in. */
struct lm *lm = table->lm;
struct fieldedRow *fr;
lmAllocVar(lm, fr);
lmAllocArray(lm, fr->row, rowSize);
fr->id = id;
int i;
for (i=0; i<rowSize; ++i)
    fr->row[i] = lmCloneString(lm, row[i]);

/* Add it to end of list using cursor to avoid slReverse hassles. */
*(table->cursor) = fr;
table->cursor = &fr->next;

return fr;
}

int bdwGetHost(struct sqlConnection *conn, char *hostName)
/* Look up host name in table and return associated ID.  If not found
 * make up new table entry. */
{
/* If it's already in table, just return ID. */
char query[512];
safef(query, sizeof(query), "select id from bdwHost where name='%s'", hostName);
int hostId = sqlQuickNum(conn, query);
if (hostId > 0)
    {
    uglyf("whoot - found %s = %d\n", hostName, hostId);
    return hostId;
    }

safef(query, sizeof(query), 
   "insert bdwHost (name, lastOkTime, lastNotOkTime, firstAdded, errorMessage, uploadAttempts) "
   "values('%s', 0, 0, %lld, '', 0)", 
   hostName, (long long)time(NULL));
sqlUpdate(conn, query);
return sqlLastAutoId(conn);
}

int bdwGetSubmissionDir(struct sqlConnection *conn, int hostId, char *submissionDir)
/* Get submissionDir from database, creating it if it doesn't already exist. */
{
/* If it's already in table, just return ID. */
char query[512];
safef(query, sizeof(query), "select id from bdwSubmissionDir where url='%s'", submissionDir);
int dirId = sqlQuickNum(conn, query);
if (dirId > 0)
    {
    uglyf("wh0ot - found %s = %d\n", submissionDir, dirId);
    return dirId;
    }

safef(query, sizeof(query), 
   "insert bdwSubmissionDir (url, firstAdded, hostId) values('%s', %lld, %d)", 
   submissionDir, (long long)time(NULL), hostId);
sqlUpdate(conn, query);
return sqlLastAutoId(conn);
}

void recordIntoHistory(struct sqlConnection *conn, unsigned id, char *table, boolean success)
/* Record success/failure into uploadAttempts and historyBits fields of table.   */
{
/* Get historyBits and fold status into it. */
char quickResult[32];
char query[256];
safef(query, sizeof(query), "select historyBits from %s where id=%u", table, id);
if (sqlQuickQuery(conn, query, quickResult, sizeof(quickResult)) == NULL)
    internalErr();
long long historyBits = sqlLongLong(quickResult);
historyBits <<= 1;
if (success)
    historyBits |= 1;
safef(query, sizeof(query), 
    "update %s set historyBits=%lld, uploadAttempts=uploadAttempts+1, %s=%lld "
    "where id=%lld",
    table, historyBits, (success ? "lastOkTime" : "lastNotOkTime"), (long long)time(NULL),
    (long long)id);
sqlUpdate(conn, query);
}

int bdwOpenAndRecord(struct sqlConnection *conn, 
	char *submissionDir, char *submissionFile, char *url,
	int *retHostId, int *retDirId)
/* Return a low level read socket handle on URL if possible.  Consult and
 * possibly update the bdwHost and bdwDir tables so don't keep beating up
 * and timing out on same host or hub. The url should be made by combining
 * submissionDir and submissionFile. Note submissionFile itself may have
 * further directory info. */
{
/* Wrap netUrlOpen in errCatch and remember whether it works. */
struct errCatch *errCatch = errCatchNew();
int sd = -1;
boolean success = TRUE;
if (errCatchStart(errCatch))
    {
    sd = netUrlMustOpenPastHeader(url);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    success = FALSE;
    warn("Error: %s", trimSpaces(errCatch->message->string));
    }

/* Parse url into pieces */
struct netParsedUrl npu;
ZeroVar(&npu);
netParseUrl(url, &npu);
char urlDir[PATH_LEN], urlFileName[PATH_LEN], urlExtension[PATH_LEN];
splitPath(npu.file, urlDir, urlFileName, urlExtension);

/* Record success in host and submissionDir tables. */
int hostId = bdwGetHost(conn, npu.host);
recordIntoHistory(conn, hostId, "bdwHost", success);
int submissionDirId = bdwGetSubmissionDir(conn, hostId, submissionDir);
uglyf("submissionDirId=%d\n", submissionDirId);
recordIntoHistory(conn, submissionDirId, "bdwSubmissionDir", success);
#ifdef SOON
#endif /* SOON */

errCatchFree(&errCatch);
if (!success)
    noWarnAbort();

if (retHostId != NULL)
    *retHostId = hostId;
if (retDirId != NULL)
    *retDirId = submissionDirId;
return sd;
}

void bdwCheckWorthAttempting(char *url)
/* See if it's even worth attempting to connect with this URL.  We may know
 * host is down.... */
{
/* For now we do nothing, assume it's ok. */
}
    

struct fieldedTable *fieldedTableFromTabFile(char *url, char *requiredFields[], int requiredCount)
/* Read table, possible from remote url. */
{
struct lineFile *lf = lineFileOpen(url, TRUE);
char *line;

/* Get first line and turn it into field list. */
if (!lineFileNext(lf, &line, NULL))
   errAbort("%s is empty", url);
if (line[0] != '#')
   errAbort("%s must start with '#' and field names on first line", url);
line = skipLeadingSpaces(line+1);
int fieldCount = chopByChar(line, '\t', NULL, 0);
char *fields[fieldCount];
chopTabs(line, fields);

/* Make sure that all required fields are present. */
int i;
for (i = 0; i < requiredCount; ++i)
    {
    char *required = requiredFields[i];
    int ix = stringArrayIx(required, fields, fieldCount);
    if (ix < 0)
        errAbort("%s is missing required field '%s'", url, required);
    }

/* Create fieldedTable . */
struct fieldedTable *table = fieldedTableNew(url, fields, fieldCount);
while (lineFileRowTab(lf, fields))
    {
    fieldedTableAdd(table, fields, fieldCount, lf->lineIx);
    }


/* Clean up and go home. */
lineFileClose(&lf);
return table;
}

void cgiEncodeIntoDy(char *var, char *val, struct dyString *dy)
/* Add a CGI-encoded &var=val string to dy. */
{
if (dy->stringSize != 0)
    dyStringAppendC(dy, '&');
dyStringAppend(dy, var);
dyStringAppendC(dy, '=');
char *s = cgiEncode(val);
dyStringAppend(dy, s);
freez(&s);
}


struct bdwFile *bdwFileFromFieldedTable(struct fieldedTable *table,
    int fileIx, int md5Ix, int sizeIx, int modifiedIx)
/* Turn parsed out table (still all just strings) into list of bdwFiles. */
{
struct bdwFile *bf, *bfList = NULL;
struct fieldedRow *fr;
struct dyString *tags = dyStringNew(0);
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char **row = fr->row;
    AllocVar(bf);
    bf->submitFileName = cloneString(row[fileIx]);
    safef(bf->md5, sizeof(bf->md5), "%s", row[md5Ix]);
    bf->size = sqlLongLong(row[sizeIx]);
    bf->updateTime = sqlLongLong(row[modifiedIx]);

    /* Add as tags any fields not included in fixed fields. */
    dyStringClear(tags);
    int i;
    for (i=0; i<table->fieldCount; ++i)
        {
	if (i != fileIx && i != md5Ix && i != sizeIx && i != modifiedIx)
	    {
	    cgiEncodeIntoDy(table->fields[i], row[i], tags);
	    }
	}
    bf->tags = cloneString(tags->string);

    /* Fake other fields. */
    strcpy(bf->licensePlate, "");
    bf->bdwFileName  = cloneString("");
    slAddHead(&bfList, bf);
    }
slReverse(&bfList);
dyStringFree(&tags);
return bfList;
}

#ifdef EXAMPLE
void netParseUrl(char *url, struct netParsedUrl *parsed);
struct netParsedUrl
/* A parsed URL. */
   {
   char protocol[16];	/* Protocol - http or ftp, etc. */
   char user[128];	/* User name (optional)  */
   char password[128];	/* Password  (optional)  */
   char host[128];	/* Name of host computer - www.yahoo.com, etc. */
   char port[16];       /* Port, usually 80 or 8080. */
   char file[1024];	/* Remote file name/query string, starts with '/' */
   ssize_t byteRangeStart; /* Start of byte range, use -1 for none */
   ssize_t byteRangeEnd;   /* End of byte range use -1 for none */
   };
#endif /* EXAMPLE */

void makeLicensePlate(char *prefix, int ix, char *out, int outSize)
/* Make a license-plate type string composed of prefix + funky coding of ix
 * and put result in out. */
{
int maxIx = 10*10*10*26*26*26;
if (ix > maxIx)
    errAbort("ix exceeds max in makeLicensePlate.  ix %d, max %d\n", ix, maxIx);
int prefixSize = strlen(prefix);
int minSize = prefixSize + 6 + 1;
if (outSize < minSize)
    errAbort("outSize (%d) not big enough in makeLicensePlate", outSize);

/* Copy in prefix. */
strcpy(out, prefix);

/* Generate the 123ABC part of license plate backwards. */
char *s = out+minSize;
int x = ix;
*(--s) = 0;	// zero tag at end;
int i;
for (i=0; i<3; ++i)
    {
    int remainder = x%26;
    *(--s) = 'A' + remainder;
    x /= 26;
    }
for (i=0; i<3; ++i)
    {
    int remainder = x%10;
    *(--s) = '0' + remainder;
    x /= 10;
    }
}

int makeNewEmptySubmissionRecord(struct sqlConnection *conn, char *submissionUrl, 
    char *userSid)
/* Create a submission record around URL and return it's id. */
{
struct dyString *query = dyStringNew(0);
dyStringAppend(query, 
    "insert bdwSubmission "
    "(url, startUploadTime, endUploadTime, userSid, submitFileId, errorMessage) ");
dyStringPrintf(query, "VALUES('%s', %lld, %lld, '%s', 0, '')",
    submissionUrl, (long long)time(NULL), (long long)0, userSid);
uglyf("query=%s\n", query->string);
sqlUpdate(conn, query->string);
dyStringFree(&query);
return sqlLastAutoId(conn);
}

int makeNewEmptyFileRecord(struct sqlConnection *conn, unsigned submissionId, char *submitFileName)
/* Make a new, largely empty, record around file and submission info. */
{
struct dyString *query = dyStringNew(0);
dyStringAppend(query, 
    "insert bdwFile "
    "(submissionId, submitFileName) ");
dyStringPrintf(query, "VALUES(%u, '%s')",
    submissionId, submitFileName);
uglyf("query=%s\n", query->string);
sqlUpdate(conn, query->string);
dyStringFree(&query);
return sqlLastAutoId(conn);
}

void bdwRelDirForTime(time_t sinceEpoch, char dir[PATH_LEN])
/* Return the output directory for a given time. */
{
/* Get current time parsed into struct tm */
struct tm now;
gmtime_r(&sinceEpoch, &now);

/* make directory string out of year/month/day/ */
safef(dir, PATH_LEN, "%d/%d/%d/", now.tm_year+1900, now.tm_mon+1, now.tm_mday);
}

void writeErrToTableAndDie(struct sqlConnection *conn, char *table, int id, char *err)
/* Write out error to stderr and also save it in errorMessage field of submission table. */
{
char *trimmedError = trimSpaces(err);
warn("%s", trimmedError);
char escapedErrorMessage[2*strlen(trimmedError)+1];
sqlEscapeString2(escapedErrorMessage, trimmedError);
uglyf("escapedErrorMessage: %s\n", escapedErrorMessage);
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "update %s set errorMessage='%s' where id=%d", 
    table, escapedErrorMessage, id);
sqlUpdate(conn, query->string);
dyStringFree(&query);
noWarnAbort();
}

void handleSubmissionError(struct sqlConnection *conn, int submitId, char *err)
/* Write out error to stderr and also save it in errorMessage field of submission table. */
{
writeErrToTableAndDie(conn, "bdwSubmission", submitId, err);
}

void handleFileError(struct sqlConnection *conn, int fileId, char *err)
/* Write out error to stderr and also save it in errorMessage field of file table. */
{
writeErrToTableAndDie(conn, "bdwFile", fileId, err);
}


#define maxPlateSize 16

void fetchFdNoCheck(struct sqlConnection *conn, unsigned bdwFileId, int unixFd, 
    char licensePlate[maxPlateSize], char bdwFile[PATH_LEN], char serverPath[PATH_LEN])
/* Fetch a file from the remote place we have a connection to.  Do not check MD5sum. 
 * Do clean up file if there is an error part way through data transfer*/
{
/* Figure out bdw file name, starting with license plate. */
makeLicensePlate(licensePlatePrefix, bdwFileId, licensePlate, maxPlateSize);
uglyf("fetchFdNoCheck: unixFd %d, bdwFileId %d, licensePlate %s\n", unixFd, bdwFileId, licensePlate);
/* Figure out directory and make any components not already there. */
time_t now = time(NULL);
char bdwDir[PATH_LEN];
bdwRelDirForTime(now, bdwDir);
char uploadDir[PATH_LEN];
safef(uploadDir, sizeof(uploadDir), "%s%s", bdwRootDir, bdwDir);
makeDirsOnPath(uploadDir);

/* Figure out full file name */
safef(bdwFile, PATH_LEN, "%s%s", bdwDir, licensePlate);
safef(serverPath, PATH_LEN, "%s%s", bdwRootDir, bdwFile);

int destFd = mustOpenFd(serverPath, O_CREAT|O_WRONLY);
cpFile(unixFd, destFd);
mustCloseFd(&destFd);
}

void bdwFileFetch(struct sqlConnection *conn, struct bdwFile *bf, int fd, 
	char *submitFileName, unsigned submissionId)
/* Fetch file and if successful update a bunch of the fields in bf with the result. */
{
bf->id = makeNewEmptyFileRecord(conn, submissionId, bf->submitFileName);
makeLicensePlate(licensePlatePrefix, bf->id, bf->licensePlate, sizeof(bf->licensePlate));

/* Wrap getting the file, the actual data transfer, with an error catcher that
 * will remove partly uploaded files.  Perhaps some day we'll attempt to rescue
 * ones that are just truncated by downloading the rest,  but not now. */
char bdwFile[PATH_LEN] = "", bdwPath[PATH_LEN];
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    bf->startUploadTime = time(NULL);
    fetchFdNoCheck(conn, bf->id, fd, bf->licensePlate, bdwFile, bdwPath);
    bf->endUploadTime = time(NULL);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    /* Attempt to remove any partial file. */
    if (bdwFile[0] != 0)
        {
	char path[PATH_LEN];
	safef(path, sizeof(path), "%s%s", bdwRootDir, bdwFile);
	remove(path);
	}
    handleSubmissionError(conn, submissionId, errCatch->message->string);
    }
errCatchFree(&errCatch);

/* Now we got the file.  We'll go ahead and save the file name and stuff. */
char query[256];
safef(query, sizeof(query),
       "update bdwFile"
       "  set licensePlate='%s', bdwFileName='%s', startUploadTime=%lld, endUploadTime=%lld"
       "  where id = %d"
       , bf->licensePlate, bdwFile, bf->startUploadTime, bf->endUploadTime, bf->id);
uglyf("query: %s\n", query);
sqlUpdate(conn, query);

/* Wrap the validations in an error catcher that will save error to file table in database */
errCatch = errCatchNew();
boolean success = FALSE;
if (errCatchStart(errCatch))
    {
    /* Check MD5 sum here.  */
    uglyf("bf->md5 = %s\n", bf->md5);
    unsigned char md5bin[16];
    md5ForFile(bdwPath, md5bin);
    char md5[33];
    hexBinaryString(md5bin, sizeof(md5bin), md5, sizeof(md5));
    uglyf("    md5=%s\n", md5);
    if (!sameWord(md5, bf->md5))
        errAbort("%s corrupted in upload md5 %s != %s\n", bf->submitFileName, bf->md5, md5);

    /* Finish updating a bunch more of bdwFile record. */
    struct dyString *dy = dyStringNew(0);  /* Includes tag so query may be long */
    dyStringPrintf(dy, "update bdwFile set md5='%s',size=%lld,updateTime=%lld",
	    md5, bf->size, bf->updateTime);
    dyStringAppend(dy, ", tags='");
    dyStringAppend(dy, bf->tags);
    dyStringPrintf(dy, "' where id=%d", bf->id);
    sqlUpdate(conn, dy->string);
    dyStringFree(&dy);
    success = TRUE;
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    handleFileError(conn, bf->id, errCatch->message->string);
    }
}

long bdwGotFile(struct sqlConnection *conn, char *submissionDir, char *submitFileName, char *md5)
/* See if we already got file.  Return fileId if we do,  otherwise -1 */
{
char query[PATH_LEN+512];
safef(query, sizeof(query), "select id from bdwSubmissionDir where url='%s'", submissionDir);
int submissionDirId = sqlQuickNum(conn, query);
if (submissionDirId <= 0)
    return -1;

safef(query, sizeof(query), 
    "select file.md5,file.id from bdwSubmission sub,bdwFile file "
    "where file.submitFileName='%s' and file.submissionId = sub.id and sub.submissionDirId = %d "
    "order by file.submissionId desc"
    , submitFileName, submissionDirId);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
long fileId = -1;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *dbMd5 = row[0];
    if (sameWord(md5, dbMd5))
        fileId = sqlLongLong(row[1]);
    else
        break;
    }
sqlFreeResult(&sr);

return fileId;
}

void bdwSubmit(char *submissionUrl, char *user, char *password)
/* bdwSubmit - Submit URL with validated.txt to warehouse. */
{
/* Parse out url a little into submissionDir and submissionFile */
char *lastSlash = strrchr(submissionUrl, '/');
if (lastSlash == NULL)
    errAbort("%s is not a valid URL - it has no '/' in it.", submissionUrl);
char *submissionFile = lastSlash+1;
int submissionDirSize = submissionFile - submissionUrl;
char submissionDir[submissionDirSize+1];
memcpy(submissionDir, submissionUrl, submissionDirSize);
submissionDir[submissionDirSize] = 0;  // Add trailing zero
uglyf("%s -> %s %s\n", submissionUrl, submissionDir, submissionFile);

/* Make sure user has access. */
struct sqlConnection *conn = sqlConnect(bdwDatabase);
char userSid[BDW_ACCESS_SIZE];
bdwMustHaveAccess(conn, user, password, userSid);

/* Make a submission record. */
int submissionId = makeNewEmptySubmissionRecord(conn, submissionUrl, userSid);
uglyf("submissionId = %d\n", submissionId);

/* Start catching errors from here and writing them in submissionId.  If we don't
 * throw we'll end up having a list of all files in the submission in bfList. */
struct bdwFile *bfList = NULL; 
struct errCatch *errCatch = errCatchNew();
char query[256];
if (errCatchStart(errCatch))
    {
    int hostId=0, submissionDirId = 0;
    int fd = bdwOpenAndRecord(conn, submissionDir, submissionFile, submissionUrl, 
	&hostId, &submissionDirId);
    uglyf("Got fd=%d from bdwOpenAndRecord\n", fd);

    /* Copy over manifest file */
    int fileId = makeNewEmptyFileRecord(conn, submissionId, submissionFile);
    uglyf("Got fileId = %d\n", fileId);

    /* Now make a file record around it. */
    char bdwFile[PATH_LEN];
    char bdwPath[PATH_LEN];
    char licensePlate[maxPlateSize];
    fetchFdNoCheck(conn, fileId, fd, licensePlate, bdwFile, bdwPath);
    uglyf("bdwPath is %s\n", bdwPath);

    /* Fill in MD5sum and the like of submission file */
    unsigned char md5bin[16];
    md5ForFile(bdwPath, md5bin);
    char md5[33];
    hexBinaryString(md5bin, sizeof(md5bin), md5, sizeof(md5));
    uglyf("md5 = %s\n", md5);
    time_t updateTime = fileModTime(bdwPath);
    off_t size = fileSize(bdwPath);
    safef(query, sizeof(query), 
	"update bdwFile set "
	" updateTime=%lld, size=%lld, md5='%s', licensePlate='%s', bdwFileName='%s'"
	" where id=%u\n",
	(long long)updateTime, (long long)size, md5, licensePlate, bdwFile, fileId);
    sqlUpdate(conn, query);

    /* Load up submission file as fielded table, make sure all required fields are there,
     * and calculate indexes of required fields. */
    char *requiredFields[] = {"file_name", "md5_sum", "size", "modified"};
    struct fieldedTable *table = fieldedTableFromTabFile(bdwPath, 
	    requiredFields, ArraySize(requiredFields));
    int fileIx = stringArrayIx("file_name", table->fields, table->fieldCount);
    int md5Ix = stringArrayIx("md5_sum", table->fields, table->fieldCount);
    int sizeIx = stringArrayIx("size", table->fields, table->fieldCount);
    int modifiedIx = stringArrayIx("modified", table->fields, table->fieldCount);

    /* Convert submission file to a bdwFile list with a lot of missing fields. */
    bfList = bdwFileFromFieldedTable(table, fileIx, md5Ix, sizeIx, modifiedIx);
    uglyf("Got %d files in bfList\n", slCount(bfList));

    /* Save our progress so far to submission table. */
    safef(query, sizeof(query), 
	"update bdwSubmission"
	"  set submitFileId=%lld, submissionDirId=%lld, fileCount=%d where id=%d",  
	    (long long)fileId, (long long)submissionDirId, slCount(bfList), submissionId);
    sqlUpdate(conn, query);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    handleSubmissionError(conn, submissionId, errCatch->message->string);
    /* The handleSubmissionError will keep on throwing. */
    }
errCatchFree(&errCatch);

/* Go through attempting to load the files if we don't already have them. */
struct bdwFile *bf;
for (bf = bfList; bf != NULL; bf = bf->next)
    {
    uglyf("Processing %s\n", bf->submitFileName);
    int submissionUrlSize = strlen(submissionDir) + strlen(bf->submitFileName) + 1;
    char submissionUrl[submissionUrlSize];
    safef(submissionUrl, submissionUrlSize, "%s%s", submissionDir, bf->submitFileName);
    if (bdwGotFile(conn, submissionDir, bf->submitFileName, bf->md5)<0)
	{
	int hostId=0, submissionDirId = 0;
	int fd = bdwOpenAndRecord(conn, submissionDir, bf->submitFileName, submissionUrl,
	    &hostId, &submissionDirId);
	bdwFileFetch(conn, bf, fd, submissionUrl, submissionId);
	close(fd);
	}
    }

/* If we made it here, update submission endUploadTime */
safef(query, sizeof(query),
	"update bdwSubmission set endUploadTime=%lld where id=%d", 
	(long long)time(NULL), submissionId);
sqlUpdate(conn, query);

#ifdef SOON
    struct bdwFile *bf;
    for (bf = bfList; bf != NULL; bf = bf->next)
	{
	bdwFileTabOut(bf, uglyOut); 
	}
#endif /* SOON */

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
licensePlatePrefix = optionVal("licensePlatePrefix", licensePlatePrefix);
if (argc != 4)
    usage();
bdwSubmit(argv[1], argv[2], argv[3]);
return 0;
}
