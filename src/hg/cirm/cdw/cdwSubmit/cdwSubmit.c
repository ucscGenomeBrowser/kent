/* cdwSubmit - Submit URL with manifest and metadata to warehouse. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "net.h"
#include "options.h"
#include "errAbort.h"
#include "dystring.h"
#include "errCatch.h"
#include "sqlNum.h"
#include "cheapcgi.h"
#include "net.h"
#include "paraFetch.h"
#include "md5.h"
#include "portable.h"
#include "obscure.h"
#include "hex.h"
#include "fieldedTable.h"
#include "cdw.h"
#include "cdwValid.h"
#include "cdwLib.h"
#include "mailViaPipe.h"
#include "tagStorm.h"

boolean doUpdate = FALSE;
boolean doBelieve = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwSubmit - Submit URL with validated.txt to warehouse.\n"
  "usage:\n"
  "   cdwSubmit email /path/to/manifest.tab meta.tag\n"
  "options:\n"
  "   -believe If set, believe MD5 sums rather than checking them\n"
  "   -update  If set, will update metadata on file it already has. The default behavior is to\n"
  "            report an error if metadata doesn't match.\n"
  "   -md5=md5sum.txt Take list of file MD5s from output of md5sum command on list of files\n");
}

char *localPrefix = "local://localhost/";
struct hash *md5Hash;

/* Command line validation table. */
static struct optionSpec options[] = {
   {"update", OPTION_BOOLEAN},
   {"believe", OPTION_BOOLEAN},
   {"md5", OPTION_STRING},
   {NULL, 0},
};

void recordIntoHistory(struct sqlConnection *conn, unsigned id, char *table, boolean success)
/* Record success/failure into uploadAttempts and historyBits fields of table.   */
{
/* Get historyBits and fold status into it. */
char quickResult[32];
char query[256];
sqlSafef(query, sizeof(query), "select historyBits from %s where id=%u", table, id);
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

sqlSafef(query, sizeof(query), 
    "update %s set historyBits=%lld, %s=%s+1, %s=%lld "
    "where id=%lld",
    table, historyBits, openResultField, openResultField, lastTimeField, cdwNow(),
    (long long)id);
sqlUpdate(conn, query);
}


void parseLocalOrUrl(char *path, struct netParsedUrl *parsed)
/* Parse something that may or may not have a protocol:// prefix into
 * netParsedUrl struct.  If it doesn't have protocol:// add
 * local:// to it before parsing. */
{
char buf[PATH_LEN];
char *url = path;
if (stringIn("://", path) == NULL)
    {
    safef(buf, sizeof(buf), "%s%s", localPrefix, url);
    url = buf;
    }
netParseUrl(url, parsed);
}

int cdwOpenAndRecordInDir(struct sqlConnection *conn, 
	char *submitDir, char *submitFile, char *url,
	int *retHostId, int *retDirId)
