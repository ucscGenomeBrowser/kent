/* cdwLib - routines shared by various cdw programs.    See also cdw
 * module for tables and routines to access structs built on tables. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"
#include "errAbort.h"
#include "cheapcgi.h"
#include "hex.h"
#include "openssl/sha.h"
#include "base64.h"
#include "basicBed.h"
#include "bigBed.h"
#include "portable.h"
#include "filePath.h"
#include "genomeRangeTree.h"
#include "md5.h"
#include "htmshell.h"
#include "obscure.h"
#include "bamFile.h"
#include "raToStruct.h"
#include "web.h"
#include "hdb.h"
#include "cdwValid.h"
#include "cdw.h"
#include "cdwFastqFileFromRa.h"
#include "cdwBamFileFromRa.h"
#include "cdwQaWigSpotFromRa.h"
#include "cdwVcfFileFromRa.h"
#include "rql.h"
#include "intValTree.h"
#include "tagStorm.h"
#include "cdwLib.h"
#include "trashDir.h"
#include "wikiLink.h"


/* System globals - just a few ... for now.  Please seriously not too many more. */
char *cdwDatabase = "cdw";
int cdwSingleFileTimeout = 4*60*60;   // How many seconds we give ourselves to fetch a single file

char *cdwRootDir = "/data/cirm/cdw/";
char *eapRootDir = "/data/cirm/encodeAnalysisPipeline/";
char *cdwValDataDir = "/data/cirm/valData/";
char *cdwDaemonEmail = "cdw@cirm-01.sdsc.edu";

struct sqlConnection *cdwConnect()
/* Returns a read only connection to database. */
{
return sqlConnect(cdwDatabase);
}

struct sqlConnection *cdwConnectReadWrite()
/* Returns read/write connection to database. */
{
return sqlConnectProfile("cdw", cdwDatabase);
}

char *cdwPathForFileId(struct sqlConnection *conn, long long fileId)
/* Return full path (which eventually should be freeMem'd) for fileId */
{
char query[256];
char fileName[PATH_LEN];
sqlSafef(query, sizeof(query), "select cdwFileName from cdwFile where id=%lld", fileId);
sqlNeedQuickQuery(conn, query, fileName, sizeof(fileName));
char path[512];
safef(path, sizeof(path), "%s%s", cdwRootDir, fileName);
return cloneString(path);
}

char *cdwTempDir()
/* Returns pointer to cdwTempDir.  This is shared, so please don't modify. */
{
static char path[PATH_LEN];
if (path[0] == 0)
    {
    /* Note code elsewhere depends on tmp dir being inside of cdwRootDir - also good
     * to have it there so move to a permanent file is quick and unlikely to fail. */
    safef(path, sizeof(path), "%s%s", cdwRootDir, "tmp");
    makeDirsOnPath(path);
    strcat(path, "/");
    }
return path;
}

char *cdwTempDirForToday(char dir[PATH_LEN])
/* Fills in dir with temp dir of the day, and returns a pointer to it. */
{
char dayDir[PATH_LEN];
cdwDirForTime(cdwNow(), dayDir);
safef(dir, PATH_LEN, "%s%stmp/", cdwRootDir, dayDir);

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


long long cdwGettingFile(struct sqlConnection *conn, char *submitDir, char *submitFileName)
/* See if we are in process of getting file.  Return file record id if it exists even if
 * it's not complete. Return -1 if record does not exist. */
{
/* First see if we have even got the directory. */
char query[PATH_LEN+512];
sqlSafef(query, sizeof(query), "select id from cdwSubmitDir where url='%s'", submitDir);
int submitDirId = sqlQuickNum(conn, query);
if (submitDirId <= 0)
    return -1;

/* Then see if we have file that matches submitDir and submitFileName. */
sqlSafef(query, sizeof(query), 
    "select id from cdwFile "
    "where submitFileName='%s' and submitDirId = %d and errorMessage = '' and deprecated=''"
    " and (endUploadTime >= startUploadTime or startUploadTime < %lld) "
    "order by submitId desc limit 1"
    , submitFileName, submitDirId
    , (long long)cdwNow() - cdwSingleFileTimeout);
long long id = sqlQuickLongLong(conn, query);
if (id == 0)
    return -1;
return id;
}

long long cdwGotFile(struct sqlConnection *conn, char *submitDir, char *submitFileName, 
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
        "select id from cdwFile where md5='%s' order by submitId desc limit 1" , md5);
    long long result = sqlQuickLongLong(conn, query);
    if (result == 0)
        result = -1;
    return result;
    }

/* Rest of the routine deals with smaller files,  which we are less worried about
 * duplicating,  and indeed expect a little duplication of the empty file if none
 * other. */

/* First see if we have even got the directory. */
sqlSafef(query, sizeof(query), "select id from cdwSubmitDir where url='%s'", submitDir);
int submitDirId = sqlQuickNum(conn, query);
if (submitDirId <= 0)
    return -1;

/* The complex truth is that we may have gotten this file multiple times. 
 * We return the most recent version where it got uploaded and passed the post-upload
 * MD5 sum, and thus where the MD5 field is filled in the database. */
sqlSafef(query, sizeof(query), 
    "select md5,id from cdwFile "
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

long long cdwNow()
/* Return current time in seconds since Epoch. */
{
return time(NULL);
}

/* This is size of base64 encoded hash plus 1 for the terminating zero. */
#define CDW_SID_SIZE 65   

static void makeShaBase64(unsigned char *inputBuf, int inputSize, char out[CDW_SID_SIZE])
/* Make zero terminated printable cryptographic hash out of in */
{
unsigned char shaBuf[48];
SHA384(inputBuf, inputSize, shaBuf);
char *base64 = base64Encode((char*)shaBuf, sizeof(shaBuf));
memcpy(out, base64, CDW_SID_SIZE);
out[CDW_SID_SIZE-1] = 0; 
freeMem(base64);
}

void cdwMakeSid(char *user, char sid[CDW_SID_SIZE])
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

static void cdwVerifySid(char *user, char *sidToCheck)
/* Make sure sid/user combo is good. */
{
char sid[CDW_SID_SIZE];
cdwMakeSid(user, sid);
if (sidToCheck == NULL || memcmp(sidToCheck, sid, CDW_SID_SIZE) != 0)
    errAbort("Authentication failed, sid %s", (sidToCheck ? "fail" : "miss"));
}

char *cdwGetEmailAndVerify()
/* Get email from persona-managed cookies and validate them.
 * Return email address if all is good and user is logged in.
 * If user not logged in return NULL.  If user logged in but
 * otherwise things are wrong abort. */
{
char *email = findCookieData("email");
if (email)
    {
    char *sid = findCookieData("sid");
    cdwVerifySid(email, sid);
    }
return email;
}

struct cdwUser *cdwUserFromUserName(struct sqlConnection *conn, char* userName)
/* Return user associated with that username or NULL if not found */
{
char *email = NULL;
// if the username is already an email address, then there is no need to go through the 
// gbMembers table
if (strstr(userName, "@")!=NULL)
    email = userName;
else 
    {
    struct sqlConnection *cc = hConnectCentral();
    char query[512];
    sqlSafef(query, sizeof(query), "select email from gbMembers where userName='%s'", userName);
    email = sqlQuickString(cc, query);
    hDisconnectCentral(&cc);
    }

struct cdwUser *user = cdwUserFromEmail(conn, email);
return user;
}

struct cdwUser *cdwUserFromEmail(struct sqlConnection *conn, char *email)
/* Return user associated with that email or NULL if not found */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwUser where email='%s'", email);
struct cdwUser *user = cdwUserLoadByQuery(conn, query);
return user;
}

struct cdwUser *cdwUserFromId(struct sqlConnection *conn, int id)
/* Return user associated with that id or NULL if not found */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwUser where id='%d'", id);
struct cdwUser *user = cdwUserLoadByQuery(conn, query);
return user;
}

int cdwUserIdFromFileId(struct sqlConnection *conn, int fId)
/* Return user id who submit the file originally */
{
char query[256];
sqlSafef(query, sizeof(query), "select s.userId from cdwSubmit s, cdwFile f where f.submitId=s.id and f.id='%d'", fId);
int sId = sqlQuickNum(conn, query);
sqlSafef(query, sizeof(query), "select u.id from cdwSubmit s, cdwUser u where  u.id=s.id and s.id='%d'", sId);
return sqlQuickNum(conn, query);
}

struct cdwUser *cdwFindUserFromFileId(struct sqlConnection *conn, int fId)
/* Return user who submit the file originally */
{
int uId = cdwUserIdFromFileId(conn, fId);
struct cdwUser *user=cdwUserFromId(conn, uId);
return user; 
}

char *cdwFindOwnerNameFromFileId(struct sqlConnection *conn, int fId)
/* Return name of submitter. Return "an unknown user" if name is NULL */
{
struct cdwUser *owner = cdwFindUserFromFileId(conn, fId);
if (owner == NULL)
    return ("an unknown user");
return cloneString(owner->email);
}

int cdwFindUserIdFromEmail(struct sqlConnection *conn, char *userEmail)
/* Return true id of this user */
{
char query[256];
sqlSafef(query, sizeof(query), "select id from cdwUser where email = '%s'", userEmail);
return sqlQuickNum(conn, query);
}

boolean cdwUserIsAdmin(struct sqlConnection *conn, char *userEmail)
/* Return true if the user is an admin */
{
char query[256];
sqlSafef(query, sizeof(query), "select isAdmin from cdwUser where email = '%s'", userEmail);
int isAdmin = sqlQuickNum(conn, query);
if (isAdmin == 1) return TRUE;
return FALSE;
}

void cdwWarnUnregisteredUser(char *email)
/* Put up warning message about unregistered user and tell them how to register. */
{
warn("No user exists with email %s. If you need an account please contact your "
	 "CIRM DCC data wrangler and have them create an account for you."
	 , email);
}


struct cdwUser *cdwMustGetUserFromEmail(struct sqlConnection *conn, char *email)
/* Return user associated with email or put up error message. */
{
struct cdwUser *user = cdwUserFromEmail(conn, email);
if (user == NULL)
    {
    cdwWarnUnregisteredUser(email);
    noWarnAbort();
    }
return user;
}

struct cdwGroup *cdwGroupFromName(struct sqlConnection *conn, char *name)
/* Return cdwGroup of given name or NULL if not found. */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwGroup where name='%s'", name);
return cdwGroupLoadByQuery(conn, query);
}

struct cdwGroup *cdwNeedGroupFromName(struct sqlConnection *conn, char *groupName)
/* Get named group or die trying */
{
struct cdwGroup *group = cdwGroupFromName(conn, groupName);
if (group == NULL)
    errAbort("Group %s doesn't exist", groupName);
return group;
}

