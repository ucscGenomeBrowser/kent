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
#include "encode3/encode3Valid.h"
#include "edwLib.h"

boolean doNow = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwSubmit - Submit URL with validated.txt to warehouse.\n"
  "usage:\n"
  "   edwSubmit submitUrl email-address\n"
  "options:\n"
  "   -now  If set, start submission now even though one seems to be in progress for same url.");
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"now", OPTION_BOOLEAN},
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
	    errAbort("Unrecognized ucsc_db %s - please arrange files so that the top " 
	             "level directory in the fileName in the manifest is a UCSC database name "
		     "like 'hg19' or 'mm9.'  Alternatively please include a ucsc_db column.",
		     ucscDbVal);

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
    char *submitFileName, long long size)
/* Make a new, largely empty, record around file and submit info. */
{
struct dyString *query = dyStringNew(0);
char *escapedFileName = sqlEscapeString(submitFileName);
dyStringAppend(query, "insert edwFile (submitId, submitDirId, submitFileName, size) ");
dyStringPrintf(query, "VALUES(%u, %u, '%s', %lld)", submitId, submitDirId, escapedFileName, size);
sqlUpdate(conn, query->string);
dyStringFree(&query);
freez(&escapedFileName);
return sqlLastAutoId(conn);
}

void handleSubmitError(struct sqlConnection *conn, int submitId, char *err)
/* Write out error to stderr and also save it in errorMessage field of submit table. */
{
edwWriteErrToStderrAndTable(conn, "edwSubmit", submitId, err);
noWarnAbort();
}

void handleFileError(struct sqlConnection *conn, int submitId, int fileId, char *err)
/* Write out error to stderr and also save it in errorMessage field of file 
 * and submit table. */
{
/* Write out error message to errorMessage field of table. */
warn("%s", trimSpaces(err));
edwWriteErrToTable(conn, "edwFile", fileId, err);
edwWriteErrToTable(conn, "edwSubmit", submitId, err);
noWarnAbort(err);
}

int mustMkstemp(char tempFileName[PATH_LEN])
/* Fill in temporary file name with name of a tmp file and return open file handle. 
 * Also set permissions to something better. */
{
int fd = mkstemp(tempFileName);
if (fd == -1)
    errnoAbort("Couldn't make temp file %s", tempFileName);
if (fchmod(fd, 0664) == -1)
    errnoAbort("Couldn't change permissions on temp file %s", tempFileName);
return fd;
}

void fetchFdToTempFile(int remoteFd, char tempFileName[PATH_LEN])
/* This will fetch remote data to a temporary file. It fills in tempFileName with the name. */
{
/* Now make temp file name with XXXXXX name at end */
safef(tempFileName, PATH_LEN, "%sedwSubmitXXXXXX", edwTempDir());

/* Get open file handle, copy file, and close. */
int localFd = mustMkstemp(tempFileName);
cpFile(remoteFd, localFd);
mustCloseFd(&localFd);
}

int edwFileFetch(struct sqlConnection *conn, struct edwFile *ef, int fd, 
	char *submitFileName, unsigned submitId, unsigned submitDirId)