/* Return a low level read socket handle on URL if possible.  Consult and
 * update the cdwHost and cdwDir tables to help log and troubleshoot remote
 * problems. The url parameter should be just a concatenation of submitDir and
 * submitFile. */
{
/* Wrap routine to open network file in errCatch and remember whether it works. */
struct errCatch *errCatch = errCatchNew();
int sd = -1;
boolean success = TRUE;
if (errCatchStart(errCatch))
    {
    if (startsWith( localPrefix, url))
	sd = mustOpenFd(submitFile, O_RDONLY);
    else
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
parseLocalOrUrl(url, &npu);
char urlDir[PATH_LEN], urlFileName[PATH_LEN], urlExtension[PATH_LEN];
splitPath(npu.file, urlDir, urlFileName, urlExtension);

/* Record success of open attempt in host and submitDir tables. */
int hostId = cdwGetHost(conn, npu.host);
recordIntoHistory(conn, hostId, "cdwHost", success);
int submitDirId = cdwGetSubmitDir(conn, hostId, submitDir);
recordIntoHistory(conn, submitDirId, "cdwSubmitDir", success);

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

int cdwFileIdForLicensePlate(struct sqlConnection *conn, char *licensePlate)
/* Return ID in cdwFile table corresponding to license plate */
{
char query[256];
sqlSafef(query, sizeof(query), "select fileId from cdwValidFile where licensePlate='%s'",
    licensePlate);
return sqlQuickNum(conn, query);
}

char *replacesTag = "replaces";	    /* Tag in manifest for replacement */
char *replaceReasonTag = "replace_reason";	/* Tag in manifest for replacement reason */

struct submitFileRow
/* Information about a new file or an updated file. */
    {
    struct submitFileRow *next;
    struct cdwFile *file;   /* The file */
    char *replaces;	    /* License plate of file it replaces or NULL */
    unsigned replacesFile;       /* File table id of file it replaces or 0 */
    char *replaceReason;   /* Reason for replacement or 0 */
    long long md5MatchFileId;   /* If nonzero, then MD5 sum matches on this existing file. */
    };

struct submitFileRow *submitFileRowFromFieldedTable(
    struct sqlConnection *conn, struct fieldedTable *table,
    int fileIx, int md5Ix, int replacesIx, int replaceReasonIx)
/* Turn parsed out table (still all just strings) into list of cdwFiles. */
{
struct submitFileRow *sfr, *sfrList = NULL;
struct cdwFile *bf;
struct fieldedRow *fr;
struct dyString *tags = dyStringNew(0);

for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char **row = fr->row;
    AllocVar(bf);
    bf->submitFileName = cloneString(row[fileIx]);
    if (md5Ix >= 0)
       {
       memcpy(bf->md5, row[md5Ix], 32);
       tolowers(bf->md5);
       }

    /* Add as tags any fields not included in fixed fields. */
    dyStringClear(tags);
    int i;
    for (i=0; i<table->fieldCount; ++i)
        {
	if (i != fileIx && i != md5Ix)
	    {
	    cgiEncodeIntoDy(table->fields[i], row[i], tags);
	    }
	}
    bf->tags = cloneString(tags->string);

    /* Fake other fields. */
    bf->cdwFileName  = cloneString("");

    /* Allocate wrapper structure */
    AllocVar(sfr);
    sfr->file = bf;

    /* fill in fields about replacement maybe */
    if (replacesIx != -1)
        {
	char *replacesAcc = row[replacesIx];
	char *reason = row[replaceReasonIx];
	int fileId = cdwFileIdForLicensePlate(conn, replacesAcc);
	if (fileId == 0)
	    errAbort("%s in %s column doesn't exist in warehouse", replacesAcc, replacesTag);
	sfr->replaces = cloneString(replacesAcc);
	sfr->replaceReason = cloneString(reason);
	sfr->replacesFile = fileId;
	}

    slAddHead(&sfrList, sfr);
    }
slReverse(&sfrList);
dyStringFree(&tags);
return sfrList;
}

int makeNewEmptySubmitRecord(struct sqlConnection *conn, char *submitUrl, unsigned userId)
/* Create a submit record around URL and return it's id. */
{
struct dyString *query = dyStringNew(0);
sqlDyStringAppend(query, "insert cdwSubmit (url, startUploadTime, userId) ");
sqlDyStringPrintf(query, "VALUES('%s', %lld,  %d)", submitUrl, cdwNow(), userId);
sqlUpdate(conn, query->string);
dyStringFree(&query);
return sqlLastAutoId(conn);
}

int makeNewEmptyFileRecord(struct sqlConnection *conn, unsigned userId,
    unsigned submitId, unsigned submitDirId,
    char *submitFileName, long long size)
/* Make a new, largely empty, record around file and submit info. */
{
struct dyString *query = dyStringNew(0);
sqlDyStringAppend(query, "insert cdwFile (submitId, submitDirId, userId, submitFileName, size) ");
dyStringPrintf(query, "VALUES(%u, %u, %u, '%s', %lld)", 
    submitId, submitDirId, userId, submitFileName, size);
sqlUpdate(conn, query->string);
dyStringFree(&query);
return sqlLastAutoId(conn);
}

void handleSubmitError(struct sqlConnection *conn, int submitId, char *err)
/* Write out error to stderr and also save it in errorMessage field of submit table. */
{
cdwWriteErrToStderrAndTable(conn, "cdwSubmit", submitId, err);
noWarnAbort();
}

void handleFileError(struct sqlConnection *conn, int submitId, int fileId, char *err)
/* Write out error to stderr and also save it in errorMessage field of file 
 * and submit table. */
{
/* Write out error message to errorMessage field of table. */
warn("%s", trimSpaces(err));
cdwWriteErrToTable(conn, "cdwFile", fileId, err);
cdwWriteErrToTable(conn, "cdwSubmit", submitId, err);
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
safef(tempFileName, PATH_LEN, "%scdwSubmitXXXXXX", cdwTempDir());

/* Get open file handle, copy file, and close. */
int localFd = mustMkstemp(tempFileName);
cpFile(remoteFd, localFd);
mustCloseFd(&localFd);
}

boolean cdwSubmitShouldStop(struct sqlConnection *conn, unsigned submitId)
/* Return TRUE if there's an error message on submit, indicating we should stop. */
{
char query[256];
sqlSafef(query, sizeof(query), "select errorMessage from cdwSubmit where id=%u", submitId);
char *errorMessage = sqlQuickString(conn, query);
boolean ret = isNotEmpty(errorMessage);
freez(&errorMessage);
return ret;
}

struct paraFetchInterruptContext
/* Data needed for interrupt checker. */
    {
    struct sqlConnection *conn;
    unsigned submitId;
    boolean isInterrupted;
    long long lastChecked;
    };

boolean paraFetchInterruptFunction(void *v)
/* Return TRUE if we need to interrupt. */
{
struct paraFetchInterruptContext *context = v;
long long now = cdwNow();
if (context->lastChecked != now)  // Only do check every second
    {
    context->isInterrupted = cdwSubmitShouldStop(context->conn, context->submitId);
    context->lastChecked = now;
    }
return context->isInterrupted;
}

int cdwFileFetch(struct sqlConnection *conn, struct cdwFile *ef, int fd, 
	char *submitUrl, unsigned submitId, unsigned submitDirId, unsigned hostId,
	unsigned userId)
/* Fetch file and if successful update a bunch of the fields in ef with the result. 
 * Returns fileId. */
{
/* Create file record in database */
ef->id = makeNewEmptyFileRecord(conn, userId, submitId, submitDirId, ef->submitFileName, ef->size);

char cdwFile[PATH_LEN] = "", cdwPath[PATH_LEN];
cdwMakeFileNameAndPath(ef->id, ef->submitFileName,  cdwFile, cdwPath);
ef->startUploadTime = cdwNow();
copyFile(ef->submitFileName, cdwPath);
chmod(cdwPath, 0444);
ef->cdwFileName = cloneString(cdwFile);
ef->endUploadTime = cdwNow();

/* Now we got the file.  We'll go ahead and save rest of cdwFile record.  This
 * includes tags that may be long, so we make query in a dy. Node that the
 * updateTime being non-zero is a sign to rest of system that record is complete. */
struct dyString *dy = dyStringNew(0);  /* Includes tag so query may be long */
sqlDyStringPrintf(dy, "update cdwFile set "
		      "  cdwFileName='%s', startUploadTime=%lld, endUploadTime=%lld,"
		      "  md5='%s', size=%lld, updateTime=%lld"
       , ef->cdwFileName, ef->startUploadTime, ef->endUploadTime
       , ef->md5, ef->size, ef->updateTime);
dyStringAppend(dy, ", tags='");
dyStringAppend(dy, ef->tags);
dyStringPrintf(dy, "' where id=%d", ef->id);
uglyf("%s\n", dy->string);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);

return ef->id;
}