boolean cdwFileInGroup(struct sqlConnection *conn, unsigned int fileId, unsigned int groupId)
/* Return TRUE if file is in group */
{
char query[256];
sqlSafef(query, sizeof(query), "select count(*) from cdwGroupFile where fileId=%u and groupId=%u",
    fileId, groupId);
return sqlQuickNum(conn, query) > 0;
}

int cdwUserFileGroupsIntersect(struct sqlConnection *conn, long long fileId, int userId)
/* Return the number of groups file and user have in common,  zero for no match */
{
char query[512];
sqlSafef(query, sizeof(query),
    "select count(*) from cdwGroupUser,cdwGroupFile "
    " where cdwGroupUser.groupId = cdwGroupFile.groupId "
    " and cdwGroupUser.userId = %d and cdwGroupFile.fileId = %lld"
    , userId, fileId);
verbose(2, "%s\n", query);
return sqlQuickNum(conn, query);
}

struct rbTree *cdwFilesWithSharedGroup(struct sqlConnection *conn, int userId)
/* Make an intVal type tree where the keys are fileIds and the val is null 
 * This contains all files that are associated with any group that user is part of. 
 * Can be used to do quicker version of cdwCheckAccess. */
{
struct rbTree *groupedFiles = intValTreeNew();
char query[256];
sqlSafef(query, sizeof(query), 
    "select distinct(fileId) from cdwGroupFile,cdwGroupUser "
    " where cdwGroupUser.groupId = cdwGroupFile.groupId "
    " and cdwGroupUser.userId = %d", userId);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    long long fileId = sqlLongLong(row[0]);
    intValTreeAdd(groupedFiles, fileId, NULL);
    }
sqlFreeResult(&sr);
return groupedFiles;
}

static boolean checkAccess(struct rbTree *groupedFiles, struct sqlConnection *conn, 
    struct cdwFile *ef, struct cdwUser *user, int accessType)
/* See if user should be allowed this level of access.  The accessType is one of
 * cdwAccessRead or cdwAccessWrite.  Write access implies read access too. 
 * This can be called with user as NULL, in which case only access to shared-with-all
 * files is granted. 
 * Since the most time consuming part of the operation involved the group access
 * check, parts of this can be precomputed in the groupedFiles tree. */
{
/* First check for public access. */
if (ef->allAccess >= accessType)
    return TRUE;

/* Everything else requires an actual user */
if (user == NULL)
    return FALSE;

/* Check for user individual access */
if (ef->userId == user->id && ef->userAccess >= accessType)
    return TRUE;

/* Check admin-level access */
if (user->isAdmin)
    return TRUE;

/* Check group access, this involves SQL query  */
if (ef->groupAccess >= accessType)
    {
    if (groupedFiles != NULL)
	return intValTreeLookup(groupedFiles, ef->id) != NULL;
    else
	return cdwUserFileGroupsIntersect(conn, ef->id, user->id);
    }
    
return FALSE;
}

boolean cdwCheckAccess(struct sqlConnection *conn, struct cdwFile *ef,
    struct cdwUser *user, int accessType)
/* See if user should be allowed this level of access.  The accessType is one of
 * cdwAccessRead or cdwAccessWrite.  Write access implies read access too. 
 * This can be called with user as NULL, in which case only access to shared-with-all
 * files is granted. This function takes almost a millisecond.  If you are doing it
 * to many files consider using cdwQuickCheckAccess instead. */
{
return checkAccess(NULL, conn, ef, user, accessType);
}

boolean cdwQuickCheckAccess(struct rbTree *groupedFiles, struct cdwFile *ef,
    struct cdwUser *user, int accessType)
/* See if user should be allowed this level of access.  The groupedFiles is
 * the result of a call to cdwFilesWithSharedGroup. The other parameters are as
 * cdwCheckAccess.  If you are querying thousands of files, this function is hundreds
 * of times faster though. */
{
return checkAccess(groupedFiles, NULL, ef, user, accessType);
}

long long cdwCountAccessible(struct sqlConnection *conn, struct cdwUser *user)
/* Return total number of files associated user can access */
{
long long count = 0;
if (user == NULL)
    {
    char query[256];
    sqlSafef(query, sizeof(query), 
	"select count(*) from cdwFile,cdwValidFile "
	" where cdwFile.id = cdwValidFile.fileId and allAccess > 0"
	" and (errorMessage='' or errorMessage is null)"
	);
    count = sqlQuickLongLong(conn, query);
    }
else
    {
    struct rbTree *groupedFiles = cdwFilesWithSharedGroup(conn, user->id);
    char query[256];
    sqlSafef(query, sizeof(query), 
	"select cdwFile.* from cdwFile,cdwValidFile "
	" where cdwFile.id = cdwValidFile.fileId "
	" and (errorMessage='' or errorMessage is null)"
	);
    struct cdwFile *ef, *efList = cdwFileLoadByQuery(conn, query);
    for (ef = efList; ef != NULL; ef = ef->next)
	{
	if (cdwQuickCheckAccess(groupedFiles, ef, user, cdwAccessRead))
	    ++count;
	}
    cdwFileFree(&efList);
    rbTreeFree(&groupedFiles);
    }
return count;
}

struct cdwFile *cdwAccessibleFileList(struct sqlConnection *conn, struct cdwUser *user)
/* Get list of all files user can access.  Null user means just publicly accessible.  */
{
if (user == NULL)  // No user, just publicly readable files then
    {
    char query[256];
    sqlSafef(query, sizeof(query), 
	"select cdwFile.* from cdwFile,cdwValidFile "
	" where cdwFile.id = cdwValidFile.fileId and allAccess > 0"
	" and (errorMessage='' or errorMessage is null)");
    return cdwFileLoadByQuery(conn, query);
    }
else	// Load all valid files and check access one at a time
    {
    struct rbTree *groupedFiles = cdwFilesWithSharedGroup(conn, user->id);
    struct cdwFile *accessibleList = NULL, *validList = cdwFileLoadAllValid(conn);
    struct cdwFile *ef, *next;
    for (ef = validList; ef != NULL; ef = next)
        {
	next = ef->next;
	if (cdwQuickCheckAccess(groupedFiles, ef, user, cdwAccessRead))
	    {
	    slAddHead(&accessibleList, ef);
	    }
	else
	    {
	    cdwFileFree(&ef);
	    }
	}
    rbTreeFree(&groupedFiles);
    slReverse(&accessibleList);
    return accessibleList;
    }
}

struct rbTree *cdwAccessTreeForUser(struct sqlConnection *conn, struct cdwUser *user, 
    struct cdwFile *efList, struct rbTree *groupedFiles)
/* Construct intVal tree of files from efList that we have access to.  The
 * key is the fileId,  the value is the cdwFile object */
{
struct rbTree *accessTree = intValTreeNew(0);
struct cdwFile *ef;
for (ef = efList; ef != NULL; ef = ef->next)
    {
    if (cdwQuickCheckAccess(groupedFiles, ef, user, cdwAccessRead))
	intValTreeAdd(accessTree, ef->id, ef);
    }
return accessTree;
}


int cdwGetHost(struct sqlConnection *conn, char *hostName)
/* Look up host name in table and return associated ID.  If not found
 * make up new table entry. */
{
/* If it's already in table, just return ID. */
char query[512];
sqlSafef(query, sizeof(query), "select id from cdwHost where name='%s'", hostName);
int hostId = sqlQuickNum(conn, query);
if (hostId > 0)
    return hostId;
sqlSafef(query, sizeof(query), "insert cdwHost (name, firstAdded, paraFetchStreams) values('%s', %lld, 10)", 
       hostName, cdwNow());
sqlUpdate(conn, query);
return sqlLastAutoId(conn);
}

int cdwGetSubmitDir(struct sqlConnection *conn, int hostId, char *submitDir)
/* Get submitDir from database, creating it if it doesn't already exist. */
{
/* If it's already in table, just return ID. */
char query[512];
sqlSafef(query, sizeof(query), "select id from cdwSubmitDir where url='%s'", submitDir);
int dirId = sqlQuickNum(conn, query);
if (dirId > 0)
    return dirId;

sqlSafef(query, sizeof(query), 
   "insert cdwSubmitDir (url, firstAdded, hostId) values('%s', %lld, %d)", 
   submitDir, cdwNow(), hostId);
sqlUpdate(conn, query);
return sqlLastAutoId(conn);
}

void cdwMakeLicensePlate(char *prefix, int ix, char *out, int outSize)
/* Make a license-plate type string composed of prefix + funky coding of ix
 * and put result in out. */
{
int maxIx = 10*10*10*26*26*26;
if (ix < 0)
    errAbort("ix must be positive in cdwMakeLicensePlate");
if (ix > maxIx)
    errAbort("ix exceeds max in cdwMakeLicensePlate.  ix %d, max %d\n", ix, maxIx);
int prefixSize = strlen(prefix);
int minSize = prefixSize + 6 + 1;
if (outSize < minSize)
    errAbort("outSize (%d) not big enough in cdwMakeLicensePlate", outSize);

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

void cdwDirForTime(time_t sinceEpoch, char dir[PATH_LEN])
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

void cdwMakeBabyName(unsigned long id, char *baseName, int baseNameSize)
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
        errAbort("Not enough room for %lu in %d letters in cdwMakeBabyName", id, baseNameSize);
    baseName[basePos] = c;
    baseName[basePos+1] = v;
    basePos += 2;
    }
while (ix > 0);
baseName[basePos] = 0;
}

char *cdwFindDoubleFileSuffix(char *path)
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

void cdwMakeFileNameAndPath(int cdwFileId, char *submitFileName, char cdwFile[PATH_LEN], char serverPath[PATH_LEN])
/* Convert file id to local file name, and full file path. Make any directories needed
 * along serverPath. */
{
/* Preserve suffix.  Give ourselves up to two suffixes. */
char *suffix = cdwFindDoubleFileSuffix(submitFileName);

/* Figure out cdw file name, starting with baseName. */
char baseName[32];
cdwMakeBabyName(cdwFileId, baseName, sizeof(baseName));

/* Figure out directory and make any components not already there. */
char cdwDir[PATH_LEN];
cdwDirForTime(cdwNow(), cdwDir);
char uploadDir[PATH_LEN];
safef(uploadDir, sizeof(uploadDir), "%s%s", cdwRootDir, cdwDir);
makeDirsOnPath(uploadDir);

/* Figure out full file names */
safef(cdwFile, PATH_LEN, "%s%s%s", cdwDir, baseName, suffix);
safef(serverPath, PATH_LEN, "%s%s", cdwRootDir, cdwFile);
}

