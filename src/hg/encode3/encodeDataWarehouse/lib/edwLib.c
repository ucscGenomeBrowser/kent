/* edwLib - routines shared by various encodeDataWarehouse programs.    See also encodeDataWarehouse
 * module for tables and routines to access structs built on tables. */

#include "common.h"
#include "edwLib.h"
#include "hex.h"
#include "openssl/sha.h"
#include "base64.h"
#include "portable.h"
#include "md5.h"
#include "obscure.h"

/* System globals - just a few ... for now.  Please seriously not too many more. */
char *edwDatabase = "encodeDataWarehouse";
char *edwRootDir = "/scratch/kent/encodeDataWarehouse/";
char *edwLicensePlatePrefix = "ENCFF";

char *edwTempDir()
/* Returns pointer to edwTempDir.  This is shared, so please don't modify. */
{
static char path[PATH_LEN];
if (path[0] == 0)
    {
    safef(path, sizeof(path), "%s%s", edwRootDir, "tmp");
    makeDirsOnPath(path);
    strcat(path, "/");
    }
return path;
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

static void makeShaBase64(unsigned char *inputBuf, int inputSize, char out[EDW_ACCESS_SIZE])
/* Make zero terminated printable cryptographic hash out of in */
{
unsigned char shaBuf[48];
SHA384(inputBuf, inputSize, shaBuf);
char *base64 = base64Encode((char*)shaBuf, sizeof(shaBuf));
memcpy(out, base64, EDW_ACCESS_SIZE);
out[EDW_ACCESS_SIZE-1] = 0; 
freeMem(base64);
}

void edwMakeAccess(char *password, char access[EDW_ACCESS_SIZE])
/* Convert password + salt to an access code */
{
/* Salt it well with stuff that is reproducible but hard to guess, and some
 * one time true random stuff. Sneak in password too. */
unsigned char inputBuf[512];
memset(inputBuf, 0, sizeof(inputBuf));
int i;
for (i=0; i<ArraySize(inputBuf); i += 2)
    {
    inputBuf[i] = i ^ 0x3f;
    inputBuf[i+1] = -i*i;
    }
safef((char*)inputBuf, sizeof(inputBuf), 
    "f186ed79bae8MNKLKEDSP*O:OHe364d73%s<*#$*(#)!DSDFOUIHLjksdfOP:J>KEJWYGk",
    password);
makeShaBase64(inputBuf, sizeof(inputBuf), access);
}

#define edwMaxUserNameSize 128     /* Maximum size of an email handle */

int edwCheckUserNameSize(char *user)
/* Make sure user name not too long. Returns size or aborts if too long. */
{
int size = strlen(user);
if (size > edwMaxUserNameSize)
   errAbort("size of user name too long: %s", user);
return size;
}

int edwCheckAccess(struct sqlConnection *conn, char *user, char *password)
/* Make sure user exists and password checks out. Returns (non-zero) user ID on success*/
{
/* Make escaped version of email string since it may be raw user input. */
int nameSize  = edwCheckUserNameSize(user);
char escapedUser[2*nameSize+1];
sqlEscapeString2(escapedUser, user);

char access[EDW_ACCESS_SIZE];
edwMakeAccess(password, access);

char query[256];
safef(query, sizeof(query), "select access,id from edwUser where name='%s'", escapedUser);
struct sqlResult *sr = sqlGetResult(conn, query);
int userId = 0;
char **row;
if ((row = sqlNextRow(sr)) != NULL)
    {
    if (memcmp(row[0], access, sizeof(access)) == 0)
	{
	userId = sqlUnsigned(row[1]);
	}
    }
sqlFreeResult(&sr);
return userId;
}

int edwMustHaveAccess(struct sqlConnection *conn, char *user, char *password)
/* Check user has access and abort with an error message if not. Returns user id. */
{
int id = edwCheckAccess(conn, user, password);
if (id == 0)
    errAbort("User/password combination doesn't give access to database");
return id;
}

void edwMakeSid(char *user, char sid[EDW_ACCESS_SIZE])
/* Convert users to sid */
{
/* Salt it well with stuff that is reproducible but hard to guess, and some
 * one time true random stuff. Sneak in user too and time. */
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

safef(query, sizeof(query), "insert edwHost (name, firstAdded) values('%s', %lld)", 
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

long long edwGetLocalFile(struct sqlConnection *conn, char *localAbsolutePath, boolean symLink)
/* Get the id of a local file from the database, adding it if it doesn't already exist.
 * Add a local file to the database.  Go ahead and give it a license plate and an ID. 
 * optionally can make it a symbolic link instead of a copy. */
{
/* First do a reality check on the local absolute path.  Is there a file there? */
if (localAbsolutePath[0] != '/')
    errAbort("Using relative path in edwAddLocalFile.");
long long size = fileSize(localAbsolutePath);
if (size == -1)
    errAbort("%s does not exist", localAbsolutePath);
long long updateTime = fileModTime(localAbsolutePath);

/* Get file ID if such a file is already in database. */
int submitDirId = getLocalSubmitDir(conn);
int submitId = getLocalSubmit(conn);
char query[256+PATH_LEN];
safef(query, sizeof(query), "select id from edwFile where submitId=%d and submitFileName='%s'",
    submitId, localAbsolutePath);
long long fileId = sqlQuickLongLong(conn, query);

/* If we got something in database, check update time and size, and if it's no change just 
 * return existing database id. */
if (fileId != 0)
    {
    safef(query, sizeof(query), "select updateTime,size from edwFile where id=%lld", fileId);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row = sqlNeedNextRow(sr);
    long long oldUpdateTime = sqlLongLong(row[0]);
    long long oldSize =sqlLongLong(row[1]);
    sqlFreeResult(&sr);
    if (oldUpdateTime == updateTime && oldSize == size)
	return fileId;
    }

/* If we got here, then we need to make a new file record. Start with pretty empty record
 * that just has file ID, submitted file name and a few things*/
safef(query, sizeof(query), 
    "insert edwFile (submitId,submitDirId,submitFileName,startUploadTime) "
            " values(%d, %d, '%s', %lld)"
	    , submitId, submitDirId, localAbsolutePath, edwNow());
sqlUpdate(conn, query);
fileId = sqlLastAutoId(conn);

/* Create license plate and big data warehouse file/path name. */
char licensePlate[edwMaxPlateSize];
char edwFile[PATH_LEN], edwPath[PATH_LEN];
edwMakePlateFileNameAndPath(fileId, localAbsolutePath, licensePlate, edwFile, edwPath);

/* We're a little paranoid so md5 it */
char *md5 = "";

/* Do copy or symbolic linking of file into warehouse managed dir. */
if (symLink)
    makeSymLink(localAbsolutePath, edwPath);  
else
    {
    copyFile(localAbsolutePath, edwPath);
    md5 = md5HexForFile(localAbsolutePath);
    }

/* Update file record. */
safef(query, sizeof(query), 
    "update edwFile set licensePlate='%s', edwFileName='%s', endUploadTime=%lld,"
                       "updateTime=%lld, size=%lld, md5='%s' where id=%lld"
			, licensePlate,edwFile, edwNow(), updateTime, size, md5, fileId);
sqlUpdate(conn, query);
return fileId;
}

