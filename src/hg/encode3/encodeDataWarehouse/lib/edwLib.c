/* edwLib - routines shared by various encodeDataWarehouse programs.    See also encodeDataWarehouse
 * module for tables and routines to access structs built on tables. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hex.h"
#include "dystring.h"
#include "jksql.h"
#include "errAbort.h"
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
#include "bamFile.h"
#include "raToStruct.h"
#include "web.h"
#include "encode3/encode3Valid.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "edwFastqFileFromRa.h"
#include "edwBamFileFromRa.h"
#include "edwQaWigSpotFromRa.h"


/* System globals - just a few ... for now.  Please seriously not too many more. */
char *edwDatabase = "encodeDataWarehouse";
int edwSingleFileTimeout = 4*60*60;   // How many seconds we give ourselves to fetch a single file

char *edwRootDir = "/data/encode3/encodeDataWarehouse/";
char *eapRootDir = "/data/encode3/encodeAnalysisPipeline/";
char *edwValDataDir = "/data/encode3/encValData/";
char *edwDaemonEmail = "edw@encodedcc.sdsc.edu";

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
sqlSafef(query, sizeof(query), "select edwFileName from edwFile where id=%lld", fileId);
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


long long edwGettingFile(struct sqlConnection *conn, char *submitDir, char *submitFileName)
/* See if we are in process of getting file.  Return file record id if it exists even if
 * it's not complete. Return -1 if record does not exist. */
{
/* First see if we have even got the directory. */
char query[PATH_LEN+512];
sqlSafef(query, sizeof(query), "select id from edwSubmitDir where url='%s'", submitDir);
int submitDirId = sqlQuickNum(conn, query);
if (submitDirId <= 0)
    return -1;

/* Then see if we have file that matches submitDir and submitFileName. */
sqlSafef(query, sizeof(query), 
    "select id from edwFile "
    "where submitFileName='%s' and submitDirId = %d and errorMessage = '' and deprecated=''"
    " and (endUploadTime > startUploadTime or startUploadTime < %lld) "
    "order by submitId desc limit 1"
    , submitFileName, submitDirId
    , (long long)edwNow() - edwSingleFileTimeout);
long long id = sqlQuickLongLong(conn, query);
if (id == 0)
    return -1;
return id;
}

long long edwGotFile(struct sqlConnection *conn, char *submitDir, char *submitFileName, 
    char *md5, long long size)
/* See if we already got file.  Return fileId if we do,  otherwise -1.  This returns
 * TRUE based mostly on the MD5sum.  For short files (less than 100k) then we also require
 * the submitDir and submitFileName to match.  This is to cover the case where you might
 * have legitimate empty files duplicated even though they were computed based on different
 * things. For instance coming up with no peaks is a legitimate result for many chip-seq
 * experiments. */
{
/* For large files just rely on MD5. */
char query[PATH_LEN+512];
if (size > 100000)
    {
    sqlSafef(query, sizeof(query),
        "select id from edwFile where md5='%s' order by submitId desc limit 1" , md5);
    long long result = sqlQuickLongLong(conn, query);
    if (result == 0)
        result = -1;
    return result;
    }

/* Rest of the routine deals with smaller files,  which we are less worried about
 * duplicating,  and indeed expect a little duplication of the empty file if none
 * other. */

/* First see if we have even got the directory. */
sqlSafef(query, sizeof(query), "select id from edwSubmitDir where url='%s'", submitDir);
int submitDirId = sqlQuickNum(conn, query);
if (submitDirId <= 0)
    return -1;

/* The complex truth is that we may have gotten this file multiple times. 
 * We return the most recent version where it got uploaded and passed the post-upload
 * MD5 sum, and thus where the MD5 field is filled in the database. */
sqlSafef(query, sizeof(query), 
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
sqlSafef(query, sizeof(query), "select * from edwUser where email='%s'", email);
struct edwUser *user = edwUserLoadByQuery(conn, query);
return user;
}

struct edwUser *edwUserFromId(struct sqlConnection *conn, int id)
/* Return user associated with that id or NULL if not found */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from edwUser where id='%d'", id);
struct edwUser *user = edwUserLoadByQuery(conn, query);
return user;
}

int edwUserIdFromFileId(struct sqlConnection *conn, int fId)
/* Return user id who submit the file originally */
{
char query[256];
sqlSafef(query, sizeof(query), "select s.userId from edwSubmit s, edwFile f where f.submitId=s.id and f.id='%d'", fId);
int sId = sqlQuickNum(conn, query);
sqlSafef(query, sizeof(query), "select u.id from edwSubmit s, edwUser u where  u.id=s.id and s.id='%d'", sId);
return sqlQuickNum(conn, query);
}

struct edwUser *edwFindUserFromFileId(struct sqlConnection *conn, int fId)
/* Return user who submit the file originally */
{
int uId = edwUserIdFromFileId(conn, fId);
struct edwUser *user=edwUserFromId(conn, uId);
return user; 
}

char *edwFindOwnerNameFromFileId(struct sqlConnection *conn, int fId)
/* Return name of submitter. Return "an unknown user" if name is NULL */
{
struct edwUser *owner = edwFindUserFromFileId(conn, fId);
if (owner == NULL)
    return ("an unknown user");
return cloneString(owner->email);
}

int edwFindUserIdFromEmail(struct sqlConnection *conn, char *userEmail)
/* Return true id of this user */
{
char query[256];
sqlSafef(query, sizeof(query), "select id from edwUser where email = '%s'", userEmail);
return sqlQuickNum(conn, query);
}

boolean edwUserIsAdmin(struct sqlConnection *conn, char *userEmail)
/* Return true if the user is an admin */
{
char query[256];
sqlSafef(query, sizeof(query), "select isAdmin from edwUser where email = '%s'", userEmail);
int isAdmin = sqlQuickNum(conn, query);
if (isAdmin == 1) return TRUE;
return FALSE;
}