char *cdwSetting(struct sqlConnection *conn, char *name)
/* Return named settings value,  or NULL if setting doesn't exist. FreeMem when done. */
{
char query[256];
sqlSafef(query, sizeof(query), "select val from cdwSettings where name='%s'", name);
return sqlQuickString(conn, query);
}

char *cdwRequiredSetting(struct sqlConnection *conn, char *name)
/* Returns setting, abort if it isn't found. FreeMem when done. */
{
char *val = cdwSetting(conn, name);
if (val == NULL)
    errAbort("Required %s setting is not defined in cdwSettings table", name);
return val;
}

char *cdwLicensePlateHead(struct sqlConnection *conn)
/* Return license plate prefix for current database - something like TSTFF or DEVFF or ENCFF */
{
static char head[32];
if (head[0] == 0)
     {
     char *prefix = cdwRequiredSetting(conn, "prefix");
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
sqlSafef(query, sizeof(query), "select id from cdwHost where name = '%s'", localHostName);
int hostId = sqlQuickNum(conn, query);
if (hostId == 0)
    {
    sqlSafef(query, sizeof(query), "insert cdwHost(name, firstAdded) values('%s', %lld)",
	localHostName,  cdwNow());
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
sqlSafef(query, sizeof(query), "select id from cdwSubmitDir where url='%s' and hostId=%d", 
    localHostDir, hostId);
int dirId = sqlQuickNum(conn, query);
if (dirId == 0)
    {
    sqlSafef(query, sizeof(query), "insert cdwSubmitDir(url,hostId,firstAdded) values('%s',%d,%lld)",
	localHostDir, hostId, cdwNow());
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
sqlSafef(query, sizeof(query), "select id from cdwSubmit where submitDirId='%d'", dirId);
int submitId = sqlQuickNum(conn, query);
if (submitId == 0)
    {
    sqlSafef(query, sizeof(query), "insert cdwSubmit (submitDirId,startUploadTime) values(%d,%lld)",
	dirId, cdwNow());
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

void cdwUpdateFileTags(struct sqlConnection *conn, long long fileId, struct dyString *tags)
/* Update tags field in cdwFile with given value */
{
struct dyString *query = dyStringNew(0);
sqlDyStringAppend(query, "update cdwFile set tags='");
dyStringAppend(query, tags->string);
dyStringPrintf(query, "' where id=%lld", fileId);
sqlUpdate(conn, query->string);
dyStringFree(&query);
}

struct cdwFile *cdwGetLocalFile(struct sqlConnection *conn, char *localAbsolutePath, 
    char *givenMd5Sum)
/* Get record of local file from database, adding it if it doesn't already exist.
 * Can make it a symLink rather than a copy in which case pass in valid MD5 sum
 * for symLinkM5dSum. */
{
/* First do a reality check on the local absolute path.  Is there a file there? */
if (localAbsolutePath[0] != '/')
    errAbort("Using relative path in cdwAddLocalFile.");
long long size = fileSize(localAbsolutePath);
if (size == -1)
    errAbort("%s does not exist", localAbsolutePath);
long long updateTime = fileModTime(localAbsolutePath);

/* Get file if it's in database already. */
int submitDirId = getLocalSubmitDir(conn);
int submitId = getLocalSubmit(conn);
char query[256+PATH_LEN];
sqlSafef(query, sizeof(query), "select * from cdwFile where submitId=%d and submitFileName='%s'",
    submitId, localAbsolutePath);
struct cdwFile *ef = cdwFileLoadByQuery(conn, query);

/* If we got something in database, check update time and size, and if it's no change just 
 * return existing database id. */
if (ef != NULL && ef->updateTime == updateTime && ef->size == size)
    return ef;

/* If we got here, then we need to make a new file record. Start with pretty empty record
 * that just has file ID, submitted file name and a few things*/
sqlSafef(query, sizeof(query), 
    "insert cdwFile (submitId,submitDirId,submitFileName,startUploadTime) "
            " values(%d, %d, '%s', %lld)"
	    , submitId, submitDirId, localAbsolutePath, cdwNow());
sqlUpdate(conn, query);
long long fileId = sqlLastAutoId(conn);

/* Create big data warehouse file/path name. */
char cdwFile[PATH_LEN], cdwPath[PATH_LEN];
cdwMakeFileNameAndPath(fileId, localAbsolutePath, cdwFile, cdwPath);

/* We're a little paranoid so md5 it */
char *md5;

/* Do copy or symbolic linking of file into warehouse managed dir. */
if (givenMd5Sum)
    {
    md5 = givenMd5Sum;
    }
else
    {
    md5 = md5HexForFile(localAbsolutePath);
    }
copyFile(localAbsolutePath, cdwPath);
chmod(cdwPath, 0444);
replaceOriginalWithSymlink(localAbsolutePath, "", cdwPath);

/* Update file record. */
sqlSafef(query, sizeof(query), 
    "update cdwFile set cdwFileName='%s', endUploadTime=%lld,"
                       "updateTime=%lld, size=%lld, md5='%s' where id=%lld"
			, cdwFile, cdwNow(), updateTime, size, md5, fileId);
sqlUpdate(conn, query);

/* Now, it's a bit of a time waste, but cheap in code, to just load it back from DB. */
sqlSafef(query, sizeof(query), "select * from cdwFile where id=%lld", fileId);
return cdwFileLoadByQuery(conn, query);
}

struct cdwFile *cdwFileLoadAllValid(struct sqlConnection *conn)
/* Get list of cdwFiles that have been validated with no error */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select cdwFile.* from cdwFile,cdwValidFile "
    " where cdwFile.id=cdwValidFile.fileId "
    " and (cdwFile.errorMessage='' or cdwFile.errorMessage is null)");
return cdwFileLoadByQuery(conn, query);
}

struct cdwFile *cdwFileAllIntactBetween(struct sqlConnection *conn, int startId, int endId)
/* Return list of all files that are intact (finished uploading and MD5 checked) 
 * with file IDs between startId and endId - including endId */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select * from cdwFile where id>=%d and id<=%d and endUploadTime != 0 "
    "and updateTime != 0 and (errorMessage = '' or errorMessage is NULL) and deprecated = ''", 
    startId, endId);
return cdwFileLoadByQuery(conn, query);
}

long long cdwFindInSameSubmitDir(struct sqlConnection *conn, 
    struct cdwFile *ef, char *submitFileName)
/* Return fileId of most recent file of given submitFileName from submitDir
 * associated with file */
{
char query[3*PATH_LEN];
sqlSafef(query, sizeof(query),
    "select cdwFile.id from cdwFile,cdwSubmitDir "
    "where cdwFile.submitDirId = cdwSubmitDir.id and "
    "cdwSubmitDir.id = %d and "
    "cdwFile.submitFileName = '%s' order by cdwFile.id desc"
    ,  ef->submitDirId, submitFileName);
return sqlQuickLongLong(conn, query);
}

struct cdwFile *cdwFileFromId(struct sqlConnection *conn, long long fileId)
/* Return cdwValidFile given fileId - return NULL if not found. */
{
char query[128];
sqlSafef(query, sizeof(query), "select * from cdwFile where id=%lld", fileId);
return cdwFileLoadByQuery(conn, query);
}

struct cdwFile *cdwFileFromIdOrDie(struct sqlConnection *conn, long long fileId)
/* Return cdwValidFile given fileId - aborts if not found. */
{
struct cdwFile *ef = cdwFileFromId(conn, fileId);
if (ef == NULL)
    errAbort("Couldn't find file for id %lld\n", fileId);
return ef;
}

int cdwFileIdFromPathSuffix(struct sqlConnection *conn, char *suf)
/* return most recent fileId for file where submitDir.url+submitFname ends with suf. 0 if not found. */
{
char query[4096];
int sufLen = strlen(suf);
sqlSafef(query, sizeof(query), "SELECT cdwFile.id FROM cdwSubmitDir, cdwFile " 
    "WHERE cdwFile.submitDirId=cdwSubmitDir.id AND RIGHT(CONCAT_WS('/', cdwSubmitDir.url, submitFileName), %d)='%s' "
    "ORDER BY cdwFile.id DESC LIMIT 1;", sufLen, suf);
int fileId = sqlQuickNum(conn, query);
return fileId;
}

struct cdwValidFile *cdwValidFileFromFileId(struct sqlConnection *conn, long long fileId)
/* Return cdwValidFile give fileId - returns NULL if not validated. */
{
char query[128];
sqlSafef(query, sizeof(query), "select * from cdwValidFile where fileId=%lld", fileId);
return cdwValidFileLoadByQuery(conn, query);
}

struct cdwValidFile *cdwValidFileFromLicensePlate(struct sqlConnection *conn, char *licensePlate)
/* Return cdwValidFile from license plate - returns NULL if not found. */
{
char query[128];
sqlSafef(query, sizeof(query), "select * from cdwValidFile where licensePlate='%s'", licensePlate);
return cdwValidFileLoadByQuery(conn, query);
}

struct cdwExperiment *cdwExperimentFromAccession(struct sqlConnection *conn, char *acc)
/* Given something like 'ENCSR123ABC' return associated experiment. */
{
char query[128];
sqlSafef(query, sizeof(query), "select * from cdwExperiment where accession='%s'", acc);
return cdwExperimentLoadByQuery(conn, query);
}

struct genomeRangeTree *cdwMakeGrtFromBed3List(struct bed3 *bedList)
/* Make up a genomeRangeTree around bed file. */
{
struct genomeRangeTree *grt = genomeRangeTreeNew();
struct bed3 *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    genomeRangeTreeAdd(grt, bed->chrom, bed->chromStart, bed->chromEnd);
return grt;
}

struct cdwAssembly *cdwAssemblyForUcscDb(struct sqlConnection *conn, char *ucscDb)
/* Get assembly for given UCSC ID or die trying */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwAssembly where ucscDb='%s'", ucscDb);
struct cdwAssembly *assembly = cdwAssemblyLoadByQuery(conn, query);
if (assembly == NULL)
    errAbort("Can't find assembly for %s", ucscDb);
return assembly;
}

struct cdwAssembly *cdwAssemblyForId(struct sqlConnection *conn, long long id)
/* Get assembly of given ID. */
{
char query[128];
sqlSafef(query, sizeof(query), "select * from cdwAssembly where id=%lld", id);
struct cdwAssembly *assembly = cdwAssemblyLoadByQuery(conn, query);
if (assembly == NULL)
    errAbort("Can't find assembly for %lld", id);
return assembly;
}

char *cdwSimpleAssemblyName(char *assembly)
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


struct genomeRangeTree *cdwGrtFromBigBed(char *fileName)
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