int findFileGivenMd5AndSubmitDir(struct sqlConnection *conn, char *md5, int submitDirId)
/* Given hexed md5 and a submitDir see if we have a matching file. */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select file.id from cdwSubmit sub,cdwFile file "
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
sqlSafef(query, sizeof(query), "select tags from cdwFile where id=%u", id);
char *tagsString = sqlQuickString(conn, query);
struct hash *tagHash=NULL;
struct cgiVar *tagList=NULL;
if (!isEmpty(tagsString))
    cgiParseInputAbort(tagsString, &tagHash, &tagList);


char **row;
struct sqlResult *sr = sqlGetResult(conn, "NOSQLINJ select * from cdwSubscriber order by runOrder,id");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cdwSubscriber *subscriber = cdwSubscriberLoad(row);
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
    cdwSubscriberFree(&subscriber);
    }

sqlFreeResult(&sr);
freez(&tagsString);
slFreeList(&tagList);
hashFree(&tagHash);
}

void allGoodFileNameChars(char *fileName)
/* Return TRUE if all chars are good for a file name */
{
char c, *s = fileName;
while ((c = *s++) != 0)
    if (!isalnum(c) && c != '_' && c != '-' && c != '.' && c != '/' && c != '+') 
        errAbort("Character '%c' (binary %d) not allowed in fileName '%s'", c, (int)c, fileName);
}

void allGoodSymbolChars(char *symbol)
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

