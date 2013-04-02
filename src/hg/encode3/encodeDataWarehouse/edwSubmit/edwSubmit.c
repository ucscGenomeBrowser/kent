/* edwSubmit - Submit URL with validated.txt to warehouse.. */
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
#include "encodeDataWarehouse.h"
#include "edwLib.h"

char *licensePlatePrefix = "ENCFF";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwSubmit - Submit URL with validated.txt to warehouse.\n"
  "usage:\n"
  "   edwSubmit submitUrl user password\n"
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

int edwGetHost(struct sqlConnection *conn, char *hostName)
/* Look up host name in table and return associated ID.  If not found
 * make up new table entry. */
{
/* If it's already in table, just return ID. */
char query[512];
safef(query, sizeof(query), "select id from edwHost where name='%s'", hostName);
int hostId = sqlQuickNum(conn, query);
if (hostId > 0)
    return hostId;

safef(query, sizeof(query), 
   "insert edwHost (name, lastOkTime, lastNotOkTime, firstAdded, errorMessage, uploadAttempts) "
   "values('%s', 0, 0, %lld, '', 0)", 
   hostName, (long long)time(NULL));
sqlUpdate(conn, query);
return sqlLastAutoId(conn);
}

int edwGetSubmitDir(struct sqlConnection *conn, int hostId, char *submitDir)
/* Get submitDir from database, creating it if it doesn't already exist. */
{
/* If it's already in table, just return ID. */
char query[512];
safef(query, sizeof(query), "select id from edwSubmitDir where url='%s'", submitDir);
int dirId = sqlQuickNum(conn, query);
if (dirId > 0)
    return dirId;

safef(query, sizeof(query), 
   "insert edwSubmitDir (url, firstAdded, hostId) values('%s', %lld, %d)", 
   submitDir, (long long)time(NULL), hostId);
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

int edwOpenAndRecordInDir(struct sqlConnection *conn, 
	char *submitDir, char *submitFile, char *url,
	int *retHostId, int *retDirId)
/* Return a low level read socket handle on URL if possible.  Consult and
 * update the edwHost and edwDir tables to help log and troubleshoot remote
 * problems. The url parameter should be just a concatenation of submitDir and
 * submitFile. */
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

/* Record success of open attempt in host and submitDir tables. */
int hostId = edwGetHost(conn, npu.host);
recordIntoHistory(conn, hostId, "edwHost", success);
int submitDirId = edwGetSubmitDir(conn, hostId, submitDir);
recordIntoHistory(conn, submitDirId, "edwSubmitDir", success);

/* Finish up error processing, bailing out of further processing if there was an error. */
errCatchFree(&errCatch);
if (!success)
    noWarnAbort();

/* Update optional return variables and return socket to read from. */
if (retHostId != NULL)
    *retHostId = hostId;
if (retDirId != NULL)
    *retDirId = submitDirId;
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


struct edwFile *edwFileFromFieldedTable(struct fieldedTable *table,
    int fileIx, int md5Ix, int sizeIx, int modifiedIx)
/* Turn parsed out table (still all just strings) into list of edwFiles. */
{
struct edwFile *bf, *bfList = NULL;
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
    bf->edwFileName  = cloneString("");
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

int makeNewEmptySubmitRecord(struct sqlConnection *conn, char *submitUrl, 
    char *userSid)
/* Create a submit record around URL and return it's id. */
{
struct dyString *query = dyStringNew(0);
dyStringAppend(query, 
    "insert edwSubmit "
    "(url, startUploadTime, endUploadTime, userSid, submitFileId, errorMessage) ");
dyStringPrintf(query, "VALUES('%s', %lld, %lld, '%s', 0, '')",
    submitUrl, (long long)time(NULL), (long long)0, userSid);
sqlUpdate(conn, query->string);
dyStringFree(&query);
return sqlLastAutoId(conn);
}

int makeNewEmptyFileRecord(struct sqlConnection *conn, unsigned submitId, char *submitFileName)
/* Make a new, largely empty, record around file and submit info. */
{
struct dyString *query = dyStringNew(0);
dyStringAppend(query, 
    "insert edwFile "
    "(submitId, submitFileName) ");
dyStringPrintf(query, "VALUES(%u, '%s')",
    submitId, submitFileName);
sqlUpdate(conn, query->string);
dyStringFree(&query);
return sqlLastAutoId(conn);
}

void edwRelDirForTime(time_t sinceEpoch, char dir[PATH_LEN])
/* Return the output directory for a given time. */
{
/* Get current time parsed into struct tm */
struct tm now;
gmtime_r(&sinceEpoch, &now);

/* make directory string out of year/month/day/ */
safef(dir, PATH_LEN, "%d/%d/%d/", now.tm_year+1900, now.tm_mon+1, now.tm_mday);
}

void writeErrToTableAndDie(struct sqlConnection *conn, char *table, int id, char *err)
/* Write out error to stderr and also save it in errorMessage field of submit table. */
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

void handleSubmitError(struct sqlConnection *conn, int submitId, char *err)
/* Write out error to stderr and also save it in errorMessage field of submit table. */
{
writeErrToTableAndDie(conn, "edwSubmit", submitId, err);
}

void handleFileError(struct sqlConnection *conn, int fileId, char *err)
/* Write out error to stderr and also save it in errorMessage field of file table. */
{
writeErrToTableAndDie(conn, "edwFile", fileId, err);
}

void fetchFdToTempFile(int remoteFd, char tempFileName[PATH_LEN])
/* This will fetch remote data to a temporary file. It fills in tempFileName with the name. */
{
/* First find out temp dir and make it. */
char tempDir[PATH_LEN];
safef(tempDir, sizeof(tempDir), "%s%s/", edwRootDir, "tmp");
makeDirsOnPath(tempDir);

/* Now make temp file name with XXXXXX name at end */
safef(tempFileName, PATH_LEN, "%sedwSubmitXXXXXX", tempDir);

/* Get open file handle. */
int localFd = mkstemp(tempFileName);
uglyf("tempFileName %s\n", tempFileName);
cpFile(remoteFd, localFd);
mustCloseFd(&localFd);
}

#define maxPlateSize 16

void makePlateFileNameAndPath(int edwFileId, char licensePlate[maxPlateSize],
    char edwFile[PATH_LEN], char serverPath[PATH_LEN])
/* Convert file id to local file name, and full file path. Make any directories needed
 * along serverPath. */
{
/* Figure out edw file name, starting with license plate. */
encode3MakeLicensePlate(licensePlatePrefix, edwFileId, licensePlate, maxPlateSize);

/* Figure out directory and make any components not already there. */
time_t now = time(NULL);
char edwDir[PATH_LEN];
edwRelDirForTime(now, edwDir);
char uploadDir[PATH_LEN];
safef(uploadDir, sizeof(uploadDir), "%s%s", edwRootDir, edwDir);
makeDirsOnPath(uploadDir);

/* Figure out full file names */
safef(edwFile, PATH_LEN, "%s%s", edwDir, licensePlate);
safef(serverPath, PATH_LEN, "%s%s", edwRootDir, edwFile);
}

void fetchFdNoCheck(unsigned edwFileId, int remoteFd, 
    char licensePlate[maxPlateSize], char edwFile[PATH_LEN], char serverPath[PATH_LEN])
/* Fetch a file from the remote place we have a connection to.  Do not check MD5sum. 
 * Do clean up file if there is an error part way through data transfer*/
{
makePlateFileNameAndPath(edwFileId, licensePlate, edwFile, serverPath);

/* Do remote copy to temp file - not to actual file because who knows,
 * it might get truncated in transit. Then rename. */
char tempName[PATH_LEN];
fetchFdToTempFile(remoteFd, tempName);
rename(tempName, serverPath);
}

void edwFileFetch(struct sqlConnection *conn, struct edwFile *bf, int fd, 
	char *submitFileName, unsigned submitId)
/* Fetch file and if successful update a bunch of the fields in bf with the result. */
{
bf->id = makeNewEmptyFileRecord(conn, submitId, bf->submitFileName);
encode3MakeLicensePlate(licensePlatePrefix, bf->id, bf->licensePlate, sizeof(bf->licensePlate));

/* Wrap getting the file, the actual data transfer, with an error catcher that
 * will remove partly uploaded files.  Perhaps some day we'll attempt to rescue
 * ones that are just truncated by downloading the rest,  but not now. */
char edwFile[PATH_LEN] = "", edwPath[PATH_LEN];
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    bf->startUploadTime = time(NULL);
    fetchFdNoCheck(bf->id, fd, bf->licensePlate, edwFile, edwPath);
    bf->endUploadTime = time(NULL);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    /* Attempt to remove any partial file. */
    if (edwFile[0] != 0)
        {
	char path[PATH_LEN];
	safef(path, sizeof(path), "%s%s", edwRootDir, edwFile);
	remove(path);
	}
    handleSubmitError(conn, submitId, errCatch->message->string);
    }
errCatchFree(&errCatch);

/* Now we got the file.  We'll go ahead and save the file name and stuff. */
char query[256];
safef(query, sizeof(query),
       "update edwFile"
       "  set licensePlate='%s', edwFileName='%s', startUploadTime=%lld, endUploadTime=%lld"
       "  where id = %d"
       , bf->licensePlate, edwFile, bf->startUploadTime, bf->endUploadTime, bf->id);
sqlUpdate(conn, query);

/* Wrap the validations in an error catcher that will save error to file table in database */
errCatch = errCatchNew();
boolean success = FALSE;
if (errCatchStart(errCatch))
    {
    /* Check MD5 sum here.  */
    unsigned char md5bin[16];
    md5ForFile(edwPath, md5bin);
    char md5[33];
    hexBinaryString(md5bin, sizeof(md5bin), md5, sizeof(md5));
    if (!sameWord(md5, bf->md5))
        errAbort("%s corrupted in upload md5 %s != %s\n", bf->submitFileName, bf->md5, md5);

    /* Finish updating a bunch more of edwFile record. */
    struct dyString *dy = dyStringNew(0);  /* Includes tag so query may be long */
    dyStringPrintf(dy, "update edwFile set md5='%s',size=%lld,updateTime=%lld",
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

long edwGotFile(struct sqlConnection *conn, char *submitDir, char *submitFileName, char *md5)
/* See if we already got file.  Return fileId if we do,  otherwise -1 */
{
char query[PATH_LEN+512];
safef(query, sizeof(query), "select id from edwSubmitDir where url='%s'", submitDir);
int submitDirId = sqlQuickNum(conn, query);
if (submitDirId <= 0)
    return -1;

safef(query, sizeof(query), 
    "select file.md5,file.id from edwSubmit sub,edwFile file "
    "where file.submitFileName='%s' and file.submitId = sub.id and sub.submitDirId = %d "
    "order by file.submitId desc"
    , submitFileName, submitDirId);
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

int findFileGivenMd5AndSubmitDir(struct sqlConnection *conn, char *md5, int submitDirId)
/* Given hexed md5 and a submitDir see if we have a matching file. */
{
char query[256];
safef(query, sizeof(query), 
    "select file.id from edwSubmit sub,edwFile file "
    "where file.md5 = '%s' and file.submitId = sub.id and sub.submitDirId = %d"
    , md5, submitDirId);
return sqlQuickNum(conn, query);
}

void edwSubmit(char *submitUrl, char *user, char *password)
/* edwSubmit - Submit URL with validated.txt to warehouse. */
{
/* Parse out url a little into submitDir and submitFile */
char *lastSlash = strrchr(submitUrl, '/');
if (lastSlash == NULL)
    errAbort("%s is not a valid URL - it has no '/' in it.", submitUrl);
char *submitFile = lastSlash+1;
int submitDirSize = submitFile - submitUrl;
char submitDir[submitDirSize+1];
memcpy(submitDir, submitUrl, submitDirSize);
submitDir[submitDirSize] = 0;  // Add trailing zero

/* Make sure user has access. */
struct sqlConnection *conn = sqlConnect(edwDatabase);
char userSid[EDW_ACCESS_SIZE];
edwMustHaveAccess(conn, user, password, userSid);

/* Make a submit record. */
int submitId = makeNewEmptySubmitRecord(conn, submitUrl, userSid);

/* Start catching errors from here and writing them in submitId.  If we don't
 * throw we'll end up having a list of all files in the submit in bfList. */
struct edwFile *bfList = NULL; 
struct errCatch *errCatch = errCatchNew();
char query[256];
if (errCatchStart(errCatch))
    {
    /* Open remote file.  This is most likely where we will fail. */
    int hostId=0, submitDirId = 0;
    long long startUploadTime = time(NULL);
    int remoteFd = edwOpenAndRecordInDir(conn, submitDir, submitFile, submitUrl, 
	&hostId, &submitDirId);

    /* Copy to local temp file. */
    char tempSubmitFile[PATH_LEN];
    fetchFdToTempFile(remoteFd, tempSubmitFile);
    mustCloseFd(&remoteFd);
    long long endUploadTime = time(NULL);

    /* Calculate MD5 sum, and see if we already have such a file. */
    char *md5 = md5HexForFile(tempSubmitFile);
    int fileId = findFileGivenMd5AndSubmitDir(conn, md5, submitDirId);
    uglyf("fileId for %s %s is %d\n", submitFile, md5, fileId);

    /* If we already have it, then delete temp file, otherwise put file in file table. */
    char submitLocalPath[PATH_LEN];
    if (fileId != 0)
	{
	remove(tempSubmitFile);
	char submitRelativePath[PATH_LEN];
	safef(query, sizeof(query), "select edwFileName from edwFile where id=%d", fileId);
	sqlNeedQuickQuery(conn, query, submitRelativePath, sizeof(submitRelativePath));
	safef(submitLocalPath, sizeof(submitLocalPath), "%s%s", edwRootDir, submitRelativePath);
	}
    else
        {
	/* Make nearly empty record - reserves fileId */
	fileId = makeNewEmptyFileRecord(conn, submitId, submitFile);

	/* Get license plate and file/path names that depend on it. */
	char licensePlate[maxPlateSize];
	char edwFile[PATH_LEN];
	makePlateFileNameAndPath(fileId, licensePlate, edwFile, submitLocalPath);

	/* Move file to final resting place and get update time and size from local file system.  */
	rename(tempSubmitFile, submitLocalPath);
	time_t updateTime = fileModTime(submitLocalPath);
	off_t size = fileSize(submitLocalPath);

	/* Update file table which now should be complete. */
	safef(query, sizeof(query), 
	    "update edwFile set "
	    " updateTime=%lld, size=%lld, md5='%s', licensePlate='%s', edwFileName='%s',"
	    " startUploadTime=%lld, endUploadTime=%lld"
	    " where id=%u\n",
	    (long long)updateTime, (long long)size, md5, licensePlate, edwFile, 
	    startUploadTime, endUploadTime, fileId);
	sqlUpdate(conn, query);
	}

    /* Load up submit file as fielded table, make sure all required fields are there,
     * and calculate indexes of required fields. */
    char *requiredFields[] = {"file_name", "md5_sum", "size", "modified"};
    struct fieldedTable *table = fieldedTableFromTabFile(submitLocalPath, 
	    requiredFields, ArraySize(requiredFields));
    int fileIx = stringArrayIx("file_name", table->fields, table->fieldCount);
    int md5Ix = stringArrayIx("md5_sum", table->fields, table->fieldCount);
    int sizeIx = stringArrayIx("size", table->fields, table->fieldCount);
    int modifiedIx = stringArrayIx("modified", table->fields, table->fieldCount);

    /* Convert submit file to a edwFile list with a lot of missing fields. */
    bfList = edwFileFromFieldedTable(table, fileIx, md5Ix, sizeIx, modifiedIx);

    /* Save our progress so far to submit table. */
    safef(query, sizeof(query), 
	"update edwSubmit"
	"  set submitFileId=%lld, submitDirId=%lld, fileCount=%d where id=%d",  
	    (long long)fileId, (long long)submitDirId, slCount(bfList), submitId);
    sqlUpdate(conn, query);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    handleSubmitError(conn, submitId, errCatch->message->string);
    /* The handleSubmitError will keep on throwing. */
    }
errCatchFree(&errCatch);

/* If we made it here the validated submit file itself got transfered and parses out
 * correctly.   */

/* Go through list attempting to load the files if we don't already have them. */
struct edwFile *bf;
for (bf = bfList; bf != NULL; bf = bf->next)
    {
    int submitUrlSize = strlen(submitDir) + strlen(bf->submitFileName) + 1;
    char submitUrl[submitUrlSize];
    safef(submitUrl, submitUrlSize, "%s%s", submitDir, bf->submitFileName);
    if (edwGotFile(conn, submitDir, bf->submitFileName, bf->md5)<0)
	{
	verbose(1, "Fetching %s\n", bf->submitFileName);
	int hostId=0, submitDirId = 0;
	int fd = edwOpenAndRecordInDir(conn, submitDir, bf->submitFileName, submitUrl,
	    &hostId, &submitDirId);
	edwFileFetch(conn, bf, fd, submitUrl, submitId);
	close(fd);
	}
    else
	verbose(2, "Already got %s\n", bf->submitFileName);
    }

/* If we made it here, update submit endUploadTime */
safef(query, sizeof(query),
	"update edwSubmit set endUploadTime=%lld where id=%d", 
	(long long)time(NULL), submitId);
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
edwSubmit(argv[1], argv[2], argv[3]);
return 0;
}