boolean cdwIsSupportedBigBedFormat(char *format)
/* Return TRUE if it's one of the bigBed formats we support. */
{
int i;
if (sameString(format, "bigBed"))   // Generic bigBed ok
    return TRUE;
for (i=0; i<cdwBedTypeCount; ++i)
    {
    if (sameString(format, cdwBedTypeTable[i].name))
        return TRUE;
    }
return FALSE;
}

void cdwWriteErrToTable(struct sqlConnection *conn, char *table, int id, char *err)
/* Write out error message to errorMessage field of table. */
{
char *trimmedError = trimSpaces(err);
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "update %s set errorMessage='%s' where id=%d", 
    table, trimmedError, id);
sqlUpdate(conn, query->string);
dyStringFree(&query);
}

void cdwWriteErrToStderrAndTable(struct sqlConnection *conn, char *table, int id, char *err)
/* Write out error message to errorMessage field of table and through stderr. */
{
warn("%s", trimSpaces(err));
cdwWriteErrToTable(conn, table, id, err);
}


void cdwAddJob(struct sqlConnection *conn, char *command, int submitId)
/* Add job to queue to run. */
{
char query[256+strlen(command)];
sqlSafef(query, sizeof(query), "insert into cdwJob (commandLine,submitId) values('%s',%d)", 
    command, submitId);
sqlUpdate(conn, query);
}

void cdwAddQaJob(struct sqlConnection *conn, long long fileId, int submitId)
/* Create job to do QA on this and add to queue */
{
char command[64];
safef(command, sizeof(command), "cdwQaAgent %lld", fileId);
cdwAddJob(conn, command, submitId);
}

int cdwSubmitPositionInQueue(struct sqlConnection *conn, char *url, unsigned *retJobId)
/* Return position of our URL in submission queue.  Optionally return id in cdwSubmitJob
 * table of job. */
{
char query[256];
sqlSafef(query, sizeof(query), "select id,commandLine from cdwSubmitJob where startTime = 0");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
int aheadOfUs = -1;
int pos = 0;
unsigned jobId = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    jobId = sqlUnsigned(row[0]);
    char *line = row[1];
    char *cdwSubmit = nextQuotedWord(&line);
    char *lineUrl = nextQuotedWord(&line);
    if (sameOk(cdwSubmit, "cdwSubmit") && sameOk(url, lineUrl))
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

struct cdwSubmitDir *cdwSubmitDirFromId(struct sqlConnection *conn, long long id)
/* Return submissionDir with given ID or NULL if no such submission. */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwSubmitDir where id=%lld", id);
return cdwSubmitDirLoadByQuery(conn, query);
}


struct cdwSubmit *cdwSubmitFromId(struct sqlConnection *conn, long long id)
/* Return submission with given ID or NULL if no such submission. */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwSubmit where id=%lld", id);
return cdwSubmitLoadByQuery(conn, query);
}


struct cdwSubmit *cdwMostRecentSubmission(struct sqlConnection *conn, char *url)
/* Return most recent submission, possibly in progress, from this url */
{
int urlSize = strlen(url);
char query[128 + 2*urlSize + 1];
sqlSafef(query, sizeof(query), 
    "select * from cdwSubmit where url='%s' order by id desc limit 1", url);
return cdwSubmitLoadByQuery(conn, query);
}

long long cdwSubmitMaxStartTime(struct cdwSubmit *submit, struct sqlConnection *conn)
/* Figure out when we started most recent single file in the upload, or when
 * we started if not files started yet. */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select max(startUploadTime) from cdwFile where submitId=%u", submit->id);
long long maxStartTime = sqlQuickLongLong(conn, query);
if (maxStartTime == 0)
    maxStartTime = submit->startUploadTime;
return maxStartTime;
}

int cdwSubmitCountNewValid(struct cdwSubmit *submit, struct sqlConnection *conn)
/* Count number of new files in submission that have been validated. */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select count(*) from cdwFile e,cdwValidFile v where e.id = v.fileId and e.submitId=%u",
    submit->id);
return sqlQuickNum(conn, query);
}

int cdwSubmitCountErrors(struct cdwSubmit *submit, struct sqlConnection *conn)
/* Count number of errors with submitted files */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select count(*) from cdwFile where submitId=%u and errorMessage != '' and errorMessage is not null",
    submit->id);
return sqlQuickNum(conn, query);
}

boolean cdwSubmitIsValidated(struct cdwSubmit *submit, struct sqlConnection *conn)
/* Return TRUE if validation has run.  This does not mean that they all passed validation.
 * It just means the validator has run and has made a decision on each file in the submission. */
{
/* Is this off by one because of the validated.txt being in the submission but never validated? */
return cdwSubmitCountErrors(submit,conn) + cdwSubmitCountNewValid(submit, conn) == submit->newFiles;
}

void cdwAddSubmitJob(struct sqlConnection *conn, char *userEmail, char *url, boolean update)
/* Add submission job to table and wake up daemon. */
{
/* Create command and add it to cdwSubmitJob table. */
char command[strlen(url) + strlen(userEmail) + 256];
safef(command, sizeof(command), "cdwSubmit %s'%s' %s", (update ? "-update " : ""), url, userEmail);
char query[strlen(command)+128];
sqlSafef(query, sizeof(query), "insert cdwSubmitJob (commandLine) values('%s')", command);
sqlUpdate(conn, query);

/* Write sync signal (any string ending with newline) to fifo to wake up daemon. */
FILE *fifo = mustOpen("../userdata/cdwSubmit.fifo", "w");
fputc('\n', fifo);
carefulClose(&fifo);
}


struct cdwValidFile *cdwFindElderReplicates(struct sqlConnection *conn, struct cdwValidFile *vf)
/* Find all replicates of same output and format type for experiment that are elder
 * (fileId less than your file Id).  Younger replicates are responsible for taking care 
 * of correlations with older ones.  Sorry younguns, it's like social security. */
{
if (sameString(vf->format, "unknown"))
    return NULL;
char query[256];
sqlSafef(query, sizeof(query), 
    "select * from cdwValidFile where id<%d and experiment='%s' and format='%s'"
    " and outputType='%s'"
    , vf->id, vf->experiment, vf->format, vf->outputType);
return cdwValidFileLoadByQuery(conn, query);
}

#ifdef OLD
void cdwWebHeaderWithPersona(char *title)
/* Print out HTTP and HTML header through <BODY> tag with persona info */
{
printf("Content-Type:text/html\r\n");
printf("\r\n\r\n");
puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" "
	      "\"http://www.w3.org/TR/html4/loose.dtd\">");
printf("<HTML><HEAD><TITLE>%s</TITLE>\n", "CIRM Data Warehouse");
puts("<meta http-equiv='X-UA-Compatible' content='IE=Edge'>");

// Use CIRM3 CSS for common look
puts("<link rel='stylesheet' href='/style/cirm.css' type='text/css'>");
puts("<link rel='stylesheet' href='/style/cirmUcsc.css' type='text/css'>");
// external link icon (box with arrow) is from FontAwesome (fa-external-link)
puts("<link href='//netdna.bootstrapcdn.com/font-awesome/4.0.3/css/font-awesome.css' rel='stylesheet'>");

puts("<script type='text/javascript' SRC='/js/jquery.js'></script>");
puts("<script type='text/javascript' SRC='/js/jquery.cookie.js'></script>");
puts("<script type='text/javascript' src='https://login.persona.org/include.js'></script>");
puts("<script type='text/javascript' src='/js/cdwPersona.js'></script>");
puts("<script type='text/javascript' src='https://cdnjs.cloudflare.com/ajax/libs/bowser/1.6.1/bowser.min.js'></script>");
puts("</HEAD>");

/* layout with navigation bar */
puts("<BODY>\n");

cdwWebNavBarStart();
}
#endif /* OLD */


#ifdef OLD
void cdwWebFooterWithPersona()
/* Print out end tags and persona script stuff */
{
cdwWebNavBarEnd();
htmlEnd();
}
#endif /* OLD */


void cdwCreateNewUser(char *email)
/* Create new user, checking that user does not already exist. */
{
/* Now make sure user is not already in user table. */
struct sqlConnection *conn = cdwConnectReadWrite();
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "select count(*) from cdwUser where email = '%s'", email);
if (sqlQuickNum(conn, query->string) > 0)
    errAbort("User %s already exists", email);

/* Do database insert. */
dyStringClear(query);
sqlDyStringPrintf(query, "insert into cdwUser (email) values('%s')", email);
sqlUpdate(conn, query->string);

sqlDisconnect(&conn);
}

void cdwPrintLogOutButton()
/* Print log out button */
{
printf("<INPUT TYPE=button NAME=\"signOut\" VALUE=\"sign out\" id=\"signout\">");
}

struct dyString *cdwFormatDuration(long long seconds)
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

struct cdwFile *cdwFileInProgress(struct sqlConnection *conn, int submitId)
/* Return file in submission in process of being uploaded if any. */
{
char query[256];
sqlSafef(query, sizeof(query), "select fileIdInTransit from cdwSubmit where id=%u", submitId);
long long fileId = sqlQuickLongLong(conn, query);
if (fileId == 0)
    return NULL;
sqlSafef(query, sizeof(query), "select * from cdwFile where id=%lld", (long long)fileId);
return cdwFileLoadByQuery(conn, query);
}


static void accessDenied()
/* Sleep a bit and then deny access. */
{
sleep(5);
errAbort("Access denied!");
}

struct cdwScriptRegistry *cdwScriptRegistryFromCgi()
/* Get script registery from cgi variables.  Does authentication too. */
{
struct sqlConnection *conn = cdwConnect();
char *user = sqlEscapeString(cgiString("user"));
char *password = sqlEscapeString(cgiString("password"));
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwScriptRegistry where name='%s'", user);
struct cdwScriptRegistry *reg = cdwScriptRegistryLoadByQuery(conn, query);
if (reg == NULL)
    accessDenied();
char key[CDW_SID_SIZE];
cdwMakeSid(password, key);
if (!sameString(reg->secretHash, key))
    accessDenied();
sqlDisconnect(&conn);
return reg;
}