/* Fetch file and if successful update a bunch of the fields in ef with the result. 
 * Returns fileId. */
{
ef->id = makeNewEmptyFileRecord(conn, submitId, submitDirId, ef->submitFileName, ef->size);

/* Wrap getting the file, the actual data transfer, with an error catcher that
 * will remove partly uploaded files.  Perhaps some day we'll attempt to rescue
 * ones that are just truncated by downloading the rest,  but not now. */
struct errCatch *errCatch = errCatchNew();
char tempName[PATH_LEN] = "";
char edwFile[PATH_LEN] = "", edwPath[PATH_LEN];
if (errCatchStart(errCatch))
    {
    /* Now make temp file name and open temp file in an atomic operation */
    char *tempDir = edwTempDir();
    safef(tempName, PATH_LEN, "%sedwSubmitXXXXXX", tempDir);
    int localFd = mustMkstemp(tempName);

    /* Update file name in database with temp file name so web app can track us. */
    char query[PATH_LEN+128];
    safef(query, sizeof(query), 
	"update edwFile set edwFileName='%s' where id=%lld", 
	tempName + strlen(edwRootDir), (long long)ef->id);
    sqlUpdate(conn, query);

    /* Do actual upload tracking how long it takes. */
    ef->startUploadTime = edwNow();
    cpFile(fd, localFd);
    mustCloseFd(&localFd);
    ef->endUploadTime = edwNow();

    /* Rename file both in file system and (via ef) database. */
    edwMakeFileNameAndPath(ef->id, submitFileName, edwFile, edwPath);
    uglyf("edwFile=%s, edwPath=%s\n", edwFile, edwPath);
    mustRename(tempName, edwPath);
    ef->edwFileName = cloneString(edwFile);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    /* Attempt to remove any partial file. */
    if (tempName[0] != 0)
	remove(tempName);
    handleSubmitError(conn, submitId, errCatch->message->string);  // Throws further
    assert(FALSE);  // We never get here
    }
errCatchFree(&errCatch);

/* Now we got the file.  We'll go ahead and save the file name and stuff. */
char query[256];
safef(query, sizeof(query),
       "update edwFile set"
       "  edwFileName='%s', startUploadTime=%lld, endUploadTime=%lld"
       "  where id = %d"
       , ef->edwFileName, ef->startUploadTime, ef->endUploadTime, ef->id);
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
    if (!sameWord(md5, ef->md5))
        errAbort("%s has md5 mismatch: %s != %s.  File may be corrupted in upload, or file may have been changed since validateManifest was run.  Please check that md5 of file before upload is really %s.  If it is then try submitting again,  otherwise rerun validateManifest and then try submitting again. \n", ef->submitFileName, ef->md5, md5, ef->md5);

    /* Finish updating a bunch more of edwFile record. Note there is a requirement in 
     * the validFile section that ef->updateTime be updated last.  A nonzero ef->updateTime
     * is used as a sign of record complete. */
    struct dyString *dy = dyStringNew(0);  /* Includes tag so query may be long */
    dyStringPrintf(dy, "update edwFile set md5='%s',size=%lld,updateTime=%lld",
	    md5, ef->size, ef->updateTime);
    dyStringAppend(dy, ", tags='");
    dyStringAppend(dy, ef->tags);
    dyStringPrintf(dy, "' where id=%d", ef->id);
    sqlUpdate(conn, dy->string);
    dyStringFree(&dy);
    success = TRUE;
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    handleFileError(conn, submitId, ef->id, errCatch->message->string);
    }
return ef->id;
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

static void allGoodFileNameChars(char *fileName)
/* Return TRUE if all chars are good for a file name */
{
char c, *s = fileName;
while ((c = *s++) != 0)
    if (!isalnum(c) && c != '_' && c != '-' && c != '.' && c != '/' && c != '+') 
        errAbort("Character '%c' (binary %d) not allowed in fileName '%s'", c, (int)c, fileName);
}

static void allGoodSymbolChars(char *symbol)
/* Return TRUE if all chars are good for a basic symbol in a controlled vocab */
{
if (!sameString("n/a", symbol))
    {
    char c, *s = symbol;
    while ((c = *s++) != 0)
	if (!isalnum(c) && c != '_')
	    errAbort("Character '%c' not allowed in symbol '%s'", c, symbol);
    }
}

static boolean isExperimentId(char *experiment)
/* Return TRUE if it looks like an ENCODE experiment ID */
{
if (startsWith("wgEncode", experiment))
    return TRUE;
if (!startsWith("ENCSR", experiment))
    return FALSE;
if (!isdigit(experiment[5]) || !isdigit(experiment[6]) || !isdigit(experiment[7]))
    return FALSE;
if (!isupper(experiment[8]) || !isupper(experiment[9]) || !isupper(experiment[10]))
    return FALSE;
return TRUE;
}

boolean isAllNum(char *s)
/* Return TRUE if all characters are numeric (no leading - even) */
{
char c;
while ((c = *s++) != 0)
    if (!isdigit(c))
        return FALSE;
return TRUE;
}

boolean isAllHexLower(char *s)
/* Return TRUE if all chars are valid lower case hexadecimal. */
{
char c;
while ((c = *s++) != 0)
    if (!isdigit(c) && !(c >= 'a' && c <= 'f'))
        return FALSE;
return TRUE;
}
         
char *edwSupportedFormats[] = {"unknown", "fastq", "bam", "bed", "gtf", 
    "bigWig", "bigBed", "bedLogR", "bedRnaElements", "bedRrbs", "broadPeak", 
    "narrowPeak", "openChromCombinedPeaks", "peptideMapping", "shortFrags", };
int edwSupportedFormatsCount = ArraySize(edwSupportedFormats);

char *edwSupportedEnrichedIn[] = {"unknown", "exon", "intron", "promoter", "coding", 
    "utr", "utr3", "utr5", "open"};
int edwSupportedEnrichedInCount = ArraySize(edwSupportedEnrichedIn);