boolean isExperimentId(char *experiment)
/* Return TRUE if it looks like an CIRM experiment ID */
{
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
         
boolean isSupportedFormat(char *format)
/* Return TRUE if this is one of our supported formats */
{
/* First deal with non bigBed */
static char *otherSupportedFormats[] = {"unknown", "fastq", "bam", "bed", "gtf", 
    "bigWig", "bigBed", 
    "bedLogR", "bedRrbs", "bedMethyl", "broadPeak", "narrowPeak", 
    "bed_bedLogR", "bed_bedRrbs", "bed_bedMethyl", "bed_broadPeak", "bed_narrowPeak",
    "bedRnaElements", "openChromCombinedPeaks", "peptideMapping", "shortFrags", 
    "rcc", "idat", "fasta", "customTrack", "pdf", "vcf",
    };
static int otherSupportedFormatsCount = ArraySize(otherSupportedFormats);
if (stringArrayIx(format, otherSupportedFormats, otherSupportedFormatsCount) >= 0)
    return TRUE;

/* If starts with bed_ then skip over prefix.  It will be caught by bigBed */
if (startsWith("bed_", format))
    format += 4;
return cdwIsSupportedBigBedFormat(format);
}


boolean isEmptyOrNa(char *s)
/* Return TRUE if string is NULL, "", "n/a", or "N/A" */
{
if (isEmpty(s))
    return TRUE;
return sameWord(s, "n/a");
}


void cdwParseSubmitFile(struct sqlConnection *conn, char *submitLocalPath, char *submitUrl, 
    struct submitFileRow **retSubmitList)
/* Load and parse up this file as fielded table, make sure all required fields are there,
 * and calculate indexes of required fields.   This produces an cdwFile list, but with
 * still quite a few fields missing - just what can be filled in from submit filled in. 
 * The submitUrl is just used for error reporting.  If it's local, just make it the
 * same as submitLocalPath. */
{
char *requiredFields[] = {"file", "format", "meta", };
struct fieldedTable *table = fieldedTableFromTabFile(submitLocalPath, submitUrl,
	requiredFields, ArraySize(requiredFields));

/* Get offsets of all required fields */
int fileIx = stringArrayIx("file", table->fields, table->fieldCount);
int formatIx = stringArrayIx("format", table->fields, table->fieldCount);
int metaIx = stringArrayIx("meta", table->fields, table->fieldCount);

uglyf("fileIx %d, formatIx %d, metaIx %d\n", fileIx, formatIx, metaIx);

#ifdef SOON
/* Get offsets of some other handy fields too */
int outputIx = stringArrayIx("output_type", table->fields, table->fieldCount);
int experimentIx = stringArrayIx("experiment", table->fields, table->fieldCount);
int replicateIx = stringArrayIx("replicate", table->fields, table->fieldCount);
int enrichedIx = stringArrayIx("enriched_in", table->fields, table->fieldCount);
int md5Ix = stringArrayIx("md5_sum", table->fields, table->fieldCount);
int sizeIx = stringArrayIx("size", table->fields, table->fieldCount);
int modifiedIx = stringArrayIx("modified", table->fields, table->fieldCount);
int validIx = stringArrayIx("valid_key", table->fields, table->fieldCount);

/* See if we're doing replacement and check have all columns needed if so. */
int replacesIx = stringArrayIx(replacesTag, table->fields, table->fieldCount);
int replaceReasonIx = stringArrayIx(replaceReasonTag, table->fields, table->fieldCount);
boolean doReplace = (replacesIx != -1);
if (doReplace)
    if (replaceReasonIx == -1)
        errAbort("Error: got \"%s\" column without \"%s\" column in %s.", 
	    replacesTag, replaceReasonTag, submitUrl);

/* Loop through and make sure all field values are ok */
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char **row = fr->row;
    char *fileName = row[fileIx];
    allGoodFileNameChars(fileName);
    char *format = row[formatIx];
    if (!isSupportedFormat(format))
	errAbort("Format %s is not supported", format);
    allGoodSymbolChars(row[outputIx]);
    char *experiment = row[experimentIx];
    if (!isExperimentId(experiment))
        errAbort("%s in experiment field does not seem to be experiment id", experiment);
    char *replicate = row[replicateIx];
    if (differentString(replicate, "pooled") && differentString(replicate, "n/a") )
	if (!isAllNum(replicate))
	    errAbort("%s is not a good value for the replicate column", replicate);
    char *enriched = row[enrichedIx];
    if (!cdwCheckEnrichedIn(enriched))
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
    char *realValid = cdwCalcValidationKey(md5, sqlLongLong(size));
    if (!sameString(validIn, realValid))
        errAbort("The valid_key %s for %s doesn't fit", validIn, fileName);
    freez(&realValid);

    if (doReplace)
	{
	char *replaces = row[replacesIx];
	char *reason = row[replaceReasonIx];
	if (!isEmptyOrNa(replaces))
	    {
	    char *prefix = cdwLicensePlateHead(conn);
	    if (!startsWith(prefix, replaces))
		errAbort("%s in replaces column is not an CIRM file accession", replaces);
	    if (isEmptyOrNa(reason))
		errAbort("Replacing %s without a reason\n", replaces);
	    }
	}
    }

*retSubmitList = submitFileRowFromFieldedTable(conn, table, 
    fileIx, md5Ix, sizeIx, modifiedIx, replacesIx, replaceReasonIx);
#endif /* SOON */
uglyAbort("cdwParseSubmittedFile not fully implemented");
}

void notOverlappingSelf(struct sqlConnection *conn, char *url)
/* Ensure we are only submission going on for this URL, allowing for time out
 * and command line override. */
{
#ifdef OLD
if (doNow) // Allow command line override
    return; 
#endif /* OLD */

/* Fetch most recent submission from this URL. */
struct cdwSubmit *old = cdwMostRecentSubmission(conn, url);
if (old == NULL)
    return;

/* See if we have something in progress, meaning started but not ended. */
if (old->endUploadTime == 0 && isEmpty(old->errorMessage))
    {
    /* Check submission last alive time against our usual time out. */
    long long maxStartTime = cdwSubmitMaxStartTime(old, conn);
    if (cdwNow() - maxStartTime < cdwSingleFileTimeout)
        errAbort("Submission of %s already is in progress.  Please come back in an hour", url);
    }
cdwSubmitFree(&old);
}