void cdwValidFileUpdateDb(struct sqlConnection *conn, struct cdwValidFile *el, long long id)
/* Save cdwValidFile as a row to the table specified by tableName, replacing existing record at 
 * id. */
{
struct dyString *dy = newDyString(512);
sqlDyStringAppend(dy, "update cdwValidFile set ");
// omit id and licensePlate fields - one autoupdates and the other depends on this
// also omit fileId which also really can't change.
dyStringPrintf(dy, " format='%s',", el->format);
dyStringPrintf(dy, " outputType='%s',", el->outputType);
dyStringPrintf(dy, " experiment='%s',", el->experiment);
dyStringPrintf(dy, " replicate='%s',", el->replicate);
dyStringPrintf(dy, " enrichedIn='%s',", el->enrichedIn);
dyStringPrintf(dy, " ucscDb='%s',", el->ucscDb);
dyStringPrintf(dy, " itemCount=%lld,", (long long)el->itemCount);
dyStringPrintf(dy, " basesInItems=%lld,", (long long)el->basesInItems);
dyStringPrintf(dy, " sampleCount=%lld,", (long long)el->sampleCount);
dyStringPrintf(dy, " basesInSample=%lld,", (long long)el->basesInSample);
dyStringPrintf(dy, " sampleBed='%s',", el->sampleBed);
dyStringPrintf(dy, " mapRatio=%g,", el->mapRatio);
dyStringPrintf(dy, " sampleCoverage=%g,", el->sampleCoverage);
dyStringPrintf(dy, " depth=%g,", el->depth);
dyStringPrintf(dy, " singleQaStatus=0,");
dyStringPrintf(dy, " replicateQaStatus=0,");
dyStringPrintf(dy, " part='%s',", el->part);
dyStringPrintf(dy, " pairedEnd='%s',", el->pairedEnd);
dyStringPrintf(dy, " qaVersion='%d',", el->qaVersion);
dyStringPrintf(dy, " uniqueMapRatio=%g,", el->uniqueMapRatio);
dyStringPrintf(dy, " lane='%s'", el->lane);
#if (CDWVALIDFILE_NUM_COLS != 24)
   #error "Please update this routine with new column"
#endif
dyStringPrintf(dy, " where id=%lld\n", (long long)id);
sqlUpdate(conn, dy->string);
freeDyString(&dy);
}

char *cdwLookupTag(struct cgiParsedVars *list, char *tag)
/* Return first occurence of tag on list, or empty string if not found */
{
char *ret = "";
struct cgiParsedVars *tags;
for (tags = list; tags != NULL; tags = tags->next)
    {
    char *val = hashFindVal(tags->hash, tag);
    if (val != NULL && !sameString(val, "n/a"))
	{
	ret = val;
	break;
	}
    }
return ret;
}

void cdwValidFileFieldsFromTags(struct cdwValidFile *vf, struct cgiParsedVars *tags)
/* Fill in many of vf's fields from tags. */
{
// Most fields are just taken directly from tags, converted from underbar to camelCased
// separation conventions.
vf->format = cloneString(cdwLookupTag(tags, "format"));
vf->outputType = cloneString(cdwLookupTag(tags, "output_type"));
vf->replicate = cloneString(cdwLookupTag(tags, "replicate"));
vf->enrichedIn = cloneString(cdwLookupTag(tags, "enriched_in"));
vf->ucscDb = cloneString(cdwLookupTag(tags, "ucsc_db"));
vf->part = cloneString(cdwLookupTag(tags, "file_part"));
vf->pairedEnd = cloneString(cdwLookupTag(tags, "paired_end"));
vf->lane = cloneString(cdwLookupTag(tags, "lane"));

// Experiment field defaults to same as meta, but sometimes needs to be different.
// Experiment field drives replicate comparisons.
char *experiment = cdwLookupTag(tags, "experiment");
if (isEmpty(experiment))
    experiment = cdwLookupTag(tags, "meta");
vf->experiment = cloneString(experiment);

// Make sure we update this routine if we add fields to cdwValidFile
#if (CDWVALIDFILE_NUM_COLS != 24)
   #error "Please update this routine with new column"
#endif
}

void cdwRemoveQaRecords(struct sqlConnection *conn, long long fileId)
/* Remove records associated with a file from all of the cdwQaXxx and cdwXxxFile
 * tables */
{
char query[1024];
sqlSafef(query, sizeof(query), "delete from cdwFastqFile where fileId=%lld", fileId);
sqlUpdate(conn, query);
sqlSafef(query, sizeof(query), "delete from cdwBamFile where fileId=%lld", fileId);
sqlUpdate(conn, query);
sqlSafef(query, sizeof(query), "delete from cdwVcfFile where fileId=%lld", fileId);
sqlUpdate(conn, query);
sqlSafef(query, sizeof(query),
    "delete from cdwQaPairSampleOverlap where elderFileId=%lld or youngerFileId=%lld",
    fileId, fileId);
sqlUpdate(conn, query);
sqlSafef(query, sizeof(query),
    "delete from cdwQaPairCorrelation where elderFileId=%lld or youngerFileId=%lld",
    fileId, fileId);
sqlUpdate(conn, query);
sqlSafef(query, sizeof(query), "delete from cdwQaEnrich where fileId=%lld", fileId);
sqlUpdate(conn, query);
sqlSafef(query, sizeof(query), "delete from cdwQaContam where fileId=%lld", fileId);
sqlUpdate(conn, query);
sqlSafef(query, sizeof(query), "delete from cdwQaRepeat where fileId=%lld", fileId);
sqlUpdate(conn, query);
sqlSafef(query, sizeof(query), 
    "delete from cdwQaPairedEndFastq where fileId1=%lld or fileId2=%lld",
    fileId, fileId);
sqlUpdate(conn, query);
}

static char *mustReadSymlink(char *path, struct stat *sb)
/* Read symlink or abort. FreeMem the returned value. */
{
ssize_t nbytes, bufsiz;
// determine whether the buffer returned was truncated.
bufsiz = sb->st_size + 1;
char *symPath = needMem(bufsiz);
nbytes = readlink(path, symPath, bufsiz);
if (nbytes == -1) 
    errnoAbort("readlink failure on symlink %s", path);
if (nbytes == bufsiz)
    errAbort("readlink returned buffer truncated\n");
return symPath;
}

int findSubmitSymlinkExt(char *submitFileName, char *submitDir, char **pPath, char **pLastPath, int *pSymlinkLevels)
/* Find the last symlink and real file in the chain from submitDir/submitFileName.
 * This is useful for when target of symlink in cdw/ gets renamed 
 * (e.g. license plate after passes validation), or removed (e.g. cdwReallyRemove* commands). 
 * Returns 0 for success. /
 * Returns -1 if path does not exist. */
{
int result = 0;
struct stat sb;
char *lastPath = NULL;
char *path = mustExpandRelativePath(submitDir, submitFileName);

int symlinkLevels = 0;
while (TRUE)
    {
    if (!fileExists(path))
	{
	//path=does not exist
	result = -1;
	break;
	}
    if (lstat(path, &sb) == -1)
	errnoAbort("stat failure on %s", path);
    if ((sb.st_mode & S_IFMT) != S_IFLNK)
	break;

    // follow the symlink
    ++symlinkLevels;
    if (symlinkLevels > 10)
	errAbort("Too many symlinks followed: %d symlinks. Probably a symlink loop.", symlinkLevels);

    // read the symlink
    char *symPath = mustReadSymlink(path, &sb);

    // apply symPath to path
    char *newPath = mustPathRelativeToFile(path, symPath);
    freeMem(lastPath);
    lastPath = path;
    freeMem(symPath);
    path = newPath;
    }
if (result == 0 && ((sb.st_mode & S_IFMT) != S_IFREG))
    errAbort("Expecting regular file. Followed symlinks to %s but it is not a regular file.", path);

*pPath = path;
*pLastPath = lastPath;
*pSymlinkLevels = symlinkLevels;
return result;
}

char *testOriginalSymlink(char *submitFileName, char *submitDir)
/* Follows submitted symlinks to real file.
 * Aborts if real file path starts with cdwRootDir
 * since it should not point to a file already under cdwRoot. */
{
char *lastPath = NULL;
char *path = NULL;
int symlinkLevels = 0;

int result = findSubmitSymlinkExt(submitFileName, submitDir, &path, &lastPath, &symlinkLevels);
if (result == -1)  // path does not exist
    {
    errAbort("path=[%s] does not exist following submitDir/submitFileName through symlinks.", path);
    }
if (startsWith(cdwRootDir, path))
    errAbort("Unexpected operation. The symlink %s points to %s. It should not point to a file already under cdwRoot %s", submitFileName, path, cdwRootDir);
freeMem(lastPath);
return path;
}


void replaceOriginalWithSymlink(char *submitFileName, char *submitDir, char *cdwPath)
/* For a file that was just copied, remove original and symlink to new one instead
 * to save space. Follows symlinks if any to the real file and replaces it with a symlink */
{
char *path = testOriginalSymlink(submitFileName, submitDir);
if (unlink(path) == -1)  // save space
    errnoAbort("unlink failure %s", path);
if (symlink(cdwPath, path) == -1)  // replace with symlink
    errnoAbort("symlink failure from %s to %s", path, cdwPath);
verbose(1, "%s converted to symlink to %s\n", path, cdwPath);
freeMem(path);
}



char *findSubmitSymlink(char *submitFileName, char *submitDir, char *oldPath)
/* Find the last symlink in the chain from submitDir/submitFileName.
 * This is useful for when target of symlink in cdw/ gets renamed 
 * (e.g. license plate after passes validation), or removed (e.g. cdwReallyRemove* commands). */
{
char *lastPath = NULL;
char *path = NULL;
int symlinkLevels = 0;

int result = findSubmitSymlinkExt(submitFileName, submitDir, &path, &lastPath, &symlinkLevels);
if (result == -1)  // path does not exist
    {
    warn("path=[%s] does not exist following submitDir/submitFileName through symlinks.", path);
    return NULL;
    }
if (symlinkLevels < 1)
    {
    warn("Too few symlinks followed: %d symlinks. Where is the symlink created by cdwSubmit?", symlinkLevels);
    return NULL;
    }
if (!sameString(path, oldPath))
    {
    warn("Found symlinks point to %s, expecting to find symlink pointing to old path %s", path, oldPath);
    return NULL;
    }

freeMem(path);
return lastPath;
}