void edwWarnUnregisteredUser(char *email)
/* Put up warning message about unregistered user and tell them how to register. */
{
warn("No user exists with email %s. If you need an account please contact your "
	 "ENCODE DCC data wrangler and have them create an account for you."
	 , email);
}


struct edwUser *edwMustGetUserFromEmail(struct sqlConnection *conn, char *email)
/* Return user associated with email or put up error message. */
{
struct edwUser *user = edwUserFromEmail(conn, email);
if (user == NULL)
    {
    edwWarnUnregisteredUser(email);
    noWarnAbort();
    }
return user;
}

int edwGetHost(struct sqlConnection *conn, char *hostName)
/* Look up host name in table and return associated ID.  If not found
 * make up new table entry. */
{
/* If it's already in table, just return ID. */
char query[512];
sqlSafef(query, sizeof(query), "select id from edwHost where name='%s'", hostName);
int hostId = sqlQuickNum(conn, query);
if (hostId > 0)
    return hostId;
sqlSafef(query, sizeof(query), "insert edwHost (name, firstAdded, paraFetchStreams) values('%s', %lld, 10)", 
       hostName, edwNow());
sqlUpdate(conn, query);
return sqlLastAutoId(conn);
}

int edwGetSubmitDir(struct sqlConnection *conn, int hostId, char *submitDir)
/* Get submitDir from database, creating it if it doesn't already exist. */
{
/* If it's already in table, just return ID. */
char query[512];
sqlSafef(query, sizeof(query), "select id from edwSubmitDir where url='%s'", submitDir);
int dirId = sqlQuickNum(conn, query);
if (dirId > 0)
    return dirId;

sqlSafef(query, sizeof(query), 
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
    if (sameString(suffix, ".gz") || sameString(suffix, ".bigBed"))
	{
	char *secondSuffix = lastMatchCharExcept(path, suffix, '.', '/');
	if (secondSuffix != NULL)
	    suffix = secondSuffix;
	}
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

char *edwSetting(struct sqlConnection *conn, char *name)
/* Return named settings value,  or NULL if setting doesn't exist. FreeMem when done. */
{
char query[256];
sqlSafef(query, sizeof(query), "select val from edwSettings where name='%s'", name);
return sqlQuickString(conn, query);
}

char *edwRequiredSetting(struct sqlConnection *conn, char *name)
/* Returns setting, abort if it isn't found. FreeMem when done. */
{
char *val = edwSetting(conn, name);
if (val == NULL)
    errAbort("Required %s setting is not defined in edwSettings table", name);
return val;
}

char *edwLicensePlateHead(struct sqlConnection *conn)
/* Return license plate prefix for current database - something like TSTFF or DEVFF or ENCFF */
{
static char head[32];
if (head[0] == 0)
     {
     char *prefix = edwRequiredSetting(conn, "prefix");
     safef(head, sizeof(head), "%s", prefix);
     }
return head;
}


static char *localHostName = "localhost";
static char *localHostDir = "";  

static int getLocalHost(struct sqlConnection *conn)
/* Make up record for local host if it is not there already. */
{
char query[256];
sqlSafef(query, sizeof(query), "select id from edwHost where name = '%s'", localHostName);
int hostId = sqlQuickNum(conn, query);
if (hostId == 0)
    {
    sqlSafef(query, sizeof(query), "insert edwHost(name, firstAdded) values('%s', %lld)",
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
sqlSafef(query, sizeof(query), "select id from edwSubmitDir where url='%s' and hostId=%d", 
    localHostDir, hostId);
int dirId = sqlQuickNum(conn, query);
if (dirId == 0)
    {
    sqlSafef(query, sizeof(query), "insert edwSubmitDir(url,hostId,firstAdded) values('%s',%d,%lld)",
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
sqlSafef(query, sizeof(query), "select id from edwSubmit where submitDirId='%d'", dirId);
int submitId = sqlQuickNum(conn, query);
if (submitId == 0)
    {
    sqlSafef(query, sizeof(query), "insert edwSubmit (submitDirId,startUploadTime) values(%d,%lld)",
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
sqlDyStringPrintf(query, "update edwFile set tags='%s'", tags->string);
sqlDyStringPrintf(query, " where id=%lld", fileId);
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
sqlSafef(query, sizeof(query), "select * from edwFile where submitId=%d and submitFileName='%s'",
    submitId, localAbsolutePath);
struct edwFile *ef = edwFileLoadByQuery(conn, query);

/* If we got something in database, check update time and size, and if it's no change just 
 * return existing database id. */
if (ef != NULL && ef->updateTime == updateTime && ef->size == size)
    return ef;

/* If we got here, then we need to make a new file record. Start with pretty empty record
 * that just has file ID, submitted file name and a few things*/
sqlSafef(query, sizeof(query), 
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
sqlSafef(query, sizeof(query), 
    "update edwFile set edwFileName='%s', endUploadTime=%lld,"
                       "updateTime=%lld, size=%lld, md5='%s' where id=%lld"
			, edwFile, edwNow(), updateTime, size, md5, fileId);
sqlUpdate(conn, query);

/* Now, it's a bit of a time waste, but cheap in code, to just load it back from DB. */
sqlSafef(query, sizeof(query), "select * from edwFile where id=%lld", fileId);
return edwFileLoadByQuery(conn, query);
}

struct edwFile *edwFileAllIntactBetween(struct sqlConnection *conn, int startId, int endId)
/* Return list of all files that are intact (finished uploading and MD5 checked) 
 * with file IDs between startId and endId - including endId */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select * from edwFile where id>=%d and id<=%d and endUploadTime != 0 "
    "and updateTime != 0 and errorMessage = '' and deprecated = ''", 
    startId, endId);
return edwFileLoadByQuery(conn, query);
}

struct edwFile *edwFileFromId(struct sqlConnection *conn, long long fileId)
/* Return edwValidFile given fileId - return NULL if not found. */
{
char query[128];
sqlSafef(query, sizeof(query), "select * from edwFile where id=%lld", fileId);
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
sqlSafef(query, sizeof(query), "select * from edwValidFile where fileId=%lld", fileId);
return edwValidFileLoadByQuery(conn, query);
}

struct edwExperiment *edwExperimentFromAccession(struct sqlConnection *conn, char *acc)
/* Given something like 'ENCSR123ABC' return associated experiment. */
{
char query[128];
sqlSafef(query, sizeof(query), "select * from edwExperiment where accession='%s'", acc);
return edwExperimentLoadByQuery(conn, query);
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
sqlSafef(query, sizeof(query), "select * from edwAssembly where ucscDb='%s'", ucscDb);
struct edwAssembly *assembly = edwAssemblyLoadByQuery(conn, query);
if (assembly == NULL)
    errAbort("Can't find assembly for %s", ucscDb);
return assembly;
}

struct edwAssembly *edwAssemblyForId(struct sqlConnection *conn, long long id)
/* Get assembly of given ID. */
{
char query[128];
sqlSafef(query, sizeof(query), "select * from edwAssembly where id=%lld", id);
struct edwAssembly *assembly = edwAssemblyLoadByQuery(conn, query);
if (assembly == NULL)
    errAbort("Can't find assembly for %lld", id);
return assembly;
}

char *edwSimpleAssemblyName(char *assembly)
/* Given compound name like male.hg19 return just hg19 */
/* Given name of assembly return name where we want to do enrichment calcs. */
{
/* If it ends with one of our common assembly suffix, then do enrichment calcs
 * in that space, rather than some subspace such as male, female, etc. */
static char *specialAsm[] = {".hg19",".hg38",".mm9",".mm10",".dm3",".ce10",".dm6"};
int i;
for (i=0; i<ArraySize(specialAsm); ++i)
    {
    char *special = specialAsm[i];
    if (endsWith(assembly, special))
        return special+1;
    }
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
int i;
for (i=0; i<encode3BedTypeCount; ++i)
    {
    if (sameString(format, encode3BedTypeTable[i].name))
        return TRUE;
    }
return FALSE;
}

void edwWriteErrToTable(struct sqlConnection *conn, char *table, int id, char *err)
/* Write out error message to errorMessage field of table. */
{
char *trimmedError = trimSpaces(err);
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "update %s set errorMessage='%s' where id=%d", 
    table, trimmedError, id);
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
sqlSafef(query, sizeof(query), "insert into edwJob (commandLine) values('%s')", command);
sqlUpdate(conn, query);
}

void edwAddQaJob(struct sqlConnection *conn, long long fileId)
/* Create job to do QA on this and add to queue */
{
char command[64];
safef(command, sizeof(command), "edwQaAgent %lld", fileId);
edwAddJob(conn, command);
}

int edwSubmitPositionInQueue(struct sqlConnection *conn, char *url, unsigned *retJobId)
/* Return position of our URL in submission queue.  Optionally return id in edwSubmitJob
 * table of job. */
{
char query[256];
sqlSafef(query, sizeof(query), "select id,commandLine from edwSubmitJob where startTime = 0");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
int aheadOfUs = -1;
int pos = 0;
unsigned jobId = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    jobId = sqlUnsigned(row[0]);
    char *line = row[1];
    char *edwSubmit = nextQuotedWord(&line);
    char *lineUrl = nextQuotedWord(&line);
    if (sameOk(edwSubmit, "edwSubmit") && sameOk(url, lineUrl))
        {
	aheadOfUs = pos;
	break;
	}
    ++pos;
    }
sqlFreeResult(&sr);
if (retJobId != NULL)
    *retJobId = jobId;
return aheadOfUs;
}

struct edwSubmit *edwSubmitFromId(struct sqlConnection *conn, long long id)
/* Return submission with given ID or NULL if no such submission. */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from edwSubmit where id=%lld", id);
return edwSubmitLoadByQuery(conn, query);
}


struct edwSubmit *edwMostRecentSubmission(struct sqlConnection *conn, char *url)
/* Return most recent submission, possibly in progress, from this url */
{
int urlSize = strlen(url);
char query[128 + 2*urlSize + 1];
sqlSafef(query, sizeof(query), 
    "select * from edwSubmit where url='%s' order by id desc limit 1", url);
return edwSubmitLoadByQuery(conn, query);
}

long long edwSubmitMaxStartTime(struct edwSubmit *submit, struct sqlConnection *conn)
/* Figure out when we started most recent single file in the upload, or when
 * we started if not files started yet. */
{
char query[256];
sqlSafef(query, sizeof(query), 
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
sqlSafef(query, sizeof(query), 
    "select count(*) from edwFile e,edwValidFile v where e.id = v.fileId and e.submitId=%u",
    submit->id);
return sqlQuickNum(conn, query);
}

int edwSubmitCountErrors(struct edwSubmit *submit, struct sqlConnection *conn)
/* Count number of errors with submitted files */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select count(*) from edwFile where submitId=%u and errorMessage != '' and errorMessage is not null",
    submit->id);
return sqlQuickNum(conn, query);
}

boolean edwSubmitIsValidated(struct edwSubmit *submit, struct sqlConnection *conn)
/* Return TRUE if validation has run.  This does not mean that they all passed validation.
 * It just means the validator has run and has made a decision on each file in the submission. */
{
/* Is this off by one because of the validated.txt being in the submission but never validated? */
return edwSubmitCountErrors(submit,conn) + edwSubmitCountNewValid(submit, conn) == submit->newFiles;
}

void edwAddSubmitJob(struct sqlConnection *conn, char *userEmail, char *url, boolean update)
/* Add submission job to table and wake up daemon. */
{
/* Create command and add it to edwSubmitJob table. */
char command[strlen(url) + strlen(userEmail) + 256];
safef(command, sizeof(command), "edwSubmit %s'%s' %s", (update ? "-update " : ""), url, userEmail);
char query[strlen(command)+128];
sqlSafef(query, sizeof(query), "insert edwSubmitJob (commandLine) values('%s')", command);
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
sqlSafef(query, sizeof(query), 
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
printf("<HTML><HEAD><TITLE>%s</TITLE>\n", "ENCODE Data Warehouse");
puts("<meta http-equiv='X-UA-Compatible' content='IE=Edge'>");

// Use Stanford ENCODE3 CSS for common look
puts("<link rel='stylesheet' href='/style/encode3.css' type='text/css'>");
puts("<link rel='stylesheet' href='/style/encode3Ucsc.css' type='text/css'>");
// external link icon (box with arrow) is from FontAwesome (fa-external-link)
puts("<link href='//netdna.bootstrapcdn.com/font-awesome/4.0.3/css/font-awesome.css' rel='stylesheet'>");

puts("<script type='text/javascript' SRC='/js/jquery.js'></script>");
puts("<script type='text/javascript' SRC='/js/jquery.cookie.js'></script>");
puts("<script type='text/javascript' src='https://login.persona.org/include.js'></script>");
puts("<script type='text/javascript' src='/js/edwPersona.js'></script>");
puts("</HEAD>");

/* layout with navigation bar */
puts("<BODY>\n");

edwWebNavBarStart();
}


void edwWebFooterWithPersona()
/* Print out end tags and persona script stuff */
{
edwWebNavBarEnd();
htmlEnd();
}


void edwCreateNewUser(char *email)
/* Create new user, checking that user does not already exist. */
{
/* Now make sure user is not already in user table. */
struct sqlConnection *conn = edwConnectReadWrite();
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "select count(*) from edwUser where email = '%s'", email);
if (sqlQuickNum(conn, query->string) > 0)
    errAbort("User %s already exists", email);

/* Do database insert. */
dyStringClear(query);
sqlDyStringPrintf(query, "insert into edwUser (email) values('%s')", email);
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
if (hours > 0 || days > 0)
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

struct edwFile *edwFileInProgress(struct sqlConnection *conn, int submitId)
/* Return file in submission in process of being uploaded if any. */
{
char query[256];
sqlSafef(query, sizeof(query), "select fileIdInTransit from edwSubmit where id=%u", submitId);
long long fileId = sqlQuickLongLong(conn, query);
if (fileId == 0)
    return NULL;
sqlSafef(query, sizeof(query), "select * from edwFile where id=%lld", (long long)fileId);
return edwFileLoadByQuery(conn, query);
}


static void accessDenied()
/* Sleep a bit and then deny access. */
{
sleep(5);
errAbort("Access denied!");
}

struct edwScriptRegistry *edwScriptRegistryFromCgi()
/* Get script registery from cgi variables.  Does authentication too. */
{
struct sqlConnection *conn = edwConnect();
char *user = sqlEscapeString(cgiString("user"));
char *password = sqlEscapeString(cgiString("password"));
char query[256];
sqlSafef(query, sizeof(query), "select * from edwScriptRegistry where name='%s'", user);
struct edwScriptRegistry *reg = edwScriptRegistryLoadByQuery(conn, query);
if (reg == NULL)
    accessDenied();
char key[EDW_SID_SIZE];
edwMakeSid(password, key);
if (!sameString(reg->secretHash, key))
    accessDenied();
sqlDisconnect(&conn);
return reg;
}

void edwValidFileUpdateDb(struct sqlConnection *conn, struct edwValidFile *el, long long id)
/* Save edwValidFile as a row to the table specified by tableName, replacing existing record at 
 * id. */
{
struct dyString *dy = newDyString(512);
sqlDyStringPrintf(dy, "update edwValidFile set ");
// omit id and licensePlate fields - one autoupdates and the other depends on this
// also omit fileId which also really can't change.
sqlDyStringPrintf(dy, " format='%s',", el->format);
sqlDyStringPrintf(dy, " outputType='%s',", el->outputType);
sqlDyStringPrintf(dy, " experiment='%s',", el->experiment);
sqlDyStringPrintf(dy, " replicate='%s',", el->replicate);
sqlDyStringPrintf(dy, " validKey='%s',", el->validKey);
sqlDyStringPrintf(dy, " enrichedIn='%s',", el->enrichedIn);
sqlDyStringPrintf(dy, " ucscDb='%s',", el->ucscDb);
sqlDyStringPrintf(dy, " itemCount=%lld,", (long long)el->itemCount);
sqlDyStringPrintf(dy, " basesInItems=%lld,", (long long)el->basesInItems);
sqlDyStringPrintf(dy, " sampleCount=%lld,", (long long)el->sampleCount);
sqlDyStringPrintf(dy, " basesInSample=%lld,", (long long)el->basesInSample);
sqlDyStringPrintf(dy, " sampleBed='%s',", el->sampleBed);
sqlDyStringPrintf(dy, " mapRatio=%g,", el->mapRatio);
sqlDyStringPrintf(dy, " sampleCoverage=%g,", el->sampleCoverage);
sqlDyStringPrintf(dy, " depth=%g,", el->depth);
sqlDyStringPrintf(dy, " singleQaStatus=0,");
sqlDyStringPrintf(dy, " replicateQaStatus=0,");
sqlDyStringPrintf(dy, " technicalReplicate='%s',", el->technicalReplicate);
sqlDyStringPrintf(dy, " pairedEnd='%s',", el->pairedEnd);
sqlDyStringPrintf(dy, " qaVersion='%d',", el->qaVersion);
sqlDyStringPrintf(dy, " uniqueMapRatio=%g", el->uniqueMapRatio);
#if (EDWVALIDFILE_NUM_COLS != 24)
   #error "Please update this routine with new column"
#endif
sqlDyStringPrintf(dy, " where id=%lld\n", (long long)id);
sqlUpdate(conn, dy->string);
freeDyString(&dy);
}

static char *findTagOrEmpty(struct cgiParsedVars *tags, char *key)
/* Find key in tags.  If it is not there, or empty, or 'n/a' valued return empty string
 * otherwise return val */
{
char *val = hashFindVal(tags->hash, key);
if (val == NULL || sameString(val, "n/a"))
   return "";
else
   return val;
}

void edwValidFileFieldsFromTags(struct edwValidFile *vf, struct cgiParsedVars *tags)
/* Fill in many of vf's fields from tags. */
{
vf->format = cloneString(hashFindVal(tags->hash, "format"));
vf->outputType = cloneString(findTagOrEmpty(tags, "output_type"));
vf->experiment = cloneString(findTagOrEmpty(tags, "experiment"));
vf->replicate = cloneString(findTagOrEmpty(tags, "replicate"));
vf->validKey = cloneString(hashFindVal(tags->hash, "valid_key"));
vf->enrichedIn = cloneString(findTagOrEmpty(tags, "enriched_in"));
vf->ucscDb = cloneString(findTagOrEmpty(tags, "ucsc_db"));
vf->technicalReplicate = cloneString(findTagOrEmpty(tags, "technical_replicate"));
vf->pairedEnd = cloneString(findTagOrEmpty(tags, "paired_end"));
#if (EDWVALIDFILE_NUM_COLS != 24)
   #error "Please update this routine with new column"
#endif
}

void edwFileResetTags(struct sqlConnection *conn, struct edwFile *ef, char *newTags, 
    boolean revalidate)
/* Reset tags on file, strip out old validation and QA,  schedule new validation and QA. */
/* Remove existing QA records and rerun QA agent on given file.   */
{
long long fileId = ef->id;
/* Update database to let people know format revalidation is in progress. */
char query[4*1024];

/* Update tags for file in edwFile table. */
sqlSafef(query, sizeof(query), "update edwFile set tags='%s' where id=%lld", newTags, fileId);
sqlUpdate(conn, query);
    
if (revalidate)
    {
    sqlSafef(query, sizeof(query), "update edwFile set errorMessage = '%s' where id=%lld",
	 "Revalidation in progress.", fileId); 
    sqlUpdate(conn, query);

    /* Get rid of records referring to file in other validation and qa tables. */
    sqlSafef(query, sizeof(query), "delete from edwFastqFile where fileId=%lld", fileId);
    sqlUpdate(conn, query);
    sqlSafef(query, sizeof(query),
	"delete from edwQaPairSampleOverlap where elderFileId=%lld or youngerFileId=%lld",
	fileId, fileId);
    sqlUpdate(conn, query);
    sqlSafef(query, sizeof(query),
	"delete from edwQaPairCorrelation where elderFileId=%lld or youngerFileId=%lld",
	fileId, fileId);
    sqlUpdate(conn, query);
    sqlSafef(query, sizeof(query), "delete from edwQaEnrich where fileId=%lld", fileId);
    sqlUpdate(conn, query);
    sqlSafef(query, sizeof(query), "delete from edwQaContam where fileId=%lld", fileId);
    sqlUpdate(conn, query);
    sqlSafef(query, sizeof(query), "delete from edwQaRepeat where fileId=%lld", fileId);
    sqlUpdate(conn, query);
    sqlSafef(query, sizeof(query), 
	"delete from edwQaPairedEndFastq where fileId1=%lld or fileId2=%lld",
	fileId, fileId);
    sqlUpdate(conn, query);

    /* schedule validator */
    edwAddQaJob(conn, ef->id);
    }
else
    {
    /* The revalidation case relies on edwMakeValidFile to update the edwValidFile table.
     * Here we must do it ourselves. */
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
    struct cgiParsedVars *tags = cgiParsedVarsNew(newTags);
    edwValidFileFieldsFromTags(vf, tags);
    edwValidFileUpdateDb(conn, vf, vf->id);
    cgiParsedVarsFree(&tags);
    edwValidFileFree(&vf);
    }
}

static void scanSam(char *samIn, FILE *f, struct genomeRangeTree *grt, long long *retHit, 
    long long *retMiss,  long long *retTotalBasesInHits, long long *retUniqueHitCount)
/* Scan through sam file doing several things:counting how many reads hit and how many 
 * miss target during mapping phase, copying those that hit to a little bed file, and 
 * also defining regions covered in a genomeRangeTree. */
{
samfile_t *sf = samopen(samIn, "r", NULL);
bam_hdr_t *bamHeader = sam_hdr_read(sf);
bam1_t one;
ZeroVar(&one);
int err;
long long hit = 0, miss = 0, unique = 0, totalBasesInHits = 0;
while ((err = sam_read1(sf, bamHeader, &one)) >= 0)
    {
    int32_t tid = one.core.tid;
    if (tid < 0)
	{
	++miss;
        continue;
	}
    ++hit;
    if (one.core.qual > edwMinMapQual)
        ++unique;
    char *chrom = bamHeader->target_name[tid];
    // Approximate here... can do better if parse cigar.
    int start = one.core.pos;
    int size = one.core.l_qseq;
    int end = start + size;	
    totalBasesInHits += size;
    boolean isRc = (one.core.flag & BAM_FREVERSE);
    char strand = (isRc ? '-' : '+');
    if (start < 0) start=0;
    if (f != NULL)
	fprintf(f, "%s\t%d\t%d\t.\t0\t%c\n", chrom, start, end, strand);
    genomeRangeTreeAdd(grt, chrom, start, end);
    }
if (err < 0 && err != -1)
    errnoAbort("samread err %d", err);
samclose(sf);
*retHit = hit;
*retMiss = miss;
*retTotalBasesInHits = totalBasesInHits;
*retUniqueHitCount = unique;
}

void edwReserveTempFile(char *path)
/* Call mkstemp on path.  This will fill in terminal XXXXXX in path with file name
 * and create an empty file of that name.  Generally that empty file doesn't stay empty for long. */
{
int fd = mkstemp(path);
if (fd == -1)
     errnoAbort("Couldn't create temp file %s", path);
mustCloseFd(&fd);
}

void edwBwaIndexPath(struct edwAssembly *assembly, char indexPath[PATH_LEN])
/* Fill in path to BWA index. */
{
safef(indexPath, PATH_LEN, "%s%s/bwaData/%s.fa", 
    edwValDataDir, assembly->ucscDb, assembly->ucscDb);
}

void edwAsPath(char *format, char path[PATH_LEN])
/* Convert something like "narrowPeak" in format to full path involving
 * encValDir/as/narrowPeak.as */
{
safef(path, PATH_LEN, "%sas/%s.as", edwValDataDir, format);
}

void edwAlignFastqMakeBed(struct edwFile *ef, struct edwAssembly *assembly,
    char *fastqPath, struct edwValidFile *vf, FILE *bedF,
    double *retMapRatio,  double *retDepth,  double *retSampleCoverage, 
    double *retUniqueMapRatio)
/* Take a sample fastq and run bwa on it, and then convert that file to a bed. 
 * bedF and all the ret parameters can be NULL. */
{
/* Hmm, tried doing this with Mark's pipeline code, but somehow it would be flaky the
 * second time it was run in same app.  Resorting therefore to temp files. */
char genoFile[PATH_LEN];
edwBwaIndexPath(assembly, genoFile);

char cmd[3*PATH_LEN];
char *saiName = cloneString(rTempName(edwTempDir(), "edwSample1", ".sai"));
safef(cmd, sizeof(cmd), "bwa aln -t 3 %s %s > %s", genoFile, fastqPath, saiName);
mustSystem(cmd);

char *samName = cloneString(rTempName(edwTempDir(), "ewdSample1", ".sam"));
safef(cmd, sizeof(cmd), "bwa samse %s %s %s > %s", genoFile, saiName, fastqPath, samName);
mustSystem(cmd);
remove(saiName);

/* Scan sam file to calculate vf->mapRatio, vf->sampleCoverage and vf->depth. 
 * and also to produce little bed file for enrichment step. */
struct genomeRangeTree *grt = genomeRangeTreeNew();
long long hitCount=0, missCount=0, uniqueHitCount=0, totalBasesInHits=0;
scanSam(samName, bedF, grt, &hitCount, &missCount, &totalBasesInHits, &uniqueHitCount);
verbose(1, "hitCount=%lld, missCount=%lld, totalBasesInHits=%lld, grt=%p\n", 
    hitCount, missCount, totalBasesInHits, grt);
if (retMapRatio)
    *retMapRatio = (double)hitCount/(hitCount+missCount);
if (retDepth)
    *retDepth = (double)totalBasesInHits/assembly->baseCount 
	    * (double)vf->itemCount/vf->sampleCount;
long long basesHitBySample = genomeRangeTreeSumRanges(grt);
if (retSampleCoverage)
    *retSampleCoverage = (double)basesHitBySample/assembly->baseCount;
if (retUniqueMapRatio)
    *retUniqueMapRatio = (double)uniqueHitCount/(hitCount+missCount);
genomeRangeTreeFree(&grt);
remove(samName);
}

struct edwFastqFile *edwFastqFileFromFileId(struct sqlConnection *conn, long long fileId)
/* Get edwFastqFile with given fileId or NULL if none such */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from edwFastqFile where fileId=%lld", fileId);
return edwFastqFileLoadByQuery(conn, query);
}

static int mustMkstemp(char *template)
/* Call mkstemp to make a temp file with name based on template (which is altered)
 * by the call to be the file name.   Return unix file descriptor. */
{
int fd = mkstemp(template);
if (fd == -1)
    errnoAbort("Couldn't make temp file based on %s", template);
return fd;
}

void edwMakeTempFastqSample(char *source, int size, char dest[PATH_LEN])
/* Copy size records from source into a new temporary dest.  Fills in dest */
{
/* Make temporary file to save us a unique place in file system. */
safef(dest, PATH_LEN, "%sedwSampleFastqXXXXXX", edwTempDir());
int fd = mustMkstemp(dest);
close(fd);

char command[3*PATH_LEN];
safef(command, sizeof(command), 
    "fastqStatsAndSubsample %s /dev/null %s -smallOk -sampleSize=%d", source, dest, size);
verbose(2, "command: %s\n", command);
mustSystem(command);
}

void edwMakeFastqStatsAndSample(struct sqlConnection *conn, long long fileId)
/* Run fastqStatsAndSubsample, and put results into edwFastqFile table. */
{
struct edwFastqFile *fqf = edwFastqFileFromFileId(conn, fileId);
if (fqf == NULL)
    {
    char *path = edwPathForFileId(conn, fileId);
    char statsFile[PATH_LEN], sampleFile[PATH_LEN];
    safef(statsFile, PATH_LEN, "%sedwFastqStatsXXXXXX", edwTempDir());
    edwReserveTempFile(statsFile);
    char dayTempDir[PATH_LEN];
    safef(sampleFile, PATH_LEN, "%sedwFastqSampleXXXXXX", edwTempDirForToday(dayTempDir));
    edwReserveTempFile(sampleFile);
    char command[3*PATH_LEN];
    safef(command, sizeof(command), "fastqStatsAndSubsample -sampleSize=%d -smallOk %s %s %s",
	edwSampleTargetSize, path, statsFile, sampleFile);
    mustSystem(command);
    safef(command, sizeof(command), "gzip %s", sampleFile);
    mustSystem(command);
    strcat(sampleFile, ".gz");
    fqf = edwFastqFileOneFromRa(statsFile);
    fqf->fileId = fileId;
    fqf->sampleFileName = cloneString(sampleFile);
    edwFastqFileSaveToDb(conn, fqf, "edwFastqFile", 1024);
    remove(statsFile);
    freez(&path);
    }
edwFastqFileFree(&fqf);
}

struct edwQaWigSpot *edwMakeWigSpot(struct sqlConnection *conn, long long wigId, long long spotId)
/* Create a new edwQaWigSpot record in database based on comparing wig file to spot file
 * (specified by id's in edwFile table). */
{
/* Get valid files from fileIds and check format */
struct edwValidFile *wigVf = edwValidFileFromFileId(conn, wigId);
if (!sameString(wigVf->format, "bigWig"))
    errAbort("%lld is not a bigWig file, is %s instead", wigId, wigVf->format);
struct edwValidFile *spotVf = edwValidFileFromFileId(conn, spotId);
if (!sameString(spotVf->format, "narrowPeak") && !sameString(spotVf->format, "broadPeak") &&
    !sameString(spotVf->format, "bigBed"))
    errAbort("%lld is not a recognized peak type format, is %s", spotId, spotVf->format);

/* Remove any old record for files. */
char query[256];
sqlSafef(query, sizeof(query), 
    "delete from edwQaWigSpot where wigId=%lld and spotId=%lld", wigId, spotId);
sqlUpdate(conn, query);

/* Figure out file names */
char *wigPath = edwPathForFileId(conn, wigId);
char *spotPath = edwPathForFileId(conn, spotId);
char statsFile[PATH_LEN];
safef(statsFile, PATH_LEN, "%sedwQaWigSpotXXXXXX", edwTempDir());
edwReserveTempFile(statsFile);
char peakFile[PATH_LEN];
safef(peakFile, PATH_LEN, "%sedwQaWigSpotXXXXXX", edwTempDir());
edwReserveTempFile(peakFile);

/* Convert narrowPeak input into a temporary bed4 file */
char command[3*PATH_LEN];
safef(command, sizeof(command), "bigBedToBed %s stdout | cut -f 1-4 > %s", spotPath, peakFile);
mustSystem(command);

/* Call on bigWigAverageOverBed on peaks */
safef(command, sizeof(command), 
    "bigWigAverageOverBed %s %s /dev/null -stats=%s", wigPath, peakFile, statsFile);
mustSystem(command);
remove(peakFile);

/* Parse out ra file,  save it to database, and remove ra file. */
struct edwQaWigSpot *spot = edwQaWigSpotOneFromRa(statsFile);
spot->wigId = wigId;
spot->spotId = spotId;
edwQaWigSpotSaveToDb(conn, spot, "edwQaWigSpot", 1024);
spot->id = sqlLastAutoId(conn);

/* Clean up and go home. */
edwQaWigSpotFree(&spot);
edwValidFileFree(&wigVf);
edwValidFileFree(&spotVf);
freez(&wigPath);
freez(&spotPath);
return spot;
}

struct edwQaWigSpot *edwQaWigSpotFor(struct sqlConnection *conn, 
    long long wigFileId, long long spotFileId) 
/* Return wigSpot relationship if any we have in database for these two files. */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select * from edwQaWigSpot where wigId=%lld and spotId=%lld", wigFileId, spotFileId);
return edwQaWigSpotLoadByQuery(conn, query);
}




struct edwBamFile *edwBamFileFromFileId(struct sqlConnection *conn, long long fileId)
/* Get edwBamFile with given fileId or NULL if none such */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from edwBamFile where fileId=%lld", fileId);
return edwBamFileLoadByQuery(conn, query);
}

struct edwBamFile * edwMakeBamStatsAndSample(struct sqlConnection *conn, long long fileId, 
    char sampleBed[PATH_LEN])
/* Run edwBamStats and put results into edwBamFile table, and also a sample bed.
 * The sampleBed will be filled in by this routine. */
{
/* Remove any old record for file. */
char query[256];
sqlSafef(query, sizeof(query), "delete from edwBamFile where fileId=%lld", fileId);
sqlUpdate(conn, query);

/* Figure out file names */
char *path = edwPathForFileId(conn, fileId);
char statsFile[PATH_LEN];
safef(statsFile, PATH_LEN, "%sedwBamStatsXXXXXX", edwTempDir());
edwReserveTempFile(statsFile);
char dayTempDir[PATH_LEN];
safef(sampleBed, PATH_LEN, "%sedwBamSampleXXXXXX", edwTempDirForToday(dayTempDir));
edwReserveTempFile(sampleBed);

/* Make system call to make ra and bed, and then another system call to zip bed.*/
char command[3*PATH_LEN];
safef(command, sizeof(command), "edwBamStats -sampleBed=%s -sampleBedSize=%d %s %s",
    sampleBed, edwSampleTargetSize, path, statsFile);
mustSystem(command);
safef(command, sizeof(command), "gzip %s", sampleBed);
mustSystem(command);
strcat(sampleBed, ".gz");

/* Parse out ra file,  save it to database, and remove ra file. */
struct edwBamFile *ebf = edwBamFileOneFromRa(statsFile);
ebf->fileId = fileId;
edwBamFileSaveToDb(conn, ebf, "edwBamFile", 1024);
remove(statsFile);

/* Clean up and go home. */
freez(&path);
return ebf;
}


char *edwOppositePairedEndString(char *end)
/* Return "1" for "2" and vice versa */
{
if (sameString(end, "1"))
    return "2";
else if (sameString(end, "2"))
    return "1";
else
    {
    errAbort("Expecting 1 or 2, got %s in oppositeEnd", end);
    return NULL;
    }
}

struct edwValidFile *edwOppositePairedEnd(struct sqlConnection *conn, struct edwValidFile *vf)
/* Given one file of a paired end set of fastqs, find the file with opposite ends. */
{
char *otherEnd = edwOppositePairedEndString(vf->pairedEnd);
char query[1024];
sqlSafef(query, sizeof(query), 
    "select edwValidFile.* from edwValidFile join edwFile on edwValidFile.fileId=edwFile.id"
    " where experiment='%s' and outputType='%s' and replicate='%s' "
    " and technicalReplicate='%s' and pairedEnd='%s' and itemCount=%lld and deprecated=''"
    , vf->experiment, vf->outputType, vf->replicate, vf->technicalReplicate, otherEnd
    , vf->itemCount);
struct edwValidFile *otherVf = edwValidFileLoadByQuery(conn, query);
if (otherVf == NULL)
    return NULL;
if (otherVf->next != NULL)
    errAbort("Multiple results from pairedEnd query %s", query);
return otherVf;
}

struct edwQaPairedEndFastq *edwQaPairedEndFastqFromVfs(struct sqlConnection *conn,
    struct edwValidFile *vfA, struct edwValidFile *vfB,
    struct edwValidFile **retVf1,  struct edwValidFile **retVf2)
/* Return pair record if any for the two fastq files. */
{
/* Sort the two ends. */
struct edwValidFile *vf1 = NULL, *vf2 = NULL;
if (sameString(vfA->pairedEnd, "1"))
    {
    vf1 = vfA;
    vf2 = vfB;
    }
else
    {
    vf1 = vfB;
    vf2 = vfA;
    }
if (retVf1 != NULL)
   *retVf1 = vf1;
if (retVf2 != NULL)
   *retVf2 = vf2;

/* See if we already have a record for these two. */
/* Return record for these two. */
char query[1024];
sqlSafef(query, sizeof(query), 
    "select * from edwQaPairedEndFastq where fileId1=%u and fileId2=%u",
    vf1->fileId, vf2->fileId);
return edwQaPairedEndFastqLoadByQuery(conn, query);
}

FILE *edwPopen(char *command, char *mode)
/* do popen or die trying */
{
/* Because of bugs with popen(...,"r") and programs that use stdin otherwise
 * it's probably better to use Mark's pipeline library,  but it is ever so
 * much harder to use... */
FILE *f = popen(command,  mode);
if (f == NULL)
    errnoAbort("Can't popen(%s, %s)", command, mode);
return f;
}

boolean edwOneLineSystemAttempt(char *command, char *line, int maxLineSize)
/* Execute system command and return one line result from it in line */
{
FILE *f = popen(command, "r");
boolean ok = FALSE;
if (f != NULL)
    {
    char *result  = fgets(line, maxLineSize, f);
    if (result != NULL)
	ok = TRUE;
    pclose(f);
    }
else
    {
    errnoWarn("failed popen %s", command);
    }
return ok;
}

void edwOneLineSystemResult(char *command, char *line, int maxLineSize)
/* Execute system command and return one line result from it in line */
{
if (!edwOneLineSystemAttempt(command, line, maxLineSize) )
    errAbort("Can't get line from %s", command);
}

void edwMd5File(char *fileName, char md5Hex[33])
/* call md5sum utility to calculate md5 for file and put result in hex format md5Hex 
 * This ends up being about 30% faster than library routine md5HexForFile,
 * however since there's popen() weird interactions with  stdin involved
 * it's not suitable for a general purpose library.  Environment inside edw
 * is controlled enough it should be ok. */
{
char command[PATH_LEN + 16];
safef(command, sizeof(command), "md5sum %s", fileName);
char line[2*PATH_LEN];
edwOneLineSystemResult(command, line, sizeof(line));
memcpy(md5Hex, line, 32);
md5Hex[32] = 0;
}


void edwPokeFifo(char *fifoName)
/* Send '\n' to fifo to wake up associated daemon */
{
/* Sadly we loop through places it might be since it varies. It has to live somewhere
 * that web CGIs can poke is the problem. */
char *places[] = {"/data/www/userdata/", "/usr/local/apache/userdata/"};
int i;
for (i=0; i<ArraySize(places); ++i)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", places[i], fifoName);
    if (fileExists(path))
        {
	char *message = "\n";
	writeGulp(path, message, strlen(message));
	break;
	}
    }
}

