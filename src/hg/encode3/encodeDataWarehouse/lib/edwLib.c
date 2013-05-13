/* edwLib - routines shared by various encodeDataWarehouse programs.    See also encodeDataWarehouse
 * module for tables and routines to access structs built on tables. */

#include "common.h"
#include "hex.h"
#include "dystring.h"
#include "jksql.h"
#include "openssl/sha.h"
#include "base64.h"
#include "basicBed.h"
#include "bigBed.h"
#include "portable.h"
#include "cheapcgi.h"
#include "genomeRangeTree.h"
#include "md5.h"
#include "htmshell.h"
#include "obscure.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

/* System globals - just a few ... for now.  Please seriously not too many more. */
char *edwDatabase = "encodeDataWarehouse";
char *edwLicensePlatePrefix = "ENCFF";
int edwSingleFileTimeout = 2*60*60;   // How many seconds we give ourselves to fetch a single file

char *edwRootDir = "/data/encode3/encodeDataWarehouse/";
char *edwValDataDir = "/data/encode3/encValData/";

struct sqlConnection *edwConnect()
/* Returns a read only connection to database. */
{
return sqlConnect(edwDatabase);
}

struct sqlConnection *edwConnectReadWrite()
/* Returns read/write connection to database. */
{
return sqlConnectProfile("encodeDataWarehouse", edwDatabase);
}

char *edwPathForFileId(struct sqlConnection *conn, long long fileId)
/* Return full path (which eventually should be freeMem'd) for fileId */
{
char query[256];
char fileName[PATH_LEN];
safef(query, sizeof(query), "select edwFileName from edwFile where id=%lld", fileId);
sqlNeedQuickQuery(conn, query, fileName, sizeof(fileName));
char path[512];
safef(path, sizeof(path), "%s%s", edwRootDir, fileName);
return cloneString(path);
}

char *edwTempDir()
/* Returns pointer to edwTempDir.  This is shared, so please don't modify. */
{
static char path[PATH_LEN];
if (path[0] == 0)
    {
    /* Note code elsewhere depends on tmp dir being inside of edwRootDir - also good
     * to have it there so move to a permanent file is quick and unlikely to fail. */
    safef(path, sizeof(path), "%s%s", edwRootDir, "tmp");
    makeDirsOnPath(path);
    strcat(path, "/");
    }
return path;
}

char *edwTempDirForToday(char dir[PATH_LEN])
/* Fills in dir with temp dir of the day, and returns a pointer to it. */
{
char dayDir[PATH_LEN];
edwDirForTime(edwNow(), dayDir);
safef(dir, PATH_LEN, "%s%stmp/", edwRootDir, dayDir);

/* Bracket time consuming call to makeDirsOnPath with check that we didn't just do same
 * thing. */
static char lastDayDir[PATH_LEN] = "";
if (!sameString(dayDir, lastDayDir))
    {
    strcpy(lastDayDir, dayDir);
    int len = strlen(dir);
    dir[len-1] = 0;
    makeDirsOnPath(dir);
    dir[len-1] = '/';
    }
return dir;
}


long edwGettingFile(struct sqlConnection *conn, char *submitDir, char *submitFileName)
/* See if we are in process of getting file.  Return file record id if it exists even if
 * it's not complete. Return -1 if record does not exist. */
{
/* First see if we have even got the directory. */
char query[PATH_LEN+512];
safef(query, sizeof(query), "select id from edwSubmitDir where url='%s'", submitDir);
int submitDirId = sqlQuickNum(conn, query);
if (submitDirId <= 0)
    return -1;

/* Then see if we have file that matches submitDir and submitFileName. */
safef(query, sizeof(query), 
    "select id from edwFile "
    "where submitFileName='%s' and submitDirId = %d and errorMessage = '' and deprecated=''"
    " and (endUploadTime > startUploadTime or startUploadTime < %lld) "
    "order by submitId desc limit 1"
    , submitFileName, submitDirId
    , (long long)edwNow() - edwSingleFileTimeout);
long id = sqlQuickLongLong(conn, query);
if (id == 0)
    return -1;
return id;
}