void cdwReallyRemoveFile(struct sqlConnection *conn, char *submitDir, long long fileId, boolean unSymlinkOnly, boolean really)
/* If unSymlinkOnly is NOT specified, removes all records of file from database and from Unix file system if 
 * the really flag is set.  Otherwise just print some info on the file.
 * Tries to find original submitdir and replace symlink with real file to restore it. */
{
struct cdwFile *ef = cdwFileFromId(conn, fileId);
char *path = cdwPathForFileId(conn, fileId);
verbose(1, "%s id=%u, submitFileName=%s, path=%s\n", 
    unSymlinkOnly ? "unlocking" : "removing", ef->id, ef->submitFileName, path);
if (really)
    {
    char query[1024];
    struct cdwSubmit *es = cdwSubmitFromId(conn, ef->submitId);

    if (!unSymlinkOnly)
	{
	cdwRemoveQaRecords(conn, fileId);
	sqlSafef(query, sizeof(query),
	    "delete from cdwGroupFile where fileId=%lld", fileId);
	sqlUpdate(conn, query);
	sqlSafef(query, sizeof(query), "delete from cdwValidFile where fileId=%lld", fileId);
	sqlUpdate(conn, query);
	sqlSafef(query, sizeof(query), "delete from cdwFile where id=%lld", fileId);
	sqlUpdate(conn, query);
	}

    char *lastPath = NULL;
    // skip symlink check if meta or manifest which do not get validated or license plate or symlink
    if (!((fileId == es->manifestFileId) || (fileId == es->metaFileId)))
	lastPath = findSubmitSymlink(ef->submitFileName, submitDir, path);
    if (lastPath)
	{
	verbose(3, "lastPath=%s path=%s\n", lastPath, path);
	if (unlink(lastPath) == -1)  // drop about to be invalid symlink
	    errnoAbort("unlink failure %s", lastPath);
	copyFile(path, lastPath);
	chmod(lastPath, 0664);
	freeMem(lastPath);
	}

    if (!unSymlinkOnly)
	mustRemove(path);
    }
freez(&path);
cdwFileFree(&ef);
}

void cdwFileResetTags(struct sqlConnection *conn, struct cdwFile *ef, char *newTags, 
    boolean revalidate, int submitId)
/* Reset tags on file, strip out old validation and QA,  schedule new validation and QA. */
/* Remove existing QA records and rerun QA agent on given file.   */
{
long long fileId = ef->id;
/* Update database to let people know format revalidation is in progress. */
char query[4*1024];

/* Update tags for file in cdwFile table. */
sqlSafef(query, sizeof(query), "update cdwFile set tags='%s' where id=%lld", newTags, fileId);
sqlUpdate(conn, query);
    
if (revalidate)
    {
    sqlSafef(query, sizeof(query), "update cdwFile set errorMessage = '%s' where id=%lld",
	 "Revalidation in progress.", fileId); 
    sqlUpdate(conn, query);

    /* Get rid of records referring to file in other validation and qa tables. */
    cdwRemoveQaRecords(conn, fileId);

    /* schedule validator */
    cdwAddQaJob(conn, ef->id, submitId);
    }
else
    {
    /* The revalidation case relies on cdwMakeValidFile to update the cdwValidFile table.
     * Here we must do it ourselves. */
    struct cdwValidFile *vf = cdwValidFileFromFileId(conn, ef->id);
    if (vf != NULL)
	{
	struct cgiParsedVars *tags = cdwMetaVarsList(conn, ef);
	cdwValidFileFieldsFromTags(vf, tags);
	cdwValidFileUpdateDb(conn, vf, vf->id);
	cgiParsedVarsFreeList(&tags);
	cdwValidFileFree(&vf);
	}
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
    if (one.core.qual > cdwMinMapQual)
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

void cdwReserveTempFile(char *path)
/* Call mkstemp on path.  This will fill in terminal XXXXXX in path with file name
 * and create an empty file of that name.  Generally that empty file doesn't stay empty for long. */
{
int fd = mkstemp(path);
if (fd == -1)
     errnoAbort("Couldn't create temp file %s", path);
if (fchmod(fd, 0664) == -1)
    errnoAbort("Couldn't change permissions on temp file %s", path);
mustCloseFd(&fd);
}

void cdwBwaIndexPath(struct cdwAssembly *assembly, char indexPath[PATH_LEN])
/* Fill in path to BWA index. */
{
safef(indexPath, PATH_LEN, "%s%s/bwaData/%s.fa", 
    cdwValDataDir, assembly->ucscDb, assembly->ucscDb);
}

void cdwAsPath(char *format, char path[PATH_LEN])
/* Convert something like "narrowPeak" in format to full path involving
 * encValDir/as/narrowPeak.as */
{
safef(path, PATH_LEN, "%sas/%s.as", cdwValDataDir, format);
}

boolean cdwTrimReadsForAssay(char *fastqPath, char trimmedPath[PATH_LEN], char *assay)
/* Look at assay and see if it's one that needs trimming.  If so make a new trimmed
 * file and put file name in trimmedPath.  Otherwise just copy fastqPath to trimmed
 * path and return FALSE. */
{
if (sameString(assay, "long-RNA-seq"))
    {
    char cmd[3*PATH_LEN];
    // Make up temp file name for poly-A trimmed file
    safef(trimmedPath, PATH_LEN, "%scdwFastqPolyFilterXXXXXX", cdwTempDir());
    cdwReserveTempFile(trimmedPath);

    // Run cdwFastqPolyFilter on the new file then pass the output into BWA. 
    safef(cmd, sizeof(cmd), "cdwFastqPolyFilter %s %s", 
	fastqPath, trimmedPath); 
    mustSystem(cmd);
    return TRUE;
    }
else
    {
    strcpy(trimmedPath, fastqPath);
    return FALSE;
    }
}

void cdwCleanupTrimReads(char *fastqPath, char trimmedPath[PATH_LEN])
/* Remove trimmed sample file.  Does nothing if fastqPath and trimmedPath the same. */
{
if (!sameString(fastqPath, trimmedPath))
    remove(trimmedPath);
}

void cdwAlignFastqMakeBed(struct cdwFile *ef, struct cdwAssembly *assembly,
    char *fastqPath, struct cdwValidFile *vf, FILE *bedF,
    double *retMapRatio,  double *retDepth,  double *retSampleCoverage, 
    double *retUniqueMapRatio, char *assay)
/* Take a sample fastq and run bwa on it, and then convert that file to a bed. 
 * bedF and all the ret parameters can be NULL. */
{
// Figure out BWA index
char genoFile[PATH_LEN];
cdwBwaIndexPath(assembly, genoFile);

// Trim reads if need be
char trimmedFile[PATH_LEN];
cdwTrimReadsForAssay(fastqPath, trimmedFile, assay);

// Run BWA alignment
char cmd[3*PATH_LEN], *saiName, *samName;
saiName = cloneString(rTempName(cdwTempDir(), "cdwSample1", ".sai"));
safef(cmd, sizeof(cmd), "bwa aln -t 3 %s %s > %s", genoFile, trimmedFile, saiName);
mustSystem(cmd);

samName = cloneString(rTempName(cdwTempDir(), "ewdSample1", ".sam"));
safef(cmd, sizeof(cmd), "bwa samse %s %s %s > %s", genoFile, saiName, trimmedFile, samName);
mustSystem(cmd);
remove(saiName);

/* Scan sam file to calculate vf->mapRatio, vf->sampleCoverage and vf->depth. 
 * and also to produce little bed file for enrichment step. */
struct genomeRangeTree *grt = genomeRangeTreeNew();
long long hitCount=0, missCount=0, uniqueHitCount, totalBasesInHits=0;
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

// Clean up and go home
cdwCleanupTrimReads(fastqPath, trimmedFile);
genomeRangeTreeFree(&grt);
remove(samName);
}

struct cdwFastqFile *cdwFastqFileFromFileId(struct sqlConnection *conn, long long fileId)
/* Get cdwFastqFile with given fileId or NULL if none such */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwFastqFile where fileId=%lld", fileId);
return cdwFastqFileLoadByQuery(conn, query);
}

struct cdwVcfFile *cdwVcfFileFromFileId(struct sqlConnection *conn, long long fileId)
/* Get cdwVcfFile with given fileId or NULL if none such */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwVcfFile where fileId=%lld", fileId);
return cdwVcfFileLoadByQuery(conn, query);
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

void cdwMakeTempFastqSample(char *source, int size, char dest[PATH_LEN])
/* Copy size records from source into a new temporary dest.  Fills in dest */
{
/* Make temporary file to save us a unique place in file system. */
safef(dest, PATH_LEN, "%scdwSampleFastqXXXXXX", cdwTempDir());
int fd = mustMkstemp(dest);
close(fd);

char command[3*PATH_LEN];
safef(command, sizeof(command), 
    "fastqStatsAndSubsample %s /dev/null %s -smallOk -sampleSize=%d", source, dest, size);
verbose(2, "command: %s\n", command);
mustSystem(command);
}

void cdwMakeFastqStatsAndSample(struct sqlConnection *conn, long long fileId)
/* Run fastqStatsAndSubsample, and put results into cdwFastqFile table. */
{
struct cdwFastqFile *fqf = cdwFastqFileFromFileId(conn, fileId);
if (fqf == NULL)
    {
    char *path = cdwPathForFileId(conn, fileId);
    char statsFile[PATH_LEN], sampleFile[PATH_LEN];
    char command[3*PATH_LEN];
    // Cut adapt on RNA seq files. 
    safef(statsFile, PATH_LEN, "%scdwFastqStatsXXXXXX", cdwTempDir());
    cdwReserveTempFile(statsFile);
    char dayTempDir[PATH_LEN];
    safef(sampleFile, PATH_LEN, "%scdwFastqSampleXXXXXX", cdwTempDirForToday(dayTempDir));
    cdwReserveTempFile(sampleFile);
    // For RNA seq files run on the fastqTrimmed output, otherwise run on the unaltered CDW file.  
    safef(command, sizeof(command), "fastqStatsAndSubsample -sampleSize=%d -smallOk %s %s %s",
	cdwSampleTargetSize, path, statsFile, sampleFile);
    mustSystem(command);
    safef(command, sizeof(command), "gzip -c %s > %s.fastq.gz", sampleFile, sampleFile);
    mustSystem(command);
    strcat(sampleFile, ".fastq.gz");
    fqf = cdwFastqFileOneFromRa(statsFile);
    fqf->fileId = fileId;
    fqf->sampleFileName = cloneString(sampleFile);
    cdwFastqFileSaveToDb(conn, fqf, "cdwFastqFile", 1024);
    remove(statsFile);
    freez(&path);
    }
cdwFastqFileFree(&fqf);
}

struct cdwQaWigSpot *cdwMakeWigSpot(struct sqlConnection *conn, long long wigId, long long spotId)
/* Create a new cdwQaWigSpot record in database based on comparing wig file to spot file
 * (specified by id's in cdwFile table). */
{
/* Get valid files from fileIds and check format */
struct cdwValidFile *wigVf = cdwValidFileFromFileId(conn, wigId);
if (!sameString(wigVf->format, "bigWig"))
    errAbort("%lld is not a bigWig file, is %s instead", wigId, wigVf->format);
struct cdwValidFile *spotVf = cdwValidFileFromFileId(conn, spotId);
if (!sameString(spotVf->format, "narrowPeak") && !sameString(spotVf->format, "broadPeak") &&
    !sameString(spotVf->format, "bigBed"))
    errAbort("%lld is not a recognized peak type format, is %s", spotId, spotVf->format);

/* Remove any old record for files. */
char query[256];
sqlSafef(query, sizeof(query), 
    "delete from cdwQaWigSpot where wigId=%lld and spotId=%lld", wigId, spotId);
sqlUpdate(conn, query);

/* Figure out file names */
char *wigPath = cdwPathForFileId(conn, wigId);
char *spotPath = cdwPathForFileId(conn, spotId);
char statsFile[PATH_LEN];
safef(statsFile, PATH_LEN, "%scdwQaWigSpotXXXXXX", cdwTempDir());
cdwReserveTempFile(statsFile);
char peakFile[PATH_LEN];
safef(peakFile, PATH_LEN, "%scdwQaWigSpotXXXXXX", cdwTempDir());
cdwReserveTempFile(peakFile);

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
struct cdwQaWigSpot *spot = cdwQaWigSpotOneFromRa(statsFile);
spot->wigId = wigId;
spot->spotId = spotId;
cdwQaWigSpotSaveToDb(conn, spot, "cdwQaWigSpot", 1024);
spot->id = sqlLastAutoId(conn);

/* Clean up and go home. */
cdwQaWigSpotFree(&spot);
cdwValidFileFree(&wigVf);
cdwValidFileFree(&spotVf);
freez(&wigPath);
freez(&spotPath);
return spot;
}

struct cdwQaWigSpot *cdwQaWigSpotFor(struct sqlConnection *conn, 
    long long wigFileId, long long spotFileId) 
/* Return wigSpot relationship if any we have in database for these two files. */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select * from cdwQaWigSpot where wigId=%lld and spotId=%lld", wigFileId, spotFileId);
return cdwQaWigSpotLoadByQuery(conn, query);
}




