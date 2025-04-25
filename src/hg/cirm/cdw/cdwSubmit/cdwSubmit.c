/* cdwSubmit - Submit URL with manifest and metadata to warehouse. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include <sys/stat.h>
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
#include "hmac.h"
#include "paraFetch.h"
#include "md5.h"
#include "portable.h"
#include "obscure.h"
#include "hex.h"
#include "filePath.h"
#include "fieldedTable.h"
#include "cdw.h"
#include "cdwValid.h"
#include "cdwLib.h"
#include "mailViaPipe.h"
#include "tagStorm.h"
#include "tagSchema.h"

boolean doUpdate = FALSE;
boolean noRevalidate = FALSE;
boolean noBackup = FALSE;
boolean justTest = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwSubmit - Submit URL with validated.txt to warehouse.\n"
  "usage:\n"
  "   cdwSubmit email manifest.txt meta.txt\n"
  "where email is the email address associated with the data set, typically from the lab, not the\n"
  "wrangler.  You need to run cdwCreateUser with the email address if it's the first submission \n"
  "from that user.  Manifest.txt is a tab separated file with a header line and then one line \n"
  "per file. Meta.txt is in tagStorm format.\n"
  "\n"
  "options:\n"
  "   -update  If set, will update metadata on file it already has. The default behavior is to\n"
  "            report an error if metadata doesn't match.\n"
  "   -noRevalidate - if set don't run revalidator on update\n"
  "   -noBackup - if set don't backup database first\n"
  "   -md5=md5sum.txt Take list of file MD5s from output of md5sum command on list of files\n"
  "   -test This will look at the manifest and meta, but not actually load the database\n");
}

char *localPrefix = "local://localhost/";
struct hash *md5Hash;

/* Command line validation table. */
static struct optionSpec options[] = {
   {"update", OPTION_BOOLEAN},
   {"noRevalidate", OPTION_BOOLEAN},
   {"noBackup", OPTION_BOOLEAN},
   {"md5", OPTION_STRING},
   {"test", OPTION_BOOLEAN},
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
    struct fieldedRow *fr;  /* Row in the mother file */
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
    sfr->fr = fr;

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
sqlDyStringPrintf(query, "insert cdwSubmit (url, startUploadTime, userId, wrangler) ");
sqlDyStringPrintf(query, "VALUES('%s', %lld,  %d, '%s')", submitUrl, cdwNow(), 
	    userId, getenv("USER"));
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
sqlDyStringPrintf(query, "insert cdwFile (submitId, submitDirId, userId, submitFileName, size) ");
sqlDyStringPrintf(query, "VALUES(%u, %u, %u, '%s', %lld)", 
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

char *metaManiFieldVal(char *field, struct fieldedTable *table, struct fieldedRow *row,
    int metaIx, struct hash *metaHash)
/* Look up value of field in table if it exists.  If not in table, look in tagStorm
 * that is indexed by metaHash.  If not in either return NULL. */
{
int tableIx = stringArrayIx(field, table->fields, table->fieldCount);  // In manifest
if (tableIx >= 0)
    return row->row[tableIx];
char *meta = row->row[metaIx];
struct tagStanza *stanza = hashFindVal(metaHash, meta);
return tagFindVal(stanza, field);
}

static void addFileToUsersGroup(struct sqlConnection *conn, struct cdwUser *user, long long fileId)
/* Add file to user's primary group */
{
if (user->primaryGroup != 0)
    {
    struct dyString *dy = dyStringNew(0);
    sqlDyStringPrintf(dy, "insert into cdwGroupFile (fileId,groupId) values (%lld,%u)",
	fileId, user->primaryGroup);
    sqlUpdate(conn, dy->string);
    dyStringFree(&dy);
    }
}

int cdwFileFetch(struct sqlConnection *conn, struct cdwFile *ef, int fd, 
	char *submitDir, char *submitUrl, unsigned submitId, unsigned submitDirId, unsigned hostId,
	struct cdwUser *user, struct fieldedTable *table, struct fieldedRow *row, 
	int metaIx, struct hash *metaHash)
/* Fetch file and if successful update a bunch of the fields in ef with the result. 
 * Returns fileId. */
{
/* Create file record in database */
ef->id = makeNewEmptyFileRecord(conn, user->id, submitId, submitDirId, 
    ef->submitFileName, ef->size);

char cdwFile[PATH_LEN] = "", cdwPath[PATH_LEN];
cdwMakeFileNameAndPath(ef->id, ef->submitFileName,  cdwFile, cdwPath);
ef->startUploadTime = cdwNow();

// keep copy so that we can be sure to own the target file
verbose(3, "cdwFile=%s\n", cdwFile);
verbose(3, "copyFile submitFileName=%s to cdwPath=%s\n", ef->submitFileName, cdwPath);
copyFile(ef->submitFileName, cdwPath);
// and owner can touch and chmod it
touchFileFromFile(ef->submitFileName, cdwPath); // preserve mtime.
chmod(cdwPath, 0444);

// save space by finding the last real file in symlinks chain
// and replace IT with a symlink to the new file. 
// However, make an exception for meta.txt which can get 
// be listed in maniSummary.txt
boolean metaException = sameString(ef->submitFileName, "meta.txt");
if (!metaException)
    {
    replaceOriginalWithSymlink(ef->submitFileName, submitDir, cdwPath);
    }
ef->cdwFileName = cloneString(cdwFile);
ef->endUploadTime = cdwNow();
char *access = metaManiFieldVal("access", table, row, metaIx, metaHash);
if (access == NULL)
    access = "group";

ef->userAccess = cdwAccessWrite;
if (sameString(access, "all"))
    ef->allAccess = cdwAccessRead;
else if (sameString(access, "group"))
    ef->groupAccess = cdwAccessRead;
else if (!sameString(access, "user"))
    errAbort("Unknown access %s", access);

/* Now we got the file.  We'll go ahead and save rest of cdwFile record.  This
 * includes tags that may be long, so we make query in a dy. Node that the
 * updateTime being non-zero is a sign to rest of system that record is complete. */
struct dyString *dy = dyStringNew(0);  /* Includes tag so query may be long */
sqlDyStringPrintf(dy, "update cdwFile set "
		      "  cdwFileName='%s', startUploadTime=%lld, endUploadTime=%lld,"
		      "  md5='%s', size=%lld, updateTime=%lld, metaTagsId=%u, "
		      "  userAccess=%d, groupAccess=%d, allAccess=%d"
       , ef->cdwFileName, ef->startUploadTime, ef->endUploadTime
       , ef->md5, ef->size, ef->updateTime, ef->metaTagsId
       , ef->userAccess, ef->groupAccess, ef->allAccess);
sqlDyStringPrintf(dy, ", tags='%s'", ef->tags);
sqlDyStringPrintf(dy, " where id=%d", ef->id);
sqlUpdate(conn, dy->string);

/* We also will add file to the group */
addFileToUsersGroup(conn, user, ef->id);

dyStringFree(&dy);
return ef->id;
}

int findFileGivenMd5AndSubmitDir(struct sqlConnection *conn, char *md5, char *submitFileName,
    int submitDirId)
/* Given hexed md5 and a submitDir see if we have a matching file. */
{
char query[2*PATH_LEN];
sqlSafef(query, sizeof(query), 
    "select file.id from cdwSubmit sub,cdwFile file "
    "where file.md5 = '%s' and file.submitId = sub.id and sub.submitDirId = %d "
    "and file.submitFileName = '%s'"
    , md5, submitDirId, submitFileName);
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
sqlSafef(query, sizeof(query), "select * from cdwSubscriber order by runOrder,id");
struct sqlResult *sr = sqlGetResult(conn, query);
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

boolean isSupportedFormat(char *format)
/* Return TRUE if this is one of our supported formats */
{
/* First deal with non bigBed */
static char *otherSupportedFormats[] = {"unknown", "fastq", "bam", "bed", "gtf", 
    "bam.bai", "vcf.gz.tbi",
    "bigWig", "bigBed", 
    "bedLogR", "bedRrbs", "bedMethyl", "broadPeak", "narrowPeak", 
    "bed_bedLogR", "bed_bedRrbs", "bed_bedMethyl", "bed_broadPeak", "bed_narrowPeak",
    "bedRnaElements", "openChromCombinedPeaks", "peptideMapping", "shortFrags", 
    "rcc", "idat", "fasta", "customTrack", "pdf", "png", "vcf", "cram", "jpg", "text", "html",
    "txt", "tsv", "csv",
    "raw", "xls", "xlsx",
    "h5ad", "rds",
    "tif", "avi",
    "kallisto_abundance", "expression_matrix",
    };
static int otherSupportedFormatsCount = ArraySize(otherSupportedFormats);
if (stringArrayIx(format, otherSupportedFormats, otherSupportedFormatsCount) >= 0)
    return TRUE;

/* If starts with bed_ then skip over prefix.  It will be caught by bigBed */
if (startsWith("bed_", format))
    format += 4;
return cdwIsSupportedBigBedFormat(format);
}

void prefetchChecks(char *format, char *fileName, char *submitDir)
/* Perform some basic format checks. */
{
if (sameString(format, "fastq") || sameString(format, "vcf"))
    {
    if (!cdwIsGzipped(fileName))
        errAbort("%s file %s must be gzipped", format, fileName);
    }

// Aborts early if fileName points to a file already under cdwRootDir
char *path = testOriginalSymlink(fileName, submitDir);  

// check if parent dir is writable, needed for symlinking
char dir[PATH_LEN];
splitPath(path, dir, NULL, NULL);
if (sameString(dir, ""))
    safecpy(dir, PATH_LEN, ".");
if (access(dir, R_OK | W_OK) == -1)
    errnoAbort("%s directory must be writable to enable symlinking.", dir);

freeMem(path);
}

void getSubmittedFile(struct sqlConnection *conn, char *format,
    struct tagStorm *tagStorm, struct hash *metaHash, struct cdwFile *ef,  
    char *submitDir, char *submitUrl, int submitId, struct cdwUser *user,
    struct fieldedTable *table, struct fieldedRow *row, int metaIx)
/* We know the submission, we know what the file is supposed to look like.  Fetch it.
 * If things go badly catch the error, attach it to the submission record, and then
 * keep throwing. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    if (freeSpaceOnFileSystem(cdwRootDir) < 2*ef->size)
	errAbort("No space left in warehouse!!");

    int hostId=0, submitDirId = 0;
    int fd = cdwOpenAndRecordInDir(conn, submitDir, ef->submitFileName, submitUrl,
	&hostId, &submitDirId);

    prefetchChecks(format, ef->submitFileName, submitDir);
    verbose(1, "copying %s\n", ef->submitFileName);
    int fileId = cdwFileFetch(conn, ef, fd, submitDir, submitUrl, submitId, submitDirId, hostId, user,
	table, row, metaIx, metaHash);
    close(fd);
    cdwAddQaJob(conn, fileId, submitId);
    tellSubscribers(conn, submitDir, ef->submitFileName, fileId);
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

void cdwFileUpdateMetaTagsId(struct sqlConnection *conn, long long fileId, long long metaTagsId)
/* Update metaTagsId for file */
{
char query[128];
sqlSafef(query, sizeof(query), "update cdwFile set metaTagsId=%lld where id=%lld",  
    metaTagsId, fileId);
sqlUpdate(conn, query);
}

int handleOldFileTags(struct sqlConnection *conn, 
    struct hash *metaIdHash, struct submitFileRow *sfrList, boolean update, int submitId)
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
    boolean updateMeta = (newFile->metaTagsId != oldFile->metaTagsId);

    if (updateName)
	{
	if (!update)
	    errAbort("%s already uploaded with name %s.  Please use the 'update' option if you "
	             "want to give it a new name.",  
		     newFile->submitFileName, oldFile->submitFileName);
        updateSubmitName(conn, oldFile->id,  newFile->submitFileName);
	}
    if (updateMeta)
	{
	if (!update)
	    errAbort("Old file %s has new metadata.  Please use the 'update' option to allow "
	             "this.", newFile->submitFileName);
	cdwFileUpdateMetaTagsId(conn, oldFile->id, newFile->metaTagsId);
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
	}
    if (updateMeta || updateTags)
	cdwFileResetTags(conn, oldFile, newFile->tags, !noRevalidate, submitId);
    if (updateTags || updateName || updateMeta)
	++updateCount;
    cgiDictionaryFree(&oldTags);
    cgiDictionaryFree(&newTags);
    }
return updateCount;
}

static void rCheckTagValid(struct tagStorm *tagStorm, struct tagStanza *list, struct hash *schemaHash)
/* Check tagStorm tags */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *pair;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
	{
	cdwValidateTagName(pair->name, schemaHash);
	}
    rCheckTagValid(tagStorm, stanza->children, schemaHash);
    }
}