struct edwFile *edwParseSubmitFile(char *submitLocalPath, char *submitUrl)
/* Load and parse up this file as fielded table, make sure all required fields are there,
 * and calculate indexes of required fields.   This produces an edwFile list, but with
 * still quite a few fields missing - just what can be filled in from submit filled in. 
 * The submitUrl is just used for error reporting.  If it's local, just make it the
 * same as submitLocalPath. */
{
char *requiredFields[] = {"file_name", "format", "output_type", "experiment", "replicate", 
    "enriched_in", "md5_sum", "size",  "modified", "valid_key"};
struct fieldedTable *table = fieldedTableFromTabFile(submitLocalPath, submitUrl,
	requiredFields, ArraySize(requiredFields));

/* Get offsets of all required fields */
int fileIx = stringArrayIx("file_name", table->fields, table->fieldCount);
int formatIx = stringArrayIx("format", table->fields, table->fieldCount);
int outputIx = stringArrayIx("output_type", table->fields, table->fieldCount);
int experimentIx = stringArrayIx("experiment", table->fields, table->fieldCount);
int replicateIx = stringArrayIx("replicate", table->fields, table->fieldCount);
int enrichedIx = stringArrayIx("enriched_in", table->fields, table->fieldCount);
int md5Ix = stringArrayIx("md5_sum", table->fields, table->fieldCount);
int sizeIx = stringArrayIx("size", table->fields, table->fieldCount);
int modifiedIx = stringArrayIx("modified", table->fields, table->fieldCount);
int validIx = stringArrayIx("valid_key", table->fields, table->fieldCount);

/* Loop through and make sure all field values are ok */
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char **row = fr->row;
    char *fileName = row[fileIx];
    allGoodFileNameChars(fileName);
    char *format = row[formatIx];
    if (stringArrayIx(format, edwSupportedFormats, edwSupportedFormatsCount) < 0)
	errAbort("Format %s is not supported", format);
    allGoodSymbolChars(row[outputIx]);
    char *experiment = row[experimentIx];
    if (!isExperimentId(experiment))
        errAbort("%s in experiment field does not seem to be an encode experiment", experiment);
    char *replicate = row[replicateIx];
    if (differentString(replicate, "pooled") && differentString(replicate, "n/a") )
	if (!isAllNum(replicate))
	    errAbort("%s is not a good value for the replicate column", replicate);
    char *enriched = row[enrichedIx];
    if (stringArrayIx(enriched, edwSupportedEnrichedIn, edwSupportedEnrichedInCount) < 0)
        errAbort("Enriched_in %s is not supported", enriched);
    char *md5 = row[md5Ix];
    if (strlen(md5) != 32 || !isAllHexLower(md5))
        errAbort("md5 '%s' is not in all lower case 32 character hexadecimal format.", md5);
    char *size = row[sizeIx];
    if (!isAllNum(size))
        errAbort("Invalid size '%s'", size);
    char *modified = row[modifiedIx];
    if (!isAllNum(modified))
        errAbort("Invalid modification time '%s'", modified);
    char *validIn = row[validIx];
    char *realValid = encode3CalcValidationKey(md5, sqlLongLong(size));
    if (!sameString(validIn, realValid))
        errAbort("The valid_key %s for %s doesn't fit", validIn, fileName);
    freez(&realValid);
    }

return edwFileFromFieldedTable(table, fileIx, md5Ix, sizeIx, modifiedIx);
}

void notOverlappingSelf(struct sqlConnection *conn, char *url)
/* Ensure we are only submission going on for this URL, allowing for time out
 * and command line override. */
{
if (doNow) // Allow command line override
    return; 

/* Fetch most recent submission from this URL. */
struct edwSubmit *old = edwMostRecentSubmission(conn, url);
if (old == NULL)
    return;

/* See if we have something in progress, meaning started but not ended. */
if (old->endUploadTime == 0 && isEmpty(old->errorMessage))
    {
    /* Figure out when we started most recent single file in the upload, or when
     * we started if not files started yet. */
    char query[256];
    safef(query, sizeof(query), 
	"select max(startUploadTime) from edwFile where submitId=%u", old->id);
    long long maxStartTime = sqlQuickLongLong(conn, query);
    if (maxStartTime == 0)
	maxStartTime = old->startUploadTime;

    /* Check against our usual time out. */
    if (edwNow() - maxStartTime < edwSingleFileTimeout)
        errAbort("Submission of %s already is in progress.  Please come back in an hour", url);
    }

edwSubmitFree(&old);
}

static void getSubmittedFile(struct sqlConnection *conn, struct edwFile *bf,  
    char *submitDir, char *submitUrl, int submitId)
/* We know the submission, we know what the file is supposed to look like.  Fetch it.
 * If things go badly catch the error, attach it to the submission record, and then
 * keep throwing. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    int hostId=0, submitDirId = 0;
    int fd = edwOpenAndRecordInDir(conn, submitDir, bf->submitFileName, submitUrl,
	&hostId, &submitDirId);
    int fileId = edwFileFetch(conn, bf, fd, submitUrl, submitId, submitDirId);
    close(fd);
    edwAddQaJob(conn, fileId);
    tellSubscribers(conn, submitDir, bf->submitFileName, fileId);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    handleSubmitError(conn, submitId, errCatch->message->string);
    /* The handleSubmitError will keep on throwing. */
    }