struct cdwBamFile *cdwBamFileFromFileId(struct sqlConnection *conn, long long fileId)
/* Get cdwBamFile with given fileId or NULL if none such */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwBamFile where fileId=%lld", fileId);
return cdwBamFileLoadByQuery(conn, query);
}

struct cdwBamFile * cdwMakeBamStatsAndSample(struct sqlConnection *conn, long long fileId, 
    char sampleBed[PATH_LEN])
/* Run cdwBamStats and put results into cdwBamFile table, and also a sample bed.
 * The sampleBed will be filled in by this routine. */
{
/* Remove any old record for file. */
char query[256];
sqlSafef(query, sizeof(query), "delete from cdwBamFile where fileId=%lld", fileId);
sqlUpdate(conn, query);

/* Figure out file names */
char *path = cdwPathForFileId(conn, fileId);
char statsFile[PATH_LEN];
safef(statsFile, PATH_LEN, "%scdwBamStatsXXXXXX", cdwTempDir());
cdwReserveTempFile(statsFile);
char dayTempDir[PATH_LEN];
safef(sampleBed, PATH_LEN, "%scdwBamSampleXXXXXX", cdwTempDirForToday(dayTempDir));
cdwReserveTempFile(sampleBed);

/* Make system call to make ra and bed, and then another system call to zip bed.*/
char command[3*PATH_LEN];
safef(command, sizeof(command), "edwBamStats -sampleBed=%s -sampleBedSize=%d %s %s",
    sampleBed, cdwSampleTargetSize, path, statsFile);
mustSystem(command);
safef(command, sizeof(command), "gzip %s", sampleBed);
mustSystem(command);
strcat(sampleBed, ".gz");

/* Parse out ra file,  save it to database, and remove ra file. */
struct cdwBamFile *ebf = cdwBamFileOneFromRa(statsFile);
ebf->fileId = fileId;
cdwBamFileSaveToDb(conn, ebf, "cdwBamFile", 1024);
remove(statsFile);

/* Clean up and go home. */
freez(&path);
return ebf;
}

struct cdwVcfFile * cdwMakeVcfStatsAndSample(struct sqlConnection *conn, long long fileId, 
    char sampleBed[PATH_LEN])
/* Run cdwVcfStats and put results into cdwVcfFile table, and also a sample bed.
 * The sampleBed will be filled in by this routine. */
{
/* Remove any old record for file. */
char query[256];
sqlSafef(query, sizeof(query), "delete from cdwVcfFile where fileId=%lld", fileId);
sqlUpdate(conn, query);

/* Figure out file names */
char *path = cdwPathForFileId(conn, fileId);
char statsFile[PATH_LEN];
safef(statsFile, PATH_LEN, "%scdwVcfStatsXXXXXX", cdwTempDir());
cdwReserveTempFile(statsFile);
char dayTempDir[PATH_LEN];
safef(sampleBed, PATH_LEN, "%scdwVcfSampleXXXXXX", cdwTempDirForToday(dayTempDir));
cdwReserveTempFile(sampleBed);

/* Make system call to make ra and bed, and then another system call to zip bed.*/
char command[3*PATH_LEN];
safef(command, sizeof(command), "cdwVcfStats -bed=%s %s %s",
    sampleBed, path, statsFile);
mustSystem(command);
safef(command, sizeof(command), "gzip %s", sampleBed);
mustSystem(command);
strcat(sampleBed, ".gz");

/* Parse out ra file,  save it to database, and remove ra file. */
struct cdwVcfFile *vcf = cdwVcfFileOneFromRa(statsFile);
vcf->fileId = fileId;
cdwVcfFileSaveToDb(conn, vcf, "cdwVcfFile", 1024);
remove(statsFile);

/* Clean up and go home. */
freez(&path);
return vcf;
}

char *cdwOppositePairedEndString(char *end)
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

struct cdwValidFile *cdwOppositePairedEnd(struct sqlConnection *conn, struct cdwFile *ef, struct cdwValidFile *vf)
/* Given one file of a paired end set of fastqs, find the file with opposite ends. */
{
char *otherEnd = cdwOppositePairedEndString(vf->pairedEnd);
char query[1024];
sqlSafef(query, sizeof(query), 
    "select cdwValidFile.* from cdwValidFile join cdwFile on cdwValidFile.fileId=cdwFile.id"
    " where experiment='%s' and submitDirId=%d and outputType='%s' and replicate='%s' "
    " and part='%s' and pairedEnd='%s' and itemCount=%lld and deprecated=''"
    , vf->experiment, ef->submitDirId, vf->outputType, vf->replicate, vf->part, otherEnd
    , vf->itemCount);
struct cdwValidFile *otherVf = cdwValidFileLoadByQuery(conn, query);
if (otherVf == NULL)
    return NULL;
if (otherVf->next != NULL)
    errAbort("Multiple results from pairedEnd query %s", query);
return otherVf;
}

struct cdwQaPairedEndFastq *cdwQaPairedEndFastqFromVfs(struct sqlConnection *conn,
    struct cdwValidFile *vfA, struct cdwValidFile *vfB,
    struct cdwValidFile **retVf1,  struct cdwValidFile **retVf2)
/* Return pair record if any for the two fastq files. */
{
/* Sort the two ends. */
struct cdwValidFile *vf1 = NULL, *vf2 = NULL;
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
    "select * from cdwQaPairedEndFastq where fileId1=%u and fileId2=%u",
    vf1->fileId, vf2->fileId);
return cdwQaPairedEndFastqLoadByQuery(conn, query);
}

FILE *cdwPopen(char *command, char *mode)
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

boolean cdwOneLineSystemAttempt(char *command, char *line, int maxLineSize)
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

void cdwOneLineSystemResult(char *command, char *line, int maxLineSize)
/* Execute system command and return one line result from it in line */
{
if (!cdwOneLineSystemAttempt(command, line, maxLineSize) )
    errAbort("Can't get line from %s", command);
}

void cdwMd5File(char *fileName, char md5Hex[33])
/* call md5sum utility to calculate md5 for file and put result in hex format md5Hex 
 * This ends up being about 30% faster than library routine md5HexForFile,
 * however since there's popen() weird interactions with  stdin involved
 * it's not suitable for a general purpose library.  Environment inside cdw
 * is controlled enough it should be ok. */
{
char command[PATH_LEN + 16];
safef(command, sizeof(command), "md5sum %s", fileName);
char line[2*PATH_LEN];
cdwOneLineSystemResult(command, line, sizeof(line));
memcpy(md5Hex, line, 32);
md5Hex[32] = 0;
}


void cdwPokeFifo(char *fifoName)
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
/* Shared functions for CDW web CGI's.
   Mostly wrappers for javascript tweaks */

void cdwWebAutoRefresh(int msec)
/* Refresh page after msec.  Use 0 to cancel autorefresh */
{
if (msec > 0)
    {
    // set timeout to refresh page (saving/restoring scroll position via cookie)
    printf("<script type='text/javascript'>var cdwRefresh = setTimeout(function() { $.cookie('cdwWeb.scrollTop', $(window).scrollTop()); $('form').submit(); }, %d);</script>", msec);
    puts("<script type='text/javascript'>$(document).ready(function() {$(document).scrollTop($.cookie('cdwWeb.scrollTop'))});</script>");

    // disable autorefresh when user is changing page settings
    puts("<script type='text/javascript'>$('form').click(function() {clearTimeout(cdwRefresh); $.cookie('cdwWeb.scrollTop', null);});</script>");
    }
else if (msec == 0)
    puts("clearTimeout(cdwRefresh);</script>");

// Negative msec ignored
}

/***/
/* Navigation bar */

void cdwWebNavBarStart()
/* Layout navigation bar */
{
puts("<div id='layout'>");
puts("<div id='navbar' class='navbar navbar-fixed-top navbar-inverse'>");
webIncludeFile("/inc/cdwNavBar.html");
puts("</div>");
puts("<div id='content' class='container'><div>");
}

void cdwWebNavBarEnd()
/* Close layout after navigation bar */
{
puts("</div></div></div>");
}

void cdwWebBrowseMenuItem(boolean on)
/* Toggle visibility of 'Browse submissions' link on navigation menu */
{
printf("<script type='text/javascript'>$('#cdw-browse').%s();</script>", on ? "show" : "hide");
}

void cdwWebSubmitMenuItem(boolean on)
/* Toggle visibility of 'Submit data' link on navigation menu */
{
printf("<script type='text/javascript'>$('#cdw-submit').%s();</script>", on ? "show" : "hide");
}

char *cdwRqlLookupField(void *record, char *key)
/* Lookup a field in a tagStanza. */
{
struct tagStanza *stanza = record;
return tagFindVal(stanza, key);
}

boolean cdwRqlStatementMatch(struct rqlStatement *rql, struct tagStanza *stanza,
	struct lm *lm)