long edwGotFile(struct sqlConnection *conn, char *submitDir, char *submitFileName, char *md5)
/* See if we already got file.  Return fileId if we do,  otherwise -1 */
{
/* First see if we have even got the directory. */
char query[PATH_LEN+512];
safef(query, sizeof(query), "select id from edwSubmitDir where url='%s'", submitDir);
int submitDirId = sqlQuickNum(conn, query);
if (submitDirId <= 0)
    return -1;

/* The complex truth is that we may have gotten this file multiple times. 
 * We return the most recent version where it got uploaded and passed the post-upload
 * MD5 sum, and thus where the MD5 field is filled in the database. */
safef(query, sizeof(query), 
    "select md5,id from edwFile "
    "where submitFileName='%s' and submitDirId = %d and md5 != '' "
    "order by submitId desc limit 1"
    , submitFileName, submitDirId);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
long fileId = -1;
if ((row = sqlNextRow(sr)) != NULL)
    {
    char *dbMd5 = row[0];
    if (sameWord(md5, dbMd5))
	fileId = sqlLongLong(row[1]);
    }
sqlFreeResult(&sr);

return fileId;
}

long long edwNow()
/* Return current time in seconds since Epoch. */
{
return time(NULL);
}

/* This is size of base64 encoded hash plus 1 for the terminating zero. */
#define EDW_SID_SIZE 65   

static void makeShaBase64(unsigned char *inputBuf, int inputSize, char out[EDW_SID_SIZE])
/* Make zero terminated printable cryptographic hash out of in */
{
unsigned char shaBuf[48];
SHA384(inputBuf, inputSize, shaBuf);
char *base64 = base64Encode((char*)shaBuf, sizeof(shaBuf));
memcpy(out, base64, EDW_SID_SIZE);
out[EDW_SID_SIZE-1] = 0; 
freeMem(base64);
}

void edwMakeSid(char *user, char sid[EDW_SID_SIZE])
/* Convert users to sid */
{
/* Salt it well with stuff that is reproducible but hard to guess */
unsigned char inputBuf[512];
memset(inputBuf, 0, sizeof(inputBuf));
int i;
for (i=0; i<ArraySize(inputBuf); i += 2)
    {
    inputBuf[i] = i ^ 0x29;
    inputBuf[i+1] = ~i;
    }
safef((char*)inputBuf, sizeof(inputBuf), 
	"186ED79BAEXzeusdioIsdklnw88e86cd73%s<*#$*(#)!DSDFOUIHLjksdf", user);
makeShaBase64(inputBuf, sizeof(inputBuf), sid);
}

static void edwVerifySid(char *user, char *sidToCheck)
/* Make sure sid/user combo is good. */
{
char sid[EDW_SID_SIZE];
edwMakeSid(user, sid);
if (sidToCheck == NULL || memcmp(sidToCheck, sid, EDW_SID_SIZE) != 0)
    errAbort("Authentication failed, sid %s", (sidToCheck ? "fail" : "miss"));
}

char *edwGetEmailAndVerify()
/* Get email from persona-managed cookies and validate them.
 * Return email address if all is good and user is logged in.
 * If user not logged in return NULL.  If user logged in but
 * otherwise things are wrong abort. */
{
char *email = findCookieData("email");
if (email)
    {
    char *sid = findCookieData("sid");
    edwVerifySid(email, sid);
    }
return email;
}


struct edwUser *edwUserFromEmail(struct sqlConnection *conn, char *email)
/* Return user associated with that email or NULL if not found */
{
char query[256];
char *escapedEmail = sqlEscapeString(email);
safef(query, sizeof(query), "select * from edwUser where email='%s'", escapedEmail);
freez(&escapedEmail);
struct edwUser *user = edwUserLoadByQuery(conn, query);
return user;
}

struct edwUser *edwMustGetUserFromEmail(struct sqlConnection *conn, char *email)
/* Return user associated with email or put up error message. */
{
struct edwUser *user = edwUserFromEmail(conn, email);
if (user == NULL)
    errAbort("No user exists with email %s.  If you need an account please contact your "
             "ENCODE DCC data wrangler, or someone you know who already does have an "
	     "ENCODE Data Warehouse account, and have them create an account for you with "
	     "http://%s/cgi-bin/edwWebCreateUser"
	     , email, getenv("SERVER_NAME"));
return user;
}

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

safef(query, sizeof(query), "insert edwHost (name, firstAdded, paraFetchStreams) values('%s', %lld, 10)", 
       hostName, edwNow());
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
   submitDir, edwNow(), hostId);
sqlUpdate(conn, query);
return sqlLastAutoId(conn);
}

