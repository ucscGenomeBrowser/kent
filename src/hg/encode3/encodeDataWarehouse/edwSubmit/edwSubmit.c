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

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwSubmit - Submit URL with validated.txt to warehouse.\n"
  "usage:\n"
  "   edwSubmit submitUrl user password\n"
  "Generally user is an email address\n"
  "options:\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void recordIntoHistory(struct sqlConnection *conn, unsigned id, char *table, boolean success)
/* Record success/failure into uploadAttempts and historyBits fields of table.   */
{
/* Get historyBits and fold status into it. */
char quickResult[32];
char query[256];
safef(query, sizeof(query), "select historyBits from %s where id=%u", table, id);
if (sqlQuickQuery(conn, query, quickResult, sizeof(quickResult)) == NULL)
    internalErr();
char *lastTimeField;
char *openResultField;
long long historyBits = sqlLongLong(quickResult);
historyBits <<= 1;
if (success)
    {
    historyBits |= 1;
    lastTimeField = "lastOkTime";
    openResultField = "openSuccesses";
    }
else
    {
    lastTimeField = "lastNotOkTime";
    openResultField = "openFails";
    }

safef(query, sizeof(query), 
    "update %s set historyBits=%lld, %s=%s+1, %s=%lld "
    "where id=%lld",
    table, historyBits, openResultField, openResultField, lastTimeField, edwNow(),
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


struct edwFile *edwFileFromFieldedTable(struct fieldedTable *table,
    int fileIx, int md5Ix, int sizeIx, int modifiedIx)
/* Turn parsed out table (still all just strings) into list of edwFiles. */
{
struct edwFile *bf, *bfList = NULL;
struct fieldedRow *fr;
struct dyString *tags = dyStringNew(0);
char *ucscDbTag = "ucsc_db";
int ucscDbField = stringArrayIx(ucscDbTag, table->fields, table->fieldCount);
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
    if (ucscDbField < 0)
        {
	/* Try to make this field up from file name */
	char *slash = strchr(bf->submitFileName, '/');
	if (slash == NULL)
	    errAbort("Can't make up '%s' field from '%s'", ucscDbTag, bf->submitFileName);
	int len = slash - bf->submitFileName;
	char ucscDbVal[len+1];
	memcpy(ucscDbVal, bf->submitFileName, len);
	ucscDbVal[len] = 0;

	/* Do a little check on it */
	if (!sameString("mm9", ucscDbVal) && !sameString("hg19", ucscDbVal))
	    errAbort("Unrecognized ucsc_db %s", ucscDbVal);

	/* Add it to tags. */
	cgiEncodeIntoDy(ucscDbTag, ucscDbVal, tags);
	}
    bf->tags = cloneString(tags->string);

    /* Fake other fields. */
    bf->edwFileName  = cloneString("");
    slAddHead(&bfList, bf);
    }
slReverse(&bfList);
dyStringFree(&tags);
return bfList;
}

int makeNewEmptySubmitRecord(struct sqlConnection *conn, char *submitUrl, unsigned userId)
/* Create a submit record around URL and return it's id. */
{
char *escapedUrl = sqlEscapeString(submitUrl);
struct dyString *query = dyStringNew(0);
dyStringAppend(query, "insert edwSubmit (url, startUploadTime, userId) ");
dyStringPrintf(query, "VALUES('%s', %lld,  %d)", escapedUrl, edwNow(), userId);
sqlUpdate(conn, query->string);
dyStringFree(&query);
freez(&escapedUrl);
return sqlLastAutoId(conn);
}

int makeNewEmptyFileRecord(struct sqlConnection *conn, unsigned submitId, unsigned submitDirId,
    char *submitFileName)
/* Make a new, largely empty, record around file and submit info. */
{
struct dyString *query = dyStringNew(0);
char *escapedFileName = sqlEscapeString(submitFileName);
dyStringAppend(query, "insert edwFile (submitId, submitDirId, submitFileName) ");
dyStringPrintf(query, "VALUES(%u, %u, '%s')", submitId, submitDirId, escapedFileName);
sqlUpdate(conn, query->string);
dyStringFree(&query);
freez(&escapedFileName);
return sqlLastAutoId(conn);
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
/* Now make temp file name with XXXXXX name at end */
safef(tempFileName, PATH_LEN, "%sedwSubmitXXXXXX", edwTempDir());

/* Get open file handle. */
int localFd = mkstemp(tempFileName);
cpFile(remoteFd, localFd);
mustCloseFd(&localFd);
}

int edwFileFetch(struct sqlConnection *conn, struct edwFile *bf, int fd, 
	char *submitFileName, unsigned submitId, unsigned submitDirId)
/* Fetch file and if successful update a bunch of the fields in bf with the result. 
 * Returns fileId. */
{
bf->id = makeNewEmptyFileRecord(conn, submitId, submitDirId, bf->submitFileName);

/* Wrap getting the file, the actual data transfer, with an error catcher that
 * will remove partly uploaded files.  Perhaps some day we'll attempt to rescue
 * ones that are just truncated by downloading the rest,  but not now. */
char edwFile[PATH_LEN] = "", edwPath[PATH_LEN];
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    edwMakeFileNameAndPath(bf->id, submitFileName, edwFile, edwPath);
    bf->startUploadTime = edwNow();
    char tempName[PATH_LEN];
    fetchFdToTempFile(fd, tempName);
    mustRename(tempName, edwPath);
    bf->endUploadTime = edwNow();
    bf->edwFileName = cloneString(edwFile);
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
       "update edwFile set"
       "  edwFileName='%s', startUploadTime=%lld, endUploadTime=%lld"
       "  where id = %d"
       , bf->edwFileName, bf->startUploadTime, bf->endUploadTime, bf->id);
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

    /* Finish updating a bunch more of edwFile record. Note there is a requirement in 
     * the validFile section that bf->updateTime be update last. */
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
return bf->id;
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

boolean allTagsWildMatch(char *tagPattern, struct hash *tagHash)
/* Tag pattern is a cgi encoded list of tags with wild card values.  This routine returns TRUE
 * if every tag in tagPattern is also in tagHash,  and the value in tagHash is wildcard
 * compatible with tagPattern. */
{
boolean match = TRUE;
char *tagsString = cloneString(tagPattern);
struct cgiVar *pattern, *patternList=NULL;
struct hash *patternHash=NULL;

cgiParseInputAbort(tagsString, &patternHash, &patternList);
for (pattern = patternList; pattern != NULL; pattern = pattern->next)
    {
    struct cgiVar *cv = hashFindVal(tagHash, pattern->name);
    char *val = cv->val;
    if (val == NULL)
	{
        match = FALSE;
	break;
	}
    if (!wildMatch(pattern->val, val))
        {
	match = FALSE;
	break;
	}
    }

slFreeList(&patternList);
hashFree(&patternHash);
freez(&tagsString);
return match;
}


void tellSubscribers(struct sqlConnection *conn, char *submitDir, char *submitFileName, unsigned id)
/* Tell subscribers that match about file of given id */
{
char query[256];
safef(query, sizeof(query), "select tags from edwFile where id=%u", id);
char *tagsString = sqlQuickString(conn, query);
struct hash *tagHash=NULL;
struct cgiVar *tagList=NULL;
if (!isEmpty(tagsString))
    cgiParseInputAbort(tagsString, &tagHash, &tagList);


char **row;
struct sqlResult *sr = sqlGetResult(conn, "select * from edwSubscriber order by runOrder,id");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct edwSubscriber *subscriber = edwSubscriberLoad(row);
    if (wildMatch(subscriber->filePattern, submitFileName)
        && wildMatch(subscriber->dirPattern, submitDir))
	{
	/* Might have to check for tags match, which involves db load and a cgi vs. cgi compare */
	boolean tagsOk = TRUE;
	if (!isEmpty(subscriber->tagPattern))
	    {
	    if (tagHash == NULL)  // if we're nonempty they better be too
	        tagsOk = FALSE;
	    else
	        {
		if (!allTagsWildMatch(subscriber->tagPattern, tagHash))
		    tagsOk = FALSE;
		}
	    }
	if (tagsOk)
	    {
	    int maxNumSize=16;	// more than enough digits base ten.
	    int maxCommandSize = strlen(subscriber->onFileEndUpload) + maxNumSize + 1;
	    char command[maxCommandSize];
	    safef(command, sizeof(command), subscriber->onFileEndUpload, id);
	    verbose(2, "system(%s)\n", command);
	    int err = system(command);
	    if (err != 0)
	        warn("err %d from system(%s)\n", err, command);
	    }
	}
    edwSubscriberFree(&subscriber);
    }

sqlFreeResult(&sr);
freez(&tagsString);
slFreeList(&tagList);
hashFree(&tagHash);
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
int userId =  edwMustHaveAccess(conn, user, password);

/* Make a submit record. */
int submitId = makeNewEmptySubmitRecord(conn, submitUrl, userId);

/* Start catching errors from here and writing them in submitId.  If we don't
 * throw we'll end up having a list of all files in the submit in bfList. */
struct edwFile *bfList = NULL; 
struct errCatch *errCatch = errCatchNew();
char query[256];
if (errCatchStart(errCatch))
    {
    /* Open remote submission file.  This is most likely where we will fail. */
    int hostId=0, submitDirId = 0;
    long long startUploadTime = edwNow();
    int remoteFd = edwOpenAndRecordInDir(conn, submitDir, submitFile, submitUrl, 
	&hostId, &submitDirId);

    /* Copy to local temp file. */
    char tempSubmitFile[PATH_LEN];
    fetchFdToTempFile(remoteFd, tempSubmitFile);
    mustCloseFd(&remoteFd);
    long long endUploadTime = edwNow();

    /* Calculate MD5 sum, and see if we already have such a file. */
    char *md5 = md5HexForFile(tempSubmitFile);
    int fileId = findFileGivenMd5AndSubmitDir(conn, md5, submitDirId);

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
	fileId = makeNewEmptyFileRecord(conn, submitId, submitDirId, submitFile);

	/* Get file/path names inside warehouse. */
	char edwFile[PATH_LEN];
	edwMakeFileNameAndPath(fileId, submitFile, edwFile, submitLocalPath);

	/* Move file to final resting place and get update time and size from local file system.  */
	mustRename(tempSubmitFile, submitLocalPath);
	time_t updateTime = fileModTime(submitLocalPath);
	off_t size = fileSize(submitLocalPath);

	/* Update file table which now should be complete. */
	safef(query, sizeof(query), 
	    "update edwFile set "
	    " updateTime=%lld, size=%lld, md5='%s', edwFileName='%s',"
	    " startUploadTime=%lld, endUploadTime=%lld"
	    " where id=%u\n",
	    (long long)updateTime, (long long)size, md5, edwFile, 
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
	int fileId = edwFileFetch(conn, bf, fd, submitUrl, submitId, submitDirId);
	close(fd);
	safef(query, sizeof(query), "update edwSubmit set newFiles=newFiles+1 where id=%d", 
	    submitId);
	sqlUpdate(conn, query);
	tellSubscribers(conn, submitDir, bf->submitFileName, fileId);
	}
    else
	{
	verbose(2, "Already got %s\n", bf->submitFileName);
	safef(query, sizeof(query), "update edwSubmit set oldFiles=oldFiles+1 where id=%d", 
	    submitId);
	sqlUpdate(conn, query);
	}
    }

/* If we made it here, update submit endUploadTime */
safef(query, sizeof(query),
	"update edwSubmit set endUploadTime=%lld where id=%d", 
	edwNow(), submitId);
sqlUpdate(conn, query);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
edwSubmit(argv[1], argv[2], argv[3]);
return 0;
}