/* Return TRUE if where clause and tableList in statement evaluates true for stanza. */
{
struct rqlParse *whereClause = rql->whereClause;
if (whereClause == NULL)
    return TRUE;
else
    {
    struct rqlEval res = rqlEvalOnRecord(whereClause, stanza, cdwRqlLookupField, lm);
    res = rqlEvalCoerceToBoolean(res);
    return res.val.b;
    }
}

static void rBuildStanzaRefList(struct tagStorm *tags, struct tagStanza *stanzaList,
    struct rqlStatement *rql, struct lm *lm, int *pMatchCount, struct slRef **pList)
/* Recursively add stanzas that match query to list */
{
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    if (rql->limit < 0 || rql->limit > *pMatchCount)
	{
	if (cdwRqlStatementMatch(rql, stanza, lm))
	    {
	    refAdd(pList, stanza);
	    *pMatchCount += 1;
	    }
	if (stanza->children != NULL)
	    rBuildStanzaRefList(tags, stanza->children, rql, lm, pMatchCount, pList);
	}
    }
}

void cdwCheckRqlFields(struct rqlStatement *rql, struct slName *tagFieldList)
/* Make sure that rql query only includes fields that exist in tags */
{
struct hash *hash = hashFromSlNameList(tagFieldList);
rqlCheckFieldsExist(rql, hash, "cdwFileTags table");
hashFree(&hash);
}

struct slRef *tagStanzasMatchingQuery(struct tagStorm *tags, char *query)
/* Return list of references to stanzas that match RQL query */
{
struct rqlStatement *rql = rqlStatementParseString(query);
struct slName *tagFieldList = tagStormFieldList(tags);
cdwCheckRqlFields(rql, tagFieldList);
slFreeList(&tagFieldList);
int matchCount = 0;
struct slRef *list = NULL;
struct lm *lm = lmInit(0);
rBuildStanzaRefList(tags, tags->forest, rql, lm, &matchCount, &list);
rqlStatementFree(&rql);
lmCleanup(&lm);
return list;
}

struct cgiParsedVars *cdwMetaVarsList(struct sqlConnection *conn, struct cdwFile *ef)
/* Return list of cgiParsedVars dictionaries for metadata for file.  Free this up 
 * with cgiParsedVarsFreeList() */
{
struct cgiParsedVars *tagsList = cgiParsedVarsNew(ef->tags);
struct cgiParsedVars *parentTags = NULL;
char query[256];
sqlSafef(query, sizeof(query), 
    "select tags from cdwMetaTags where id=%u", ef->metaTagsId);
char *metaCgi = sqlQuickString(conn, query);
if (metaCgi != NULL)
    {
    parentTags = cgiParsedVarsNew(metaCgi);
    tagsList->next = parentTags;
    freez(&metaCgi);
    }
return tagsList;
}

static int gMatchCount = 0;
static boolean gDoSelect = FALSE;
static boolean gFirst = TRUE; 

static void rMatchesToRa(struct tagStorm *tags, struct tagStanza *list, 
    struct rqlStatement *rql, struct lm *lm)
/* Recursively traverse stanzas on list outputting matching stanzas as ra. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (rql->limit < 0 || rql->limit > gMatchCount)
	{
	if (stanza->children)
	    rMatchesToRa(tags, stanza->children, rql, lm);
	else    /* Just apply query to leaves */
	    {
	    if (cdwRqlStatementMatch(rql, stanza, lm))
		{
		++gMatchCount;
		if (gDoSelect)
		    {
		    struct slName *field;
		    for (field = rql->fieldList; field != NULL; field = field->next)
			{
			char *val = tagFindVal(stanza, field->name);
			if (val != NULL)
			    printf("%s\t%s\n", field->name, val);
			}
		    printf("\n");
		    }
		}
	    }
	}
    }
}

static void printQuotedTsv(char *val)
/* Print out tab separated value inside of double quotes. Escape any existing quotes with quotes. */
{
putchar('"');
char c;
while ((c = *val++) != 0)
    {
    if (c == '"')
        putchar(c);
    putchar(c);
    }
putchar('"');
}

static void rMatchesToCsv(struct tagStorm *tags, struct tagStanza *list, 
    struct rqlStatement *rql, struct lm *lm)
/* Recursively traverse stanzas on list outputting matching stanzas as 
 * a comma separated values file. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (rql->limit < 0 || rql->limit > gMatchCount)  // We are inside the acceptable limit
	{
	if (stanza->children) // Recurse till we have just leaves. 
	    rMatchesToCsv(tags, stanza->children, rql, lm);
	else    /* Just apply query to leaves */
	    {
	    if (cdwRqlStatementMatch(rql, stanza, lm))
		{
		++gMatchCount;
		if (gDoSelect)
		    {
		    struct slName *field;
		    if (gFirst)// For the first stanza print out a header line. 
			{
			char *sep = "";
			gFirst = FALSE;
			for (field = rql->fieldList; field != NULL; field = field->next)
			    {
			    printf("%s%s", sep, field->name); 
			    sep = ",";
			    }
			printf("\n"); 
			}
		    char *sep = "";
		    for (field = rql->fieldList; field != NULL; field = field->next)
			{
			fputs(sep, stdout);
			sep = ",";
			char *val = emptyForNull(tagFindVal(stanza, field->name));
			// Check for embedded comma or existing quotes
			if (strchr(val, ',') == NULL && strchr(val, '"') == NULL)
			    fputs(val, stdout);
			else
			    {
			    printQuotedTsv(val);
			    }
			}
		    printf("\n");
		    }
		}
	    }
	}
    }
}

static void rMatchesToTsv(struct tagStorm *tags, struct tagStanza *list, 
    struct rqlStatement *rql, struct lm *lm)
/* Recursively traverse stanzas on list outputting matching stanzas as a tab separated values file. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (rql->limit < 0 || rql->limit > gMatchCount)  // We are inside the acceptable limit
	{
	if (stanza->children) // Recurse till we have just leaves. 
	    rMatchesToTsv(tags, stanza->children, rql, lm);
	else    /* Just apply query to leaves */
	    {
	    if (cdwRqlStatementMatch(rql, stanza, lm))
		{
		++gMatchCount;
		if (gDoSelect)
		    {
		    struct slName *field;
		    if (gFirst)// For the first stanza print out a header line. 
			{
			gFirst = FALSE;
			printf("#"); 
			char *sep = "";
			for (field = rql->fieldList; field != NULL; field = field->next)
			    {
			    printf("%s%s", sep, field->name); 
			    sep = "\t";
			    }
			printf("\n"); 
			}
		    char *sep = "";
		    for (field = rql->fieldList; field != NULL; field = field->next)
			{
			char *val = naForNull(tagFindVal(stanza, field->name));
			printf("%s%s", sep, val);
			sep = "\t";
			}
		    printf("\n");
		    }
		}
	    }
	}
    }
}

void cdwPrintMatchingStanzas(char *rqlQuery, int limit, struct tagStorm *tags, char *format)
/* Show stanzas that match query */
{
struct dyString *dy = dyStringCreate("%s", rqlQuery);
int maxLimit = 10000;
if (limit > maxLimit)
    limit = maxLimit;
struct rqlStatement *rql = rqlStatementParseString(dy->string);

/* Get list of all tag types in tree and use it to expand wildcards in the query
 * field list. */
struct slName *allFieldList = tagStormFieldList(tags);
slSort(&allFieldList, slNameCmpCase);
rql->fieldList = wildExpandList(allFieldList, rql->fieldList, TRUE);
/* Traverse tag tree outputting when rql statement matches in select case, just
 * updateing count in count case. */
gDoSelect = sameWord(rql->command, "select");
if (gDoSelect)
    rql->limit = limit;
struct lm *lm = lmInit(0);
if (sameString(format, "ra"))
    rMatchesToRa(tags, tags->forest, rql, lm);
else if (sameString(format, "tsv"))
    rMatchesToTsv(tags, tags->forest, rql, lm); 
else if (sameString(format, "csv"))
    rMatchesToCsv(tags, tags->forest, rql, lm);
if (sameWord(rql->command, "count"))
    printf("%d\n", gMatchCount);
}

static struct dyString *getLoginBits(struct cart *cart)
/* Get a little HTML fragment that has login/logout bit of menu */
{
/* Construct URL to return back to this page */
char *command = cartUsualString(cart, "cdwCommand", "home");
char *sidString = cartSidUrlString(cart);
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "/cgi-bin/cdwWebBrowse?cdwCommand=%s&%s",
    command, sidString );
char *encodedReturn = cgiEncode(returnUrl);

/* Write a little html into loginBits */
struct dyString *loginBits = dyStringNew(0);
dyStringAppend(loginBits, "<li id=\"query\"><a href=\"");
char *userName = wikiLinkUserName();
if (userName == NULL)
    {
    dyStringPrintf(loginBits, "../cgi-bin/hgLogin?hgLogin.do.displayLoginPage=1&returnto=%s&%s",
	    encodedReturn, sidString);
    dyStringPrintf(loginBits, "\">Login</a></li>");
    }
else
    {
    dyStringPrintf(loginBits, "../cgi-bin/hgLogin?hgLogin.do.displayLogout=1&returnto=%s&%s",
	    encodedReturn, sidString);
    dyStringPrintf(loginBits, "\" id=\"logoutLink\">Logout %s</a></li>", userName);

    if (loginUseBasicAuth())
        wikiFixLogoutLinkWithJs();
    }

/* Clean up and go home */
freez(&encodedReturn);
return loginBits;
}

char *cdwLocalMenuBar(struct cart *cart, boolean makeAbsolute)
/* Return menu bar string. Optionally make links in menubar to point to absolute URLs, not relative. */
{
struct dyString *loginBits = getLoginBits(cart);

// menu bar html is in a stringified .h file
struct dyString *dy = dyStringNew(4*1024);
dyStringPrintf(dy, 
#include "cdwNavBar.h"
       , loginBits->string);


char *menubarStr = menuBarAddUiVars(dy->string, "/cgi-bin/cdw", cartSidUrlString(cart));
if (!makeAbsolute)
    return menubarStr;

char *menubarStr2 = replaceChars(menubarStr, "../", "/");
freez(&menubarStr);
return menubarStr2;
}

char *fileExtFromFormat(char *format)
/* return file extension given the cdwFile format as defined in cdwValid.c. Result has to be freed */
{
if (sameWord(format, "vcf"))
    return cloneString(".vcf.gz");
if (sameWord(format, "fasta"))
    return cloneString(".fa.gz");
if (sameWord(format, "fastq"))
    return cloneString(".fastq.gz");
if (sameWord(format, "unknown"))
    return cloneString("");

return catTwoStrings(".", format);
}