void edwMakeLicensePlate(char *prefix, int ix, char *out, int outSize)
/* Make a license-plate type string composed of prefix + funky coding of ix
 * and put result in out. */
{
int maxIx = 10*10*10*26*26*26;
if (ix < 0)
    errAbort("ix must be positive in edwMakeLicensePlate");
if (ix > maxIx)
    errAbort("ix exceeds max in edwMakeLicensePlate.  ix %d, max %d\n", ix, maxIx);
int prefixSize = strlen(prefix);
int minSize = prefixSize + 6 + 1;
if (outSize < minSize)
    errAbort("outSize (%d) not big enough in edwMakeLicensePlate", outSize);

/* Copy in prefix. */
strcpy(out, prefix);

/* Generate the 123ABC part of license plate backwards. */
char *s = out+minSize;
int x = ix - 1;	// -1 so start with AAA not AAB
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

void edwDirForTime(time_t sinceEpoch, char dir[PATH_LEN])
/* Return the output directory for a given time. */
{
/* Get current time parsed into struct tm */
struct tm now;
gmtime_r(&sinceEpoch, &now);

/* make directory string out of year/month/day/ */
safef(dir, PATH_LEN, "%d/%d/%d/", now.tm_year+1900, now.tm_mon+1, now.tm_mday);
}

char *lastMatchCharExcept(char *start, char *end, char match, char except)
/* Return last char between start up to but not including end that is match.
 * However if except occurs between end and this match, return NULL instead.
 * Also return NULL if there is no match */
{
char *e = end;
while (--e >= start)
    {
    char c = *e;
    if (c == except)
       return NULL;
    if (c == match)
       return e;
    }
return NULL;
}

void edwMakeBabyName(unsigned long id, char *baseName, int baseNameSize)
/* Given a numerical ID, make an easy to pronouce file name */
{
char *consonants = "bdfghjklmnprstvwxyz";   // Avoid c and q because make sound ambiguous
char *vowels = "aeiou";
int consonantCount = strlen(consonants);
int vowelCount = strlen(vowels);
assert(id >= 1);
unsigned long ix = id - 1;   /* We start at zero not 1 */
int basePos = 0;
do
    {
    char v = vowels[ix%vowelCount];
    ix /= vowelCount;
    char c = consonants[ix%consonantCount];
    ix /= consonantCount;
    if (basePos + 2 >= baseNameSize)
        errAbort("Not enough room for %lu in %d letters in edwMakeBabyName", id, baseNameSize);
    baseName[basePos] = c;
    baseName[basePos+1] = v;
    basePos += 2;
    }
while (ix > 0);
baseName[basePos] = 0;
}

char *edwFindDoubleFileSuffix(char *path)
/* Return pointer to second from last '.' in part of path between last / and end.  
 * If there aren't two dots, just return pointer to normal single dot suffix. */
{
int nameSize = strlen(path);
char *suffix = lastMatchCharExcept(path, path + nameSize, '.', '/');
if (suffix != NULL)
    {
    char *secondSuffix = lastMatchCharExcept(path, suffix, '.', '/');
    if (secondSuffix != NULL)
        suffix = secondSuffix;
    }
else
    suffix = path + nameSize;
return suffix;
}

void edwMakeFileNameAndPath(int edwFileId, char *submitFileName, char edwFile[PATH_LEN], char serverPath[PATH_LEN])
/* Convert file id to local file name, and full file path. Make any directories needed
 * along serverPath. */
{
/* Preserve suffix.  Give ourselves up to two suffixes. */
char *suffix = edwFindDoubleFileSuffix(submitFileName);

/* Figure out edw file name, starting with baseName. */
char baseName[32];
edwMakeBabyName(edwFileId, baseName, sizeof(baseName));

/* Figure out directory and make any components not already there. */
char edwDir[PATH_LEN];
edwDirForTime(edwNow(), edwDir);
char uploadDir[PATH_LEN];
safef(uploadDir, sizeof(uploadDir), "%s%s", edwRootDir, edwDir);
makeDirsOnPath(uploadDir);

/* Figure out full file names */
safef(edwFile, PATH_LEN, "%s%s%s", edwDir, baseName, suffix);
safef(serverPath, PATH_LEN, "%s%s", edwRootDir, edwFile);
}

#ifdef OLD
void edwMakePlateFileNameAndPath(int edwFileId, char *submitFileName,
    char licensePlate[edwMaxPlateSize], char edwFile[PATH_LEN], char serverPath[PATH_LEN])
/* Convert file id to local file name, and full file path. Make any directories needed
 * along serverPath. */
{
/* Preserve suffix.  Give ourselves up to two suffixes. */
int nameSize = strlen(submitFileName);
char *suffix = lastMatchCharExcept(submitFileName, submitFileName + nameSize, '.', '/');
if (suffix != NULL)
    {
    char *secondSuffix = lastMatchCharExcept(submitFileName, suffix, '.', '/');
    if (secondSuffix != NULL)
        suffix = secondSuffix;
    }
suffix = emptyForNull(suffix);

/* Figure out edw file name, starting with license plate. */
edwMakeLicensePlate(edwLicensePlatePrefix, edwFileId, licensePlate, edwMaxPlateSize);

/* Figure out directory and make any components not already there. */
char edwDir[PATH_LEN];
edwDirForTime(edwNow(), edwDir);
char uploadDir[PATH_LEN];
safef(uploadDir, sizeof(uploadDir), "%s%s", edwRootDir, edwDir);
makeDirsOnPath(uploadDir);

/* Figure out full file names */
safef(edwFile, PATH_LEN, "%s%s%s", edwDir, licensePlate, suffix);
safef(serverPath, PATH_LEN, "%s%s", edwRootDir, edwFile);
}
#endif /* OLD */

static char *localHostName = "localhost";
static char *localHostDir = "";  

static int getLocalHost(struct sqlConnection *conn)
/* Make up record for local host if it is not there already. */
{
char query[256];
safef(query, sizeof(query), "select id from edwHost where name = '%s'", localHostName);
int hostId = sqlQuickNum(conn, query);
if (hostId == 0)
    {
    safef(query, sizeof(query), "insert edwHost(name, firstAdded) values('%s', %lld)",
	localHostName,  edwNow());
    sqlUpdate(conn, query);
    hostId = sqlLastAutoId(conn);
    }
return hostId;
}

static int getLocalSubmitDir(struct sqlConnection *conn)
/* Get submit dir for local submissions, making it up if it does not exist. */
{
int hostId = getLocalHost(conn);
char query[256];
safef(query, sizeof(query), "select id from edwSubmitDir where url='%s' and hostId=%d", 
    localHostDir, hostId);
int dirId = sqlQuickNum(conn, query);
if (dirId == 0)
    {
    safef(query, sizeof(query), "insert edwSubmitDir(url,hostId,firstAdded) values('%s',%d,%lld)",
	localHostDir, hostId, edwNow());
    sqlUpdate(conn, query);
    dirId = sqlLastAutoId(conn);
    }
return dirId;
}

static int getLocalSubmit(struct sqlConnection *conn)
/* Get the submission that covers all of our local additions. */
{
int dirId = getLocalSubmitDir(conn);
char query[256];
safef(query, sizeof(query), "select id from edwSubmit where submitDirId='%d'", dirId);
int submitId = sqlQuickNum(conn, query);
if (submitId == 0)
    {
    safef(query, sizeof(query), "insert edwSubmit (submitDirId,startUploadTime) values(%d,%lld)",
	dirId, edwNow());
    sqlUpdate(conn, query);
    submitId = sqlLastAutoId(conn);
    }
return submitId;
}

char **sqlNeedNextRow(struct sqlResult *sr)
/* Get next row or die trying.  Since the error reporting is not good, please only
 * use when an error would be unusual. */
{
char **row = sqlNextRow(sr);
if (row == NULL) 
    errAbort("Unexpected empty result from database.");
return row;
}

void edwUpdateFileTags(struct sqlConnection *conn, long long fileId, struct dyString *tags)
/* Update tags field in edwFile with given value */
{
struct dyString *query = dyStringNew(0);
dyStringAppend(query, "update edwFile set tags='");
dyStringAppend(query, tags->string);
dyStringPrintf(query, "' where id=%lld", fileId);
sqlUpdate(conn, query->string);
dyStringFree(&query);
}

struct edwFile *edwGetLocalFile(struct sqlConnection *conn, char *localAbsolutePath, 
    char *symLinkMd5Sum)
/* Get record of local file from database, adding it if it doesn't already exist.
 * Can make it a symLink rather than a copy in which case pass in valid MD5 sum
 * for symLinkM5dSum. */
{
/* First do a reality check on the local absolute path.  Is there a file there? */
if (localAbsolutePath[0] != '/')
    errAbort("Using relative path in edwAddLocalFile.");
long long size = fileSize(localAbsolutePath);
if (size == -1)
    errAbort("%s does not exist", localAbsolutePath);
long long updateTime = fileModTime(localAbsolutePath);

/* Get file if it's in database already. */
int submitDirId = getLocalSubmitDir(conn);
int submitId = getLocalSubmit(conn);
char query[256+PATH_LEN];
safef(query, sizeof(query), "select * from edwFile where submitId=%d and submitFileName='%s'",
    submitId, localAbsolutePath);
struct edwFile *ef = edwFileLoadByQuery(conn, query);

/* If we got something in database, check update time and size, and if it's no change just 
 * return existing database id. */
if (ef != NULL && ef->updateTime == updateTime && ef->size == size)
    return ef;

/* If we got here, then we need to make a new file record. Start with pretty empty record
 * that just has file ID, submitted file name and a few things*/
safef(query, sizeof(query), 
    "insert edwFile (submitId,submitDirId,submitFileName,startUploadTime) "
            " values(%d, %d, '%s', %lld)"
	    , submitId, submitDirId, localAbsolutePath, edwNow());
sqlUpdate(conn, query);
long long fileId = sqlLastAutoId(conn);

/* Create big data warehouse file/path name. */
char edwFile[PATH_LEN], edwPath[PATH_LEN];
edwMakeFileNameAndPath(fileId, localAbsolutePath, edwFile, edwPath);

/* We're a little paranoid so md5 it */
char *md5;

/* Do copy or symbolic linking of file into warehouse managed dir. */
if (symLinkMd5Sum)
    {
    md5 = symLinkMd5Sum;
    makeSymLink(localAbsolutePath, edwPath);  
    }
else
    {
    copyFile(localAbsolutePath, edwPath);
    md5 = md5HexForFile(localAbsolutePath);
    }

/* Update file record. */
safef(query, sizeof(query), 
    "update edwFile set edwFileName='%s', endUploadTime=%lld,"
                       "updateTime=%lld, size=%lld, md5='%s' where id=%lld"
			, edwFile, edwNow(), updateTime, size, md5, fileId);
sqlUpdate(conn, query);

/* Now, it's a bit of a time waste, but cheap in code, to just load it back from DB. */
safef(query, sizeof(query), "select * from edwFile where id=%lld", fileId);
return edwFileLoadByQuery(conn, query);
}

struct edwFile *edwFileAllIntactBetween(struct sqlConnection *conn, int startId, int endId)
/* Return list of all files that are intact (finished uploading and MD5 checked) 
 * with file IDs between startId and endId - including endId */
{
char query[128];
safef(query, sizeof(query), 
    "select * from edwFile where id>=%d and id<=%d and endUploadTime != 0 "
    "and updateTime != 0 and deprecated = ''", 
    startId, endId);
return edwFileLoadByQuery(conn, query);
}

struct edwFile *edwFileFromId(struct sqlConnection *conn, long long fileId)
/* Return edwValidFile given fileId - return NULL if not found. */
{
char query[128];
safef(query, sizeof(query), "select * from edwFile where id=%lld", fileId);
return edwFileLoadByQuery(conn, query);
}

struct edwFile *edwFileFromIdOrDie(struct sqlConnection *conn, long long fileId)
/* Return edwValidFile given fileId - aborts if not found. */
{
struct edwFile *ef = edwFileFromId(conn, fileId);
if (ef == NULL)
    errAbort("Couldn't find file for id %lld\n", fileId);
return ef;
}

struct edwValidFile *edwValidFileFromFileId(struct sqlConnection *conn, long long fileId)
/* Return edwValidFile give fileId - returns NULL if not validated. */
{
char query[128];
safef(query, sizeof(query), "select * from edwValidFile where fileId=%lld", fileId);
return edwValidFileLoadByQuery(conn, query);
}

struct genomeRangeTree *edwMakeGrtFromBed3List(struct bed3 *bedList)
/* Make up a genomeRangeTree around bed file. */
{
struct genomeRangeTree *grt = genomeRangeTreeNew();
struct bed3 *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    genomeRangeTreeAdd(grt, bed->chrom, bed->chromStart, bed->chromEnd);
return grt;
}

struct edwAssembly *edwAssemblyForUcscDb(struct sqlConnection *conn, char *ucscDb)
/* Get assembly for given UCSC ID or die trying */
{
char query[256];
safef(query, sizeof(query), "select * from edwAssembly where ucscDb='%s'", ucscDb);
struct edwAssembly *assembly = edwAssemblyLoadByQuery(conn, query);
if (assembly == NULL)
    errAbort("Can't find assembly for %s", ucscDb);
return assembly;
}

struct genomeRangeTree *edwGrtFromBigBed(char *fileName)
/* Return genome range tree for simple (unblocked) bed */
{
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
struct genomeRangeTree *grt = genomeRangeTreeNew();
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct rbTree *tree = genomeRangeTreeFindOrAddRangeTree(grt, chrom->name);
    struct lm *lm = lmInit(0);
    struct bigBedInterval *iv, *ivList = NULL;
    ivList = bigBedIntervalQuery(bbi, chrom->name, 0, chrom->size, 0, lm);
    for (iv = ivList; iv != NULL; iv = iv->next)
        rangeTreeAdd(tree, iv->start, iv->end);
    lmCleanup(&lm);
    }
bigBedFileClose(&bbi);
bbiChromInfoFreeList(&chromList);
return grt;
}