void getSubmittedFile(struct sqlConnection *conn, struct cdwFile *bf,  
    char *submitDir, char *submitUrl, int submitId, int userId)
/* We know the submission, we know what the file is supposed to look like.  Fetch it.
 * If things go badly catch the error, attach it to the submission record, and then
 * keep throwing. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    if (freeSpaceOnFileSystem(cdwRootDir) < 2*bf->size)
	errAbort("No space left in warehouse!!");

    int hostId=0, submitDirId = 0;
    int fd = cdwOpenAndRecordInDir(conn, submitDir, bf->submitFileName, submitUrl,
	&hostId, &submitDirId);

    int fileId = cdwFileFetch(conn, bf, fd, submitUrl, submitId, submitDirId, hostId, userId);

    close(fd);
    cdwAddQaJob(conn, fileId);
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

boolean cgiDictionaryVarInListSame(struct cgiDictionary *d, struct cgiVar *list,
    char **retName, char **retDictVal,  char **retListVal)
/* Return TRUE if all variables in list are found in dictionary with the same vals. */
{
struct cgiVar *var;
struct hash *hash = d->hash;
for (var = list; var != NULL; var = var->next)
    {
    struct cgiVar *dVar = hashFindVal(hash, var->name);
    if (dVar == NULL)
	{
	*retName = var->name;
	*retListVal = var->val;
	*retDictVal = NULL;
        return FALSE;
	}
    if (!sameString(dVar->val, var->val))
	{
	*retName = var->name;
	*retListVal = var->val;
	*retDictVal = dVar->val;
        return FALSE;
	}
    }
return TRUE;
}

boolean cgiDictionaryFirstDiff(struct cgiDictionary *a, struct cgiDictionary *b,
    char **retName, char **retOldVal,  char **retNewVal)
/* If the dictionaries differ then return TRUE and return info about first difference. */
{
return !(cgiDictionaryVarInListSame(a, b->list, retName, retOldVal, retNewVal) 
    && cgiDictionaryVarInListSame(b, a->list, retName, retNewVal, retOldVal));
}

boolean cgiDictionarySame(struct cgiDictionary *a, struct cgiDictionary *b)
/* See if dictionaries have same tags with same values. */
{
char *ignore = NULL;
return !cgiDictionaryFirstDiff(a, b, &ignore, &ignore, &ignore);
}

static void updateSubmitName(struct sqlConnection *conn, long long fileId, char *newSubmitName)
/* Update submit name in database. */
{
char query[256];
sqlSafef(query, sizeof(query), 
   "update cdwFile set submitFileName=\"%s\" where id=%lld", newSubmitName, fileId);
sqlUpdate(conn, query);
}

int handleOldFileTags(struct sqlConnection *conn, struct submitFileRow *sfrList,
    boolean update)
/* Check metadata on files mentioned in manifest that by MD5 sum we already have in
 * warehouse.   We may want to update metadata on these. This returns the number
 * of files with tags updated. */
{
struct submitFileRow *sfr;
int updateCount = 0;
for (sfr = sfrList; sfr != NULL; sfr = sfr->next)
    {
    struct cdwFile *newFile = sfr->file;
    struct cdwFile *oldFile = cdwFileFromId(conn, sfr->md5MatchFileId);
    verbose(2, "looking at old file %s (%s)\n", oldFile->submitFileName, newFile->submitFileName);
    struct cgiDictionary *newTags = cgiDictionaryFromEncodedString(newFile->tags);
    struct cgiDictionary *oldTags = cgiDictionaryFromEncodedString(oldFile->tags);
    boolean updateName = !sameString(oldFile->submitFileName, newFile->submitFileName);
    boolean updateTags = !cgiDictionarySame(oldTags, newTags);
    if (updateName)
	{
	if (!update)
	    errAbort("%s already uploaded with name %s.  Please use the 'update' option if you "
	             "want to give it a new name.",  
		     newFile->submitFileName, oldFile->submitFileName);
        updateSubmitName(conn, oldFile->id,  newFile->submitFileName);
	}
    if (updateTags)
	{
	if (!update)
	    {
	    char *name="", *oldVal="", *newVal="";
	    cgiDictionaryFirstDiff(oldTags, newTags, &name, &oldVal, &newVal);
	    errAbort("%s is duplicate of %s in warehouse, but %s column went from %s to %s.\n"
	             "Please use the 'update' option if you are meaning to update the information\n"
		     "associated with this file and try again if this is intentional.",
		     newFile->submitFileName, oldFile->cdwFileName,
		     name, oldVal, newVal);
	    }
	verbose(1, "updating tags for %s\n", newFile->submitFileName);
	cdwFileResetTags(conn, oldFile, newFile->tags, TRUE);
	}
    if (updateTags || updateName)
	++updateCount;
    cgiDictionaryFree(&oldTags);
    cgiDictionaryFree(&newTags);
    }
return updateCount;
}