void checkMetaTags(struct tagStorm *tagStorm, struct hash *schemaHash)
/* Check tags are all good. */
{
rCheckTagValid(tagStorm, tagStorm->forest, schemaHash);
}

boolean isEnrichedInFormat(char *format)
/* Return TRUE if it is a genomic format */
{
char *formats[] = {"bed", "bigBed", "bigWig", "fastq", "gtf",};
return stringArrayIx(format, formats, ArraySize(formats)) >= 0;
}

void checkManifestAndMetadata( struct fieldedTable *table, 
    int fileIx, int formatIx, int metaIx, int enrichedInIx,
    struct tagStorm *tagStorm, struct hash *metaHash, struct hash *schemaHash)
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

    struct tagStanza *stanza = hashFindVal(metaHash, meta);
    if (stanza == NULL)
        errAbort("Meta ID %s is in %s but not %s", meta, table->name, tagStorm->fileName);

    if (isEnrichedInFormat(format) && enrichedInIx < 0)
        {
	char *enrichedIn = tagFindVal(stanza, "enriched_in");
	if (enrichedIn == NULL)
	    errAbort("Genomics file %s missing enriched_in tag", file);
	}

    if (justTest)
        {
	static char *otherForcedFields[] = {"sample_label", "data_set_id", "assay", "lab", 
	    "ucsc_db"};
	int i;
	for (i=0; i<ArraySize(otherForcedFields); ++i)
	    {
	    char *field = otherForcedFields[i];
	    if (metaManiFieldVal(field, table, row, metaIx, metaHash) == NULL)
		errAbort("Missing %s field for %s", field, file);
	    }
	}
    }