boolean edwIsSupportedBigBedFormat(char *format)
/* Return TRUE if it's one of the bigBed formats we support. */
{
return sameString(format, "broadPeak") || sameString(format, "narrowPeak") || 
	 sameString(format, "bedLogR") || sameString(format, "bigBed") ||
	 sameString(format, "bedRnaElements") || sameString(format, "bedRrbs") ||
	 sameString(format, "openChromCombinedPeaks") || sameString(format, "peptideMapping") ||
	 sameString(format, "shortFrags");
}

void edwWriteErrToTable(struct sqlConnection *conn, char *table, int id, char *err)
/* Write out error message to errorMessage field of table. */
{
char *trimmedError = trimSpaces(err);
char escapedErrorMessage[2*strlen(trimmedError)+1];
sqlEscapeString2(escapedErrorMessage, trimmedError);
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "update %s set errorMessage='%s' where id=%d", 
    table, escapedErrorMessage, id);
sqlUpdate(conn, query->string);
dyStringFree(&query);
}

void edwWriteErrToStderrAndTable(struct sqlConnection *conn, char *table, int id, char *err)
/* Write out error message to errorMessage field of table and through stderr. */
{
warn("%s", trimSpaces(err));
edwWriteErrToTable(conn, table, id, err);
}


void edwAddJob(struct sqlConnection *conn, char *command)
/* Add job to queue to run. */
{
char query[256+strlen(command)];
safef(query, sizeof(query), "insert into edwJob (commandLine) values('%s')", command);
sqlUpdate(conn, query);
}