void doValidatedEmail(struct cdwSubmit *submit, boolean isComplete)
/* Send an email with info on all validated files */
{
struct sqlConnection *conn = cdwConnect();
struct cdwUser *user = cdwUserFromId(conn, submit->userId);
struct dyString *message = dyStringNew(0);
/* Is this submission has no new file at all */
if ((submit->oldFiles != 0) && (submit->newFiles == 0) &&
    (submit->metaChangeCount == 0)  && isEmpty(submit->errorMessage)
     && (submit->fileIdInTransit == 0))
    {
    dyStringPrintf(message, "Your submission from %s is completed, but validation was not performed for this submission since all files in validate.txt have been previously submitted and validated.\n", submit->url);
    mailViaPipe(user->email, "CDW Validation Results", message->string, cdwDaemonEmail);
    sqlDisconnect(&conn);
    dyStringFree(&message);
    return;
    }

if (isComplete)
    dyStringPrintf(message, "Your submission from %s is completely validated\n", submit->url);
else
    dyStringPrintf(message, 
	"Your submission hasn't validated after 24 hours, something is probably wrong\n"
	"at %s\n", submit->url);
dyStringPrintf(message, "\n#accession\tsubmitted_file_name\tnotes\n");
char query[512];
sqlSafef(query, sizeof(query),
    "select licensePlate,submitFileName "
    " from cdwFile left join cdwValidFile on cdwFile.id = cdwValidFile.fileId "
    " where cdwFile.submitId = %u and cdwFile.id != %u"
    , submit->id, submit->manifestFileId);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    {
    char *licensePlate = row[0];
    char *submitFileName = row[1];
    dyStringPrintf(message, "%s\t%s\t", naForNull(licensePlate), submitFileName);
    if (licensePlate == NULL)
        {
	dyStringPrintf(message, "Not validating");
	}
    dyStringPrintf(message, "\n");
    }
sqlFreeResult(&sr);

mailViaPipe(user->email, "CDW Validation Results", message->string, cdwDaemonEmail);
sqlDisconnect(&conn);
dyStringFree(&message);
}

void waitForValidationAndSendEmail(struct cdwSubmit *submit, char *email)
/* Poll database every 5 minute or so to see if finished. */
{
int maxSeconds = 3600*24;
int secondsPer = 60*5;
int seconds;
for (seconds = 0; seconds < maxSeconds; seconds += secondsPer)
    {
    struct sqlConnection *conn = cdwConnect();
    if (cdwSubmitIsValidated(submit, conn))
         {
	 doValidatedEmail(submit, TRUE);
	 return;
	 }
    verbose(2, "waiting for validation\n");
    sqlDisconnect(&conn);
    sleep(secondsPer);	// Sleep for 5 more minutes
    }
doValidatedEmail(submit, FALSE);
}


void checkManifestAndMetadata( struct fieldedTable *table, int fileIx, int formatIx, int metaIx,
    struct tagStorm *tagStorm, struct hash *metaHash)
/* Make sure that all file names are unique, all metadata tags are unique, and that
 * meta tags in table exist in tagStorm.  Some of the replace a file logic is here. */
{
/* Check files for uniqueness, formats for being supported, and meta for existance. */
struct fieldedRow *row;
struct hash *fileHash = hashNew(0);
for (row = table->rowList; row != NULL; row = row->next)
    {
    char *file = row->row[fileIx];
    char *format = row->row[formatIx];
    char *meta = row->row[metaIx];

    /* Make sure that files are all unique */
    struct fieldedRow *oldRow = hashFindVal(fileHash, file);
    if (oldRow != NULL)
        errAbort("File %s duplicated on lines %d and %d of %s", 
		file, oldRow->id, row->id, table->name);
    hashAdd(fileHash, file, row);

    if (!isSupportedFormat(format))
	errAbort("Format %s is not supported", format);

    if (!hashLookup(metaHash, meta))
        errAbort("Meta ID %s is in %s but not %s", meta, table->name, tagStorm->fileName);
    }
}

char *nullForNaOrEmpty(char *s)
/* If s is NULL, "", or "n/a" return NULL, otherwise return s */
{
if (s == NULL || s[0] == 0 || sameWord(s, "n/a"))
    return NULL;
else
    return s;
}

int storeSubmissionFile(struct sqlConnection *conn,
    char *submitFileName, int submitId, int submitDirId, int userId)
