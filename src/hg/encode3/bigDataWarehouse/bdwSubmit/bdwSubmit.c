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
#include "fieldedTable.h"
#include "bigDataWarehouse.h"
#include "bdwLib.h"

char *licensePlatePrefix = "ENCFF";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bdwSubmit - Submit URL with validated.txt to warehouse.\n"
  "usage:\n"
  "   bdwSubmit submitUrl user password\n"
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

int bdwGetHost(struct sqlConnection *conn, char *hostName)
/* Look up host name in table and return associated ID.  If not found
 * make up new table entry. */
{
/* If it's already in table, just return ID. */
char query[512];
safef(query, sizeof(query), "select id from bdwHost where name='%s'", hostName);
int hostId = sqlQuickNum(conn, query);
if (hostId > 0)
    return hostId;

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
    return dirId;

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

int bdwOpenAndRecordInDir(struct sqlConnection *conn, 
	char *submissionDir, char *submissionFile, char *url,
	int *retHostId, int *retDirId)
/* Return a low level read socket handle on URL if possible.  Consult and
 * update the bdwHost and bdwDir tables to help log and troubleshoot remote
 * problems. The url parameter should be just a concatenation of submissionDir and
 * submissionFile. */
{
/* Wrap routine to open network file in errCatch and remember whether it works. */
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

/* Record success of open attempt in host and submissionDir tables. */
int hostId = bdwGetHost(conn, npu.host);
recordIntoHistory(conn, hostId, "bdwHost", success);
int submissionDirId = bdwGetSubmissionDir(conn, hostId, submissionDir);
recordIntoHistory(conn, submissionDirId, "bdwSubmissionDir", success);

/* Finish up error processing, bailing out of further processing if there was an error. */
errCatchFree(&errCatch);
if (!success)
    noWarnAbort();

/* Update optional return variables and return socket to read from. */
if (retHostId != NULL)
    *retHostId = hostId;
if (retDirId != NULL)
    *retDirId = submissionDirId;
return sd;
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

void encode3MakeLicensePlate(char *prefix, int ix, char *out, int outSize)
/* Make a license-plate type string composed of prefix + funky coding of ix
 * and put result in out. */
{
int maxIx = 10*10*10*26*26*26;
if (ix > maxIx)
    errAbort("ix exceeds max in encode3MakeLicensePlate.  ix %d, max %d\n", ix, maxIx);
int prefixSize = strlen(prefix);
int minSize = prefixSize + 6 + 1;
if (outSize < minSize)
    errAbort("outSize (%d) not big enough in encode3MakeLicensePlate", outSize);

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


void fetchFdToTempFile(int remoteFd, char tempFileName[PATH_LEN])
/* This will fetch remote data to a temporary file. It fills in tempFileName with the name. */
{
/* First find out temp dir and make it. */
char tempDir[PATH_LEN];
safef(tempDir, sizeof(tempDir), "%s%s/", bdwRootDir, "tmp");
makeDirsOnPath(tempDir);

/* Now make temp file name with XXXXXX name at end */
safef(tempFileName, PATH_LEN, "%sbdwSubmitXXXXXX", tempDir);

/* Get open file handle. */
int localFd = mkstemp(tempFileName);
cpFile(remoteFd, localFd);
mustCloseFd(&localFd);
}

void fetchFdNoCheck(struct sqlConnection *conn, unsigned bdwFileId, int remoteFd, 
    char licensePlate[maxPlateSize], char bdwFile[PATH_LEN], char serverPath[PATH_LEN])
/* Fetch a file from the remote place we have a connection to.  Do not check MD5sum. 
 * Do clean up file if there is an error part way through data transfer*/
{
/* Figure out bdw file name, starting with license plate. */
encode3MakeLicensePlate(licensePlatePrefix, bdwFileId, licensePlate, maxPlateSize);
/* Figure out directory and make any components not already there. */
time_t now = time(NULL);
char bdwDir[PATH_LEN];
bdwRelDirForTime(now, bdwDir);
char uploadDir[PATH_LEN];
safef(uploadDir, sizeof(uploadDir), "%s%s", bdwRootDir, bdwDir);
makeDirsOnPath(uploadDir);

/* Figure out full file names */
safef(bdwFile, PATH_LEN, "%s%s", bdwDir, licensePlate);
safef(serverPath, PATH_LEN, "%s%s", bdwRootDir, bdwFile);

/* Do remote copy to temp file - not to actual file because who knows,
 * it might get truncated in transit. Then rename. */
char tempName[PATH_LEN];
fetchFdToTempFile(remoteFd, tempName);
rename(tempName, serverPath);
}

void bdwFileFetch(struct sqlConnection *conn, struct bdwFile *bf, int fd, 
	char *submitFileName, unsigned submissionId)
/* Fetch file and if successful update a bunch of the fields in bf with the result. */
{
bf->id = makeNewEmptyFileRecord(conn, submissionId, bf->submitFileName);
encode3MakeLicensePlate(licensePlatePrefix, bf->id, bf->licensePlate, sizeof(bf->licensePlate));

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
sqlUpdate(conn, query);

/* Wrap the validations in an error catcher that will save error to file table in database */
errCatch = errCatchNew();
boolean success = FALSE;
if (errCatchStart(errCatch))
    {
    /* Check MD5 sum here.  */
    unsigned char md5bin[16];
    md5ForFile(bdwPath, md5bin);
    char md5[33];
    hexBinaryString(md5bin, sizeof(md5bin), md5, sizeof(md5));
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

/* Make sure user has access. */
struct sqlConnection *conn = sqlConnect(bdwDatabase);
char userSid[BDW_ACCESS_SIZE];
bdwMustHaveAccess(conn, user, password, userSid);

/* Make a submission record. */
int submissionId = makeNewEmptySubmissionRecord(conn, submissionUrl, userSid);

/* Start catching errors from here and writing them in submissionId.  If we don't
 * throw we'll end up having a list of all files in the submission in bfList. */
struct bdwFile *bfList = NULL; 
struct errCatch *errCatch = errCatchNew();
char query[256];
if (errCatchStart(errCatch))
    {
    int hostId=0, submissionDirId = 0;
    int fd = bdwOpenAndRecordInDir(conn, submissionDir, submissionFile, submissionUrl, 
	&hostId, &submissionDirId);

    /* Make empty record in db so as to reserve fileId */
    int fileId = makeNewEmptyFileRecord(conn, submissionId, submissionFile);

    /* Fetch file.  Record where you put it in licensePlate, bdwFile, and bdwPath*/
    char licensePlate[maxPlateSize];
    char bdwFile[PATH_LEN];
    char bdwPath[PATH_LEN];
    long long startUploadTime = time(NULL);
    fetchFdNoCheck(conn, fileId, fd, licensePlate, bdwFile, bdwPath);
    long long endUploadTime = time(NULL);

    /* Fill in md5, updateTime and size */
    unsigned char md5bin[16];
    md5ForFile(bdwPath, md5bin);
    char md5[33];
    hexBinaryString(md5bin, sizeof(md5bin), md5, sizeof(md5));
    time_t updateTime = fileModTime(bdwPath);
    off_t size = fileSize(bdwPath);

    /* Update database with what we know so far of the submission file. */
    safef(query, sizeof(query), 
	"update bdwFile set "
	" updateTime=%lld, size=%lld, md5='%s', licensePlate='%s', bdwFileName='%s',"
	" startUploadTime=%lld, endUploadTime=%lld"
	" where id=%u\n",
	(long long)updateTime, (long long)size, md5, licensePlate, bdwFile, 
	startUploadTime, endUploadTime, fileId);
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

/* If we made it here the validated submission file itself got transfered and parses out
 * correctly.   */

/* Go through list attempting to load the files if we don't already have them. */
struct bdwFile *bf;
for (bf = bfList; bf != NULL; bf = bf->next)
    {
    int submissionUrlSize = strlen(submissionDir) + strlen(bf->submitFileName) + 1;
    char submissionUrl[submissionUrlSize];
    safef(submissionUrl, submissionUrlSize, "%s%s", submissionDir, bf->submitFileName);
    if (bdwGotFile(conn, submissionDir, bf->submitFileName, bf->md5)<0)
	{
	verbose(1, "Fetching %s\n", bf->submitFileName);
	int hostId=0, submissionDirId = 0;
	int fd = bdwOpenAndRecordInDir(conn, submissionDir, bf->submitFileName, submissionUrl,
	    &hostId, &submissionDirId);
	bdwFileFetch(conn, bf, fd, submissionUrl, submissionId);
	close(fd);
	}
    else
	verbose(2, "Already got %s\n", bf->submitFileName);
    }

/* If we made it here, update submission endUploadTime */
safef(query, sizeof(query),
	"update bdwSubmission set endUploadTime=%lld where id=%d", 
	(long long)time(NULL), submissionId);
sqlUpdate(conn, query);

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