void edwAddQaJob(struct sqlConnection *conn, long long fileId)
/* Create job to do QA on this and add to queue */
{
char command[64];
safef(command, sizeof(command), "edwQaAgent %lld", fileId);
edwAddJob(conn, command);
}

struct edwSubmit *edwMostRecentSubmission(struct sqlConnection *conn, char *url)
/* Return most recent submission, possibly in progress, from this url */
{
int urlSize = strlen(url);
char escapedUrl[2*urlSize+1];
sqlEscapeString2(escapedUrl, url);
int escapedSize = strlen(escapedUrl);
char query[128 + escapedSize];
safef(query, sizeof(query), 
    "select * from edwSubmit where url='%s' order by id desc limit 1", escapedUrl);
return edwSubmitLoadByQuery(conn, query);
}

long long edwSubmitMaxStartTime(struct edwSubmit *submit, struct sqlConnection *conn)
/* Figure out when we started most recent single file in the upload, or when
 * we started if not files started yet. */
{
char query[256];
safef(query, sizeof(query), 
    "select max(startUploadTime) from edwFile where submitId=%u", submit->id);
long long maxStartTime = sqlQuickLongLong(conn, query);
if (maxStartTime == 0)
    maxStartTime = submit->startUploadTime;
return maxStartTime;
}