errCatchFree(&errCatch);
}

void edwSubmit(char *submitUrl, char *email)
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
struct sqlConnection *conn = sqlConnectProfile(edwDatabase, edwDatabase);
struct edwUser *user = edwMustGetUserFromEmail(conn, email);
int userId = user->id;

/* See if we are already running on same submission.  If so council patience and quit. */
notOverlappingSelf(conn, submitUrl);

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
    uglyf("edwOpenAndRecordInDir(submitDir=%s, submitFile=%s, submitUrl=%s)\n", submitDir, submitFile, submitUrl);
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
    uglyf("tempSubmitFile=%s, fileId=%d, md5=%s\n", tempSubmitFile, fileId, md5);

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
	/* Looks like it's the first time we've seen this submission file, so
	 * save the file itself.  We'll get to the records inside the file in a bit. */
	fileId = makeNewEmptyFileRecord(conn, submitId, submitDirId, submitFile, 0);

	/* Get file/path names for submission file inside warehouse. */
	char edwFile[PATH_LEN];
	edwMakeFileNameAndPath(fileId, submitFile, edwFile, submitLocalPath);

	/* Move file to final resting place and get update time and size from local file system.  */
	mustRename(tempSubmitFile, submitLocalPath);
	time_t updateTime = fileModTime(submitLocalPath);
	off_t size = fileSize(submitLocalPath);

	/* Update file table which now should be complete including updateTime. */
	safef(query, sizeof(query), 
	    "update edwFile set "
	    " updateTime=%lld, size=%lld, md5='%s', edwFileName='%s',"
	    " startUploadTime=%lld, endUploadTime=%lld"
	    " where id=%u\n",
	    (long long)updateTime, (long long)size, md5, edwFile, 
	    startUploadTime, endUploadTime, fileId);
	sqlUpdate(conn, query);
	}

    /* By now there is a submit file on the local file system.  */

    uglyf("edwParseSubmitFile(%s, %s)\n", submitLocalPath, submitUrl);
    bfList = edwParseSubmitFile(submitLocalPath, submitUrl);

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


/* Weed out files we already have. */
int oldCount = 0;
long long oldBytes = 0, newBytes = 0, byteCount = 0;
struct edwFile *bf, *oldList = NULL, *newList = NULL, *bfNext;
for (bf = bfList; bf != NULL; bf = bfNext)
    {
    bfNext = bf->next;
    if (edwGotFile(conn, submitDir, bf->submitFileName, bf->md5) >= 0)
        {
	++oldCount;
	oldBytes += bf->size;
	slAddHead(&oldList, bf);
	}
    else
        slAddHead(&newList, bf);
    byteCount += bf->size;
    }
bfList = NULL;

/* Update database with oldFile count. */
safef(query, sizeof(query), 
    "update edwSubmit set oldFiles=%d,oldBytes=%lld,byteCount=%lld where id=%u",  
	oldCount, oldBytes, byteCount, submitId);
sqlUpdate(conn, query);

/* Go through list attempting to load the files if we don't already have them. */
for (bf = newList; bf != NULL; bf = bf->next)
    {
    int submitUrlSize = strlen(submitDir) + strlen(bf->submitFileName) + 1;
    char submitUrl[submitUrlSize];
    safef(submitUrl, submitUrlSize, "%s%s", submitDir, bf->submitFileName);
    if (edwGotFile(conn, submitDir, bf->submitFileName, bf->md5)<0)
	{
	/* We can't get a ID for this file. There's two possible reasons - 
	 * either somebody is in the middle of fetching it or nobody's started. 
	 * If somebody is in the middle of fetching it, assume they died
	 * if they took more than an hour,  and start up another fetch.
	 * So here we fetch unless somebody else is fetching recently. */
	if (edwGettingFile(conn, submitDir, bf->submitFileName) < 0)
	    {
	    verbose(1, "Fetching %s\n", bf->submitFileName);
	    getSubmittedFile(conn, bf, submitDir, submitUrl, submitId);
	    newBytes += bf->size;
	    safef(query, sizeof(query), 
		"update edwSubmit set newFiles=newFiles+1,newBytes=%lld where id=%d", 
		newBytes, submitId);
	    sqlUpdate(conn, query);
	    }

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
doNow = optionExists("now");
if (argc != 3)
    usage();
edwSubmit(argv[1], argv[2]);
return 0;
}