/* Save file to warehouse and make a record for it.  This is for tagless files,
 * just the ones that make up the submission metadata. */
{
/* Calculate md5sum and see if we have it already. */
char *md5 = md5HexForFile(submitFileName);
int oldFileId = findFileGivenMd5AndSubmitDir(conn, md5, submitDirId);
if (oldFileId != 0)
    {
    freeMem(md5);
    return oldFileId;
    }

long long size = fileSize(submitFileName);
long long updateTime = fileModTime(submitFileName);
int fileId = makeNewEmptyFileRecord(conn, userId, submitId, submitDirId, submitFileName, size);
char cdwFile[PATH_LEN] = "", cdwPath[PATH_LEN];
cdwMakeFileNameAndPath(fileId, submitFileName,  cdwFile, cdwPath);
long long startUploadTime = cdwNow();
copyFile(submitFileName, cdwPath);
chmod(cdwPath, 0444);
long long endUploadTime = cdwNow();

char query[3*PATH_LEN];
sqlSafef(query, sizeof(query), "update cdwFile set "
		      "  submitId=%d, submitDirId=%d,"
		      "  cdwFileName='%s', startUploadTime=%lld, endUploadTime=%lld,"
		      "  md5='%s', updateTime=%lld where id=%d"
       , submitId, submitDirId
       , cdwPath, startUploadTime, endUploadTime
       , md5, updateTime, fileId);
uglyf("%s\n", query);
sqlUpdate(conn, query);
freeMem(md5);
return fileId;
}

    
char *cacheMd5(struct sqlConnection *conn, char *fileName, 
    char *submitDir, long long updateTime, long long size)
/* Get md5sum for fileName, using cached version if possible */
{
char *md5 = hashFindVal(md5Hash, fileName);
if (md5 == NULL)
    {
    /* Not in cache, let's look in database */
    char query[PATH_LEN*2];
    sqlSafef(query, sizeof(query), 
		   "select md5 from cdwFile,cdwSubmitDir "
                   "where cdwSubmitDir.id=cdwFile.submitDirId "
		   "and cdwSubmitDir.url='%s' "
		   "and submitFileName='%s' and updateTime=%lld and size=%lld"
		   , submitDir, fileName, updateTime, size);
    md5 = sqlQuickString(conn, query);

    /* Not in database, let's go do the calc. */
    if (md5 == NULL)
	{
	verbose(1, "Patience, md5summing %s\n", fileName);
	md5 = md5HexForFile(fileName);
	}
    hashAdd(md5Hash, fileName, md5);
    }
return md5;
}