int edwSubmitCountNewValid(struct edwSubmit *submit, struct sqlConnection *conn)
/* Count number of new files in submission that have been validated. */
{
char query[256];
safef(query, sizeof(query), 
    "select count(*) from edwFile e,edwValidFile v where e.id = v.fileId and e.submitId=%u",
    submit->id);
return sqlQuickNum(conn, query);
}

void edwAddSubmitJob(struct sqlConnection *conn, char *userEmail, char *url)
/* Add submission job to table and wake up daemon. */
{
/* Create command and add it to edwSubmitJob table. */
char command[strlen(url) + strlen(userEmail) + 256];
safef(command, sizeof(command), "edwSubmit '%s' %s", url, userEmail);
char escapedCommand[2*strlen(command) + 1];
sqlEscapeString2(escapedCommand, command);
char query[strlen(escapedCommand)+128];
safef(query, sizeof(query), "insert edwSubmitJob (commandLine) values('%s')", escapedCommand);
sqlUpdate(conn, query);

/* Write sync signal (any string ending with newline) to fifo to wake up daemon. */
FILE *fifo = mustOpen("../userdata/edwSubmit.fifo", "w");
fputc('\n', fifo);
carefulClose(&fifo);
}


struct edwValidFile *edwFindElderReplicates(struct sqlConnection *conn, struct edwValidFile *vf)
/* Find all replicates of same output and format type for experiment that are elder
 * (fileId less than your file Id).  Younger replicates are responsible for taking care 
 * of correlations with older ones.  Sorry younguns, it's like social security. */
{
if (sameString(vf->format, "unknown"))
    return NULL;
char query[256];
safef(query, sizeof(query), 
    "select * from edwValidFile where id<%d and experiment='%s' and format='%s'"
    " and outputType='%s'"
    , vf->id, vf->experiment, vf->format, vf->outputType);
return edwValidFileLoadByQuery(conn, query);
}