/* Check manifest.txt tags */
int i;
for (i=0; i<table->fieldCount; ++i)
    {
    char *field = table->fields[i];
    cdwValidateTagName(field, schemaHash);
    }

/* Check meta.txt tags */
checkMetaTags(tagStorm, schemaHash);
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
    char *submitFileName, int submitId, int submitDirId, struct cdwUser *user, char *access)
/* Save file to warehouse and make a record for it.  This is for tagless files,
 * just the ones that make up the submission metadata. */
{
/* Calculate md5sum and see if we have it already. */
char *md5 = md5HexForFile(submitFileName);
int oldFileId = findFileGivenMd5AndSubmitDir(conn, md5, submitFileName, submitDirId);
if (oldFileId != 0)
    {
    freeMem(md5);
    return oldFileId;
    }

long long size = fileSize(submitFileName);
long long updateTime = fileModTime(submitFileName);
int fileId = makeNewEmptyFileRecord(conn, user->id, submitId, submitDirId, submitFileName, size);
char cdwFile[PATH_LEN] = "", cdwPath[PATH_LEN];
cdwMakeFileNameAndPath(fileId, submitFileName,  cdwFile, cdwPath);
long long startUploadTime = cdwNow();
copyFile(submitFileName, cdwPath);
touchFileFromFile(submitFileName, cdwPath);
chmod(cdwPath, 0444);
long long endUploadTime = cdwNow();

char query[3*PATH_LEN];

/* Figure out access variables */
int userAccess = cdwAccessWrite;
int groupAccess = 0;
int allAccess = 0;
if (sameString(access, "all"))
    allAccess = cdwAccessRead;
else if (sameString(access, "group"))
    groupAccess = cdwAccessRead;
else if (!sameString(access, "user"))
    errAbort("Unknown access %s", access);

sqlSafef(query, sizeof(query), "update cdwFile set "
		      "  submitId=%d, submitDirId=%d,"
		      "  cdwFileName='%s', startUploadTime=%lld, endUploadTime=%lld,"
		      "  md5='%s', updateTime=%lld, userAccess=%d, groupAccess=%d, allAccess=%d where id=%d"
       , submitId, submitDirId
       , cdwFile, startUploadTime, endUploadTime
       , md5, updateTime, userAccess, groupAccess, allAccess, fileId);
sqlUpdate(conn, query);
addFileToUsersGroup(conn, user, fileId);
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

struct hash *hashStanzaTags(struct tagStanza *stanza)
/* Create a hash containing all tags in stanza and it's parents */
{
struct hash *hash = hashNew(0);
struct tagStanza *parent;
for (parent = stanza; parent != NULL; parent = parent->parent)
    {
    struct slPair *pair;
    for (pair = parent->tagList; pair != NULL; pair = pair->next)
        {
	if (hashLookup(hash, pair->name) == NULL)
	    hashAdd(hash, pair->name, pair->val);
	}
    }
return hash;
}

int storeCdwMetaTags(struct sqlConnection *conn, struct tagStanza *stanza)
/* Store stanza in database if it is not already there.  Return id of stanza */
{
/* Make up hash of all tags in self and ancestors,  but don't let ancestors
 * override already defined tags */
struct hash *hash = hashStanzaTags(stanza);

struct dyString *cgi = dyStringNew(0);
cgiEncodeHash(hash, cgi);
char *md5 = hmacMd5("", cgi->string);

struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "select id from cdwMetaTags where md5='%s' and tags='%s'", 
    md5, cgi->string);
int metaTagsId = sqlQuickNum(conn, query->string);

/* Create query to insert tags */
if (metaTagsId == 0)
    {
    dyStringClear(query);
    sqlDyStringPrintf(query, "insert cdwMetaTags (tags,md5) values('%s','%s')", cgi->string, md5);
    sqlUpdate(conn, query->string);
    metaTagsId = sqlLastAutoId(conn);
    }
dyStringFree(&query);
freez(&md5);
hashFree(&hash);
dyStringFree(&cgi);
return metaTagsId;
}