void cdwSubmit(char *email, char *manifestFile, char *metaFile)
/* cdwSubmit - Submit URL with validated.txt to warehouse. */
{
char query[4*1024];
char *submitDir = getCurrentDir();

/* Get table with the required fields and calculate field positions */
char *requiredFields[] = {"file", "format", "meta", };
struct fieldedTable *table = fieldedTableFromTabFile(manifestFile, manifestFile,
    requiredFields, ArraySize(requiredFields));
int fileIx = stringArrayIx("file", table->fields, table->fieldCount);
int formatIx = stringArrayIx("format", table->fields, table->fieldCount);
int metaIx = stringArrayIx("meta", table->fields, table->fieldCount);

uglyf("Got %d fields and %d rows in %s\n", table->fieldCount, slCount(table->rowList), manifestFile);
struct tagStorm *tagStorm = tagStormFromFile(metaFile);
struct hash *metaHash = tagStormUniqueIndex(tagStorm, "meta");
uglyf("Got %d items in metaHash\n", metaHash->elCount);
struct sqlConnection *conn = cdwConnectReadWrite();
struct cdwUser *user = cdwMustGetUserFromEmail(conn, email);
uglyf("User is %p\n", user);
checkManifestAndMetadata(table, fileIx, formatIx, metaIx, 
    tagStorm, metaHash);

/* Convert to data structure that has more fields.  If submission contains
 * replacement files, check that the accessions being replaced are legitimate. */
int md5Ix = stringArrayIx("md5", table->fields, table->fieldCount);
int replacesIx = stringArrayIx(replacesTag, table->fields, table->fieldCount);
int replaceReasonIx = stringArrayIx(replaceReasonTag, table->fields, table->fieldCount);
struct submitFileRow *sfrList = submitFileRowFromFieldedTable(conn, table, 
    fileIx, md5Ix, replacesIx, replaceReasonIx);
uglyf("Parsed manifest and metadata into %d files\n", slCount(sfrList));

/* Fake URL - system was built initially for remote files. */
char submitUrl[PATH_LEN];
safef(submitUrl, sizeof(submitUrl), "%s%s/%s", localPrefix, submitDir, manifestFile);

/* Figure out directory ID for submission */
int hostId = cdwGetHost(conn, "localhost");
int submitDirId = cdwGetSubmitDir(conn, hostId, submitDir);

/* Create a submission record */
int submitId = makeNewEmptySubmitRecord(conn, submitUrl, user->id);

/* Put our manifest and metadata files */
int manifestFileId= storeSubmissionFile(conn, manifestFile, submitId, submitDirId, user->id);
int metaFileId= storeSubmissionFile(conn, metaFile, submitId, submitDirId, user->id);

/* We'll make a loop through list figuring out which files are new and which are old.
 * We wrap errCatch block around these so that error message ends up in submission
 * record if there's a problem.  */
struct submitFileRow *oldList = NULL, *newList = NULL; 
int oldCount = 0, newCount = 0;
long long oldBytes = 0, newBytes = 0, byteCount = 0;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    /* Fill in file size and MD5 info */
    struct submitFileRow *sfr;
    for (sfr = sfrList; sfr != NULL; sfr = sfr->next)
	{
	struct cdwFile *file = sfr->file;
	char *fileName = file->submitFileName;

	/* Fill in file size and times */
	file->updateTime = fileModTime(fileName);
	file->size = fileSize(fileName);

	/* Fill in and check MD5 sums */
	char *oldMd5 = nullForNaOrEmpty(sfr->file->md5);
	char *newMd5 = cacheMd5(conn, fileName, submitDir, file->updateTime, file->size);
	verbose(2, "md5 for %s is %s\n", fileName, newMd5);
	if (oldMd5 == NULL)
	    {
	    memcpy(sfr->file->md5, newMd5, 32);
	    }
	else
	    {
	    if (!sameWord(oldMd5, newMd5))
		errAbort("md5 mismatch on %s\n%s - tag value\n%s - file value\n", 
		    fileName, oldMd5, newMd5);
	    }
        }

    /* Partition list into old and new files */
    struct submitFileRow *next;
    int fileCount = 0;
    for (sfr = sfrList; sfr != NULL; sfr = next)
	{
	next = sfr->next;
	struct cdwFile *file = sfr->file;

	/* See if it is already in repository */
	int oldFileId = findFileGivenMd5AndSubmitDir(conn, file->md5, submitDirId);
	if (oldFileId != 0)
	    {
	    slAddHead(&oldList, sfr);
	    oldCount += 1;
	    oldBytes += file->size;
	    sfr->md5MatchFileId = oldFileId;
	    }
	else
	    {
	    slAddHead(&newList, sfr);
	    }
	sfrList = next;
	byteCount += file->size;
	fileCount += 1;
	}
    slReverse(&newList);
    slReverse(&oldList);

    /* Update submission record database with what we know now. */
    sqlSafef(query, sizeof(query), 
	"update cdwSubmit set manifestFileId=%d,metaFileId=%d,submitDirId=%d,"
	"fileCount=%d,oldFiles=%d,oldBytes=%lld,byteCount=%lld where id=%u", 
	    manifestFileId, metaFileId,submitDirId,
	    fileCount,oldCount, oldBytes, byteCount, submitId);
    uglyf("updating submission:\n%s\n", query);
    sqlUpdate(conn, query);

    /* Deal with old files. This may throw an error.  We do it before downloading new
     * files since we want to fail fast if we are going to fail. */
    int updateCount = handleOldFileTags(conn, oldList, doUpdate);
    sqlSafef(query, sizeof(query), 
	"update cdwSubmit set metaChangeCount=%d where id=%u",  updateCount, submitId);
    sqlUpdate(conn, query);

    /* Go through list attempting to load the files if we don't already have them. */
    for (sfr = newList; sfr != NULL; sfr = sfr->next)
	{
	struct cdwFile *bf = sfr->file;
	getSubmittedFile(conn, bf, submitDir, submitUrl, submitId, user->id);

	/* Update submit record with progress (getSubmittedFile might be
	 * long and get interrupted) */
	newCount += 1,
	newBytes += bf->size;
	sqlSafef(query, sizeof(query), 
	    "update cdwSubmit set newFiles=newFiles+1,newBytes=%lld where id=%d", 
	    newBytes, submitId);
	sqlUpdate(conn, query);
	}

    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    handleSubmitError(conn, submitId, errCatch->message->string);
    /* The handleSubmitError will keep on throwing. */
    }
errCatchFree(&errCatch);

uglyf("GOt %d old files, %d new ones\n", oldCount, newCount);

/* If we made it here, update submit endUploadTime */
sqlSafef(query, sizeof(query),
	"update cdwSubmit set endUploadTime=%lld where id=%d", 
	cdwNow(), submitId);
sqlUpdate(conn, query);



sqlDisconnect(&conn);	// We'll be waiting a while so free connection
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
doUpdate = optionExists("update");
doBelieve = optionExists("believe");
if (optionExists("md5"))
    {
    md5Hash = md5FileHash(optionVal("md5", NULL));
    }
else
    {
    md5Hash = hashNew(0);
    }
    
if (argc != 4)
    usage();
cdwSubmit(argv[1], argv[2], argv[3]);
return 0;
}