void edwWebHeaderWithPersona(char *title)
/* Print out HTTP and HTML header through <BODY> tag with persona info */
{
printf("Content-Type:text/html\r\n");
printf("\r\n\r\n");
puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" "
	      "\"http://www.w3.org/TR/html4/loose.dtd\">");
printf("<HTML><HEAD><TITLE>%s</TITLE>\n", title);
puts("<meta http-equiv='X-UA-Compatible' content='IE=Edge'>");
puts("<script type='text/javascript' SRC='/js/jquery.js'></script>");
puts("<script type='text/javascript' SRC='/js/jquery.cookie.js'></script>");
puts("<script type='text/javascript' src='https://login.persona.org/include.js'></script>");
puts("<script type='text/javascript' src='/js/edwPersona.js'></script>");
puts("</HEAD><BODY>");
}

void edwWebFooterWithPersona()
/* Print out end tags and persona script stuff */
{
htmlEnd();
}


void edwCreateNewUser(char *email)
/* Create new user, checking that user does not already exist. */
{
/* Now make sure user is not already in user table. */
struct sqlConnection *conn = edwConnectReadWrite();
struct dyString *query = dyStringNew(0);
char *escapedEmail = sqlEscapeString(email);
dyStringPrintf(query, "select count(*) from edwUser where email = '%s'", escapedEmail);
if (sqlQuickNum(conn, query->string) > 0)
    errAbort("User %s already exists", email);

/* Do database insert. */
dyStringClear(query);
dyStringPrintf(query, "insert into edwUser (email) values('%s')", escapedEmail);
sqlUpdate(conn, query->string);

sqlDisconnect(&conn);
}

void edwPrintLogOutButton()
/* Print log out button */
{
printf("<INPUT TYPE=button NAME=\"signOut\" VALUE=\"sign out\" id=\"signout\">");
}

struct dyString *edwFormatDuration(long long seconds)
/* Convert seconds to days/hours/minutes. Return result in a dyString you can free */
{
struct dyString *dy = dyStringNew(0);
int days = seconds/(3600*24);
if (days > 0)
    dyStringPrintf(dy, "%d days, ", days);
seconds -= days*3600*24;

int hours = seconds/3600;
if (hours > 0)
    dyStringPrintf(dy, "%d hours", hours);
seconds -= hours*3600;

if (days == 0)
    {
    int minutes = seconds/60;
    if (minutes > 0)
	{
	if (hours > 0)
	   dyStringPrintf(dy, ", ");
	dyStringPrintf(dy, "%d minutes", minutes);
	}

    if (hours == 0)
	{
	if (minutes > 0)
	   dyStringPrintf(dy, ", ");
	seconds -= minutes*60;
	dyStringPrintf(dy, "%d seconds", (int)seconds);
	}
    }
return dy;
}