/***/
/* Shared functions for EDW web CGI's.
   Mostly wrappers for javascript tweaks */

void edwWebAutoRefresh(int msec)
/* Refresh page after msec.  Use 0 to cancel autorefresh */
{
if (msec > 0)
    {
    // set timeout to refresh page (saving/restoring scroll position via cookie)
    printf("<script type='text/javascript'>var edwRefresh = setTimeout(function() { $.cookie('edwWeb.scrollTop', $(window).scrollTop()); $('form').submit(); }, %d);</script>", msec);
    puts("<script type='text/javascript'>$(document).ready(function() {$(document).scrollTop($.cookie('edwWeb.scrollTop'))});</script>");

    // disable autorefresh when user is changing page settings
    puts("<script type='text/javascript'>$('form').click(function() {clearTimeout(edwRefresh); $.cookie('edwWeb.scrollTop', null);});</script>");
    }
else if (msec == 0)
    puts("clearTimeout(edwRefresh);</script>");

// Negative msec ignored
}

/***/
/* Navigation bar */

void edwWebNavBarStart()
/* Layout navigation bar */
{
puts("<div id='layout'>");
puts("<div id='navbar' class='navbar navbar-fixed-top navbar-inverse'>");
webIncludeFile("/inc/edwNavBar.html");
puts("</div>");
puts("<div id='content' class='container'><div>");
}

void edwWebNavBarEnd()
/* Close layout after navigation bar */
{
puts("</div></div></div>");
}

void edwWebBrowseMenuItem(boolean on)
/* Toggle visibility of 'Browse submissions' link on navigation menu */
{
printf("<script type='text/javascript'>$('#edw-browse').%s();</script>", on ? "show" : "hide");
}

void edwWebSubmitMenuItem(boolean on)
/* Toggle visibility of 'Submit data' link on navigation menu */
{
printf("<script type='text/javascript'>$('#edw-submit').%s();</script>", on ? "show" : "hide");
}