struct hash *storeUsedMetaTags(struct sqlConnection *conn, struct fieldedTable *table, int metaIx,
    struct tagStorm *tagStorm, struct hash *metaStanzaHash)
/* Store metadata tags that are used in table in database.  Returns a hash filled with
 * integer valued cdwMetaTags ids keyed by the metadata name */
{
struct hash *hash = hashNew(0);
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char *metaId = fr->row[metaIx];
    struct tagStanza *metaStanza = hashFindVal(metaStanzaHash, metaId);
    if (metaStanza != NULL)
	{
	int metaTagsId = hashIntValDefault(hash, metaId, 0);
	if (metaTagsId == 0)
	    {
	    metaTagsId = storeCdwMetaTags(conn, metaStanza);
	    hashAddInt(hash, metaId, metaTagsId);
	    }
	}
    }
return hash;
}

void cdwSubmit(char *email, char *manifestFile, char *metaFile)
/* cdwSubmit - Submit URL with validated.txt to warehouse. */
{
char query[4*1024];
char *submitDir = cloneString(getCurrentDir());
if (strchr(manifestFile,'/') || strchr(metaFile,'/'))
    errAbort("Do not specify a directory in the path of the manifest or meta files. Change to the directory they are in instead.");

/* Get table with the required fields and calculate field positions */

/* Look at the header line of the manifest file. If there is a data_set_id field use it as
 * the linker for meta data. Otherwise use the meta field. */
FILE *f = mustOpen(manifestFile,"r");
char header[2048]; 
mustGetLine(f, header, 2048); 
// Look for data_set_id in the manifestFile, if it is there then override
// the meta value for the rest of the submit pipeline
char *requiredFields[] = {"file", "format", "meta",};  

struct fieldedTable *table = fieldedTableFromTabFile(manifestFile, manifestFile,
    requiredFields, ArraySize(requiredFields));
int fileIx = stringArrayIx("file", table->fields, table->fieldCount);
int formatIx = stringArrayIx("format", table->fields, table->fieldCount);
int metaIx = stringArrayIx("meta", table->fields, table->fieldCount); 
int enrichedInIx = stringArrayIx("enriched_in", table->fields, table->fieldCount);

verbose(1, "Got %d fields and %d rows in %s\n", 
    table->fieldCount, slCount(table->rowList), manifestFile);
struct tagStorm *tagStorm = tagStormFromFile(metaFile);
struct hash *metaHash = tagStormIndexExtended(tagStorm, "meta", TRUE, FALSE); 
verbose(1, "Got %d items in metaHash\n", metaHash->elCount);

struct sqlConnection *conn = cdwConnectReadWrite();
char *schemaFile = cdwRequiredSetting(conn, "schema");
verbose(2, "Reading schema from %s\n", schemaFile);
struct tagSchema *schemaList = tagSchemaFromFile(schemaFile);
struct hash *schemaHash = tagSchemaHash(schemaList);
verbose(1, "Got %d items in tagSchema\n", schemaHash->elCount);

struct cdwUser *user = cdwMustGetUserFromEmail(conn, email);
uglyf("about to checkManifest\n");
checkManifestAndMetadata(table, fileIx, formatIx, metaIx, enrichedInIx,
    tagStorm, metaHash, schemaHash);
uglyf("done checkManifest\n");

/* Convert to data structure that has more fields.  If submission contains
 * replacement files, check that the accessions being replaced are legitimate. */
int md5Ix = stringArrayIx("md5", table->fields, table->fieldCount);
int replacesIx = stringArrayIx(replacesTag, table->fields, table->fieldCount);
int replaceReasonIx = stringArrayIx(replaceReasonTag, table->fields, table->fieldCount);
struct submitFileRow *sfrList = submitFileRowFromFieldedTable(conn, table, 
    fileIx, md5Ix, replacesIx, replaceReasonIx);
verbose(2, "Parsed manifest and metadata into %d files\n", slCount(sfrList));

/* Fake URL - system was built initially for remote files. */
char submitUrl[PATH_LEN];
safef(submitUrl, sizeof(submitUrl), "%s%s/%s", localPrefix, submitDir, manifestFile);
if (startsWith("/", manifestFile))
    errAbort("Please don't include full path to manifest file path.  This is no longer needed.");

/* Figure out directory ID for submission */
int hostId = cdwGetHost(conn, "localhost");
int submitDirId = cdwGetSubmitDir(conn, hostId, submitDir);

if (justTest)
    return;

/* Create a submission record */
int submitId = makeNewEmptySubmitRecord(conn, submitUrl, user->id);

/* Create a backup of the previous submission */
if (!noBackup)
    {
    char cmd[PATH_LEN];
    char *backupDir = cdwSetting(conn, "backup");
    if (backupDir != NULL)
	{
	safef(cmd, sizeof(cmd), "cdwBackup %s/cdwSubmit.%i", backupDir, submitId -1);  
	mustSystem(cmd); 
	}
    }

/* Put our manifest and metadata files */
char *access = tagFindLocalVal(tagStorm->forest, "access");
if (access == NULL)
    access = "group";

int manifestFileId= storeSubmissionFile(conn, manifestFile, submitId, submitDirId, user, access);
int metaFileId= storeSubmissionFile(conn, metaFile, submitId, submitDirId, user, access);

struct hash *metaIdHash = storeUsedMetaTags(conn, table, metaIx, tagStorm, metaHash);

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

	char *metaId = sfr->fr->row[metaIx];
	file->metaTagsId = hashIntValDefault(metaIdHash, metaId, 0);

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
	int oldFileId = findFileGivenMd5AndSubmitDir(conn, file->md5, file->submitFileName,
	    submitDirId);
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
    sqlUpdate(conn, query);

    /* Deal with old files. This may throw an error.  We do it before downloading new
     * files since we want to fail fast if we are going to fail. */
    int updateCount = handleOldFileTags(conn, metaIdHash, oldList, doUpdate, submitId);
    sqlSafef(query, sizeof(query), 
	"update cdwSubmit set metaChangeCount=%d where id=%u",  updateCount, submitId);
    sqlUpdate(conn, query);

    /* Go through list attempting to load the files if we don't already have them. */
    for (sfr = newList; sfr != NULL; sfr = sfr->next)
	{
	struct cdwFile *bf = sfr->file;
	char *format = sfr->fr->row[formatIx];
	getSubmittedFile(conn, format, tagStorm, metaHash, bf, 
	    submitDir, submitUrl, submitId, user, table, sfr->fr, metaIx);

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

verbose(1, "Got %d old files, %d new ones\n", oldCount, newCount);

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
noRevalidate = optionExists("noRevalidate");
noBackup = optionExists("noBackup");
justTest = optionExists("test");
if (optionExists("md5"))
    {
    char *md5File = optionVal("md5", NULL);
    md5Hash = md5FileHash(md5File);
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
