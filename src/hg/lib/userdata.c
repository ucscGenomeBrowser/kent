/* userdata.c - code for managing data stored on a per user basis */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */


#include "common.h"
#include "hash.h"
#include "portable.h"
#include "trashDir.h"
#include "md5.h"
#include "hgConfig.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "customFactory.h"
#include "wikiLink.h"
#include "userdata.h"
#include "jksql.h"
#include "hdb.h"
#include "hubSpace.h"
#include "hubSpaceQuotas.h"
#include "errCatch.h"
#include <limits.h>

char *getUserName()
/* Query the right system for the users name */
{
return (loginSystemEnabled() || wikiLinkEnabled()) ? wikiLinkUserName() : NULL;
}

char *emailForUserName(char *userName)
/* Fetch the email for this user from gbMembers hgcentral table */
{
struct sqlConnection *sc = hConnectCentral();
struct dyString *query = sqlDyStringCreate("select email from gbMembers where userName = '%s'", userName);
char *email = sqlQuickString(sc, dyStringCannibalize(&query));
hDisconnectCentral(&sc);
// this should be freeMem'd:
return email;
}

char *getEncodedUserNamePath(char *userName)
/* Compute the path for just the userName part of the users upload */
{
struct dyString *ret = dyStringNew(0);
if (!userName)
    return NULL;
char *encUserName = cgiEncode(userName);
char *userPrefix = md5HexForString(encUserName);
userPrefix[2] = '\0';
dyStringPrintf(ret, "%s/%s", userPrefix, encUserName);
return dyStringCannibalize(&ret);
}

// make this a global so if we have to repeatedly call stripDataDir()
// we only need to check the filesystem for path validity once
static char *dataDir = NULL;

static char *setDataDir(char *userName)
/* Set the dataDir value based on hg.conf and the userName. Use realpath to make sure
 * the directory exists and resolve the path if it is a symlink. Return the final
 * path for convenience */
{
char *tusdDataBaseDir = cfgOption("tusdDataDir");
if (!tusdDataBaseDir  || isEmpty(tusdDataBaseDir))
    errAbort("trying to save user file but no tusdDataDir defined in hg.conf");
if (tusdDataBaseDir[0] != '/')
    errAbort("config setting tusdDataDir must be an absolute path (starting with '/')");

// the tusdDataBaseDir may be a symlink, so canonicalize it, but do not include
// the userName part since it may not exist yet.
char *canonicalPath = needMem(PATH_MAX);
char *retValue = realpath(tusdDataBaseDir, canonicalPath);
if (!retValue)
    {
    // realpath returned NULL, check if we need to swap with a mounted filesystem
    char *swapped = swapDataDir(userName, tusdDataBaseDir);
    retValue = realpath(swapped, canonicalPath);
    if (!retValue)
        errAbort("cannot resolve tusdDataDir nor tusdMountPoint");
    }

char *encUserName = cgiEncode(userName);
char *userPrefix = md5HexForString(encUserName);
userPrefix[2] = '\0';

// now that we have a canonicalized the path we need to add a '/' back on
// so the rest of the routines can append to this result
struct dyString *newDataDir = dyStringNew(0);
dyStringPrintf(newDataDir, "%s/%s/%s/",
    canonicalPath, userPrefix, encUserName);

dataDir = dyStringCannibalize(&newDataDir);
return dataDir;
}

char *getDataDir(char *userName)
/* Return the full path to the user specific data directory, can be configured via hg.conf
 * on hgwdev, this is /data/tusd */
{
if (!dataDir)
    setDataDir(userName);

return dataDir;
}

char *swapDataDir(char *userName, char *in)
/* Try replacing the current dataDir with what is defined in hg.conf:tusdMountPoint as
 * the data server may be somewhere else and mounted over NFS. In this case, when
 * tusd saves files, it is writing it's local tusdDataDir value into the hgcentral
 * file location. When the CGI running somewhere else needs to verify file existence,
 * the tusdDataDir won't exist on the CGI filesystem, but will instead be mounted as some
 * different path.  In this case, replace tusdDataDir with tusdMountPoint */
{
char *ret = cloneString(in);
char *tusdDataDir = cfgOption("tusdDataDir");
char *tusdMountPoint = cfgOption("tusdMountPoint");
if (tusdMountPoint && !isEmpty(tusdMountPoint))
    {
    ret = replaceChars(in, tusdDataDir, tusdMountPoint);
    }
return ret;
}

char *unswapDataDir(char *userName, char *in)
/* Opposite of swapDataDir, for trusting that the other system string is 
 * correct. Used for deleting the row in the table for a file which has
 * tusdDataDir as the prefix, not the tusdMountPoint. */
{
char *ret = cloneString(in);
char *tusdDataDir = cfgOption("tusdDataDir");
char *tusdMountPoint = cfgOption("tusdMountPoint");
if (tusdMountPoint && !isEmpty(tusdMountPoint))
    {
    ret = replaceChars(in, tusdMountPoint, tusdDataDir);
    }
return ret;
}

char *stripDataDir(char *fname, char *userName)
/* Strips the getDataDir(userName) off of fname. The dataDir may be a symbolic
 * link, or on a different filesystem. */
{
getDataDir(userName);
if (!dataDir)
    {
    // catch a realpath error
    return NULL;
    }
int prefixSize = strlen(dataDir);
if (startsWith(dataDir, fname))
    {
    char *ret = fname + prefixSize;
    return ret;
    }
// may be calling from a different server than the files are actually
// residing, try swapping with hg.conf values
char *mountedFilePath = swapDataDir(userName, fname);
if (startsWith(dataDir, mountedFilePath))
    {
    int prefixSize = strlen(dataDir);
    return mountedFilePath + prefixSize;
    }
return NULL;
}

char *getHubDataDir(char *userName, char *hub)
{
char *dataDir = getDataDir(userName);
return catTwoStrings(dataDir, cgiEncode(hub));
}

char *hubSpaceUrl = NULL;
static char *getHubSpaceUrl()
{
if (!hubSpaceUrl)
    hubSpaceUrl = cfgOption("hubSpaceUrl");
return hubSpaceUrl;
}

char *webDataDir(char *userName)
/* Return a web accesible path to the userDataDir, this is different from the full path tusd uses */
{
char *retUrl = NULL;
if (userName)
    {
    char *encUserName = cgiEncode(userName);
    char *userPrefix = md5HexForString(encUserName);
    userPrefix[2] = '\0';
    struct dyString *userDirDy = dyStringNew(0);
    dyStringPrintf(userDirDy, "%s/%s/%s/", getHubSpaceUrl(), userPrefix, encUserName);
    retUrl = dyStringCannibalize(&userDirDy);
    }
return retUrl;
}

char *urlForFile(char *userName, char *filePath)
/* Return a web accessible URL to filePath */
{
char *webDataUrl = webDataDir(userName);
if (webDataUrl)
    {
    return catTwoStrings(webDataUrl, filePath);
    }
return NULL;
}

char *prefixUserFile(char *userName, char *fname, char *parentDir)
/* Allocate a new string that contains the full per-user path to fname. return NULL if
 * we cannot construct a full path because of a realpath(3) failure.
 * parentDir is optional and will go in between the per-user dir and the fname */
{
char *pathPrefix = getDataDir(userName);
char *path = NULL;
if (pathPrefix)
    {
    if (parentDir)
        {
        struct dyString *ret = dyStringCreate("%s%s%s%s", pathPrefix, parentDir, lastChar(parentDir) == '/' ? "" : "/", fname);
        path = dyStringCannibalize(&ret);
        }
    else
        path = catTwoStrings(pathPrefix, fname);
    char canonicalPath[PATH_MAX];
    realpath(path, canonicalPath);
    // after canonicalizing the path, make sure it starts with the userDataDir, to prevent
    // deleting files like blah/../../../../systemFile.text
    if (startsWith(pathPrefix, canonicalPath))
        return cloneString(canonicalPath);
    }
return NULL;
}

static boolean checkHubSpaceRowExists(struct hubSpace *row)
/* Return TRUE if row already exists */
{
struct sqlConnection *conn = hConnectCentral();
struct dyString *queryCheck = sqlDyStringCreate("select count(*) from hubSpace where userName='%s' and fileName='%s' and parentDir='%s'", row->userName, row->fileName, row->parentDir);
int ret = sqlQuickNum(conn, dyStringCannibalize(&queryCheck));
hDisconnectCentral(&conn);
return ret > 0;
}

static boolean checkHubSpaceLocationExists(char *userName, char *location)
/* Return TRUE if location exists for userName and has exactly one row */
{
struct sqlConnection *conn = hConnectCentral();
struct dyString *queryCheck = sqlDyStringCreate("select count(*) from hubSpace where userName='%s' and location='%s'", userName, location);
int ret = sqlQuickNum(conn, dyStringCannibalize(&queryCheck));
hDisconnectCentral(&conn);
return ret == 1;
}

char *hubNameFromPath(char *path)
/* Return the last directory component of path. Assume that a '.' char in the last component
 * means that component is a filename and go back further */
{
char *copy = cloneString(path);
if (endsWith(copy, "/"))
    trimLastChar(copy);
char *ptr = strrchr(copy, '/');
// check to see if we're in a file name, like /blah/blah/name/hub.txt
if (ptr)
    {
    if (strchr(ptr, '.'))
        {
        *ptr = 0;
        ptr = strrchr(copy, '/');
        }
    if (ptr)
        {
        ++ptr;
        return cloneString(ptr);
        }
    }
return copy;
}

void addHubSpaceRowForFile(struct hubSpace *row)
/* We created a file for a user, now add an entry to the hubSpace table for it */
{
struct sqlConnection *conn = hConnectCentral();

// now write out row to hubSpace table
if (!sqlTableExistsOnMain(conn, "hubSpace"))
    {
    errAbort("No hubSpace MySQL table is present. Please send an email to genome-www@soe.ucsc.edu  describing the exact steps you took just before you got this error");
    }
hubSpaceSaveToDb(conn, row, "hubSpace", 0);
hDisconnectCentral(&conn);
}

void makeParentDirRows(char *userName, time_t lastModified, char *db, char *parentDirStr, char *userDataDir)
/* For each '/' separated component of parentDirStr, create a row in hubSpace. Return the 
 * final subdirectory component of parentDirStr */
{
int i, slashCount = countChars(parentDirStr, '/');
char *components[256];
struct dyString *currLocation = dyStringCreate("%s", userDataDir);
int foundSlashes = chopByChar(cloneString(parentDirStr), '/', components, slashCount);
if (foundSlashes > 256)
    errAbort("parentDir setting '%s' too long", parentDirStr);
for (i = 0; i < foundSlashes; i++)
    {
    char *subdir = components[i];
    if (sameString(subdir, "."))
        continue;
    if (!subdir)
        errAbort("error: empty subdirectory components for parentDir string '%s'", parentDirStr);
    if (lastChar(dyStringContents(currLocation)) != '/')
        dyStringAppendC(currLocation, '/');
    dyStringAppend(currLocation, subdir);
    struct hubSpace *row = NULL;
    AllocVar(row);
    row->userName = userName;
    row->fileName = subdir;
    row->fileSize = 0;
    row->fileType = "dir";
    row->creationTime = NULL;
    row->lastModified = sqlUnixTimeToDate(&lastModified, TRUE);
    row->db = db;
    row->location = cloneString(dyStringContents(currLocation));
    row->md5sum = "";
    row->parentDir = i > 0 ? components[i-1] : "";
    // only insert a row for this parentDir if it's unique to the table
    if (!checkHubSpaceRowExists(row))
        addHubSpaceRowForFile(row);
    }
}

char *writeHubText(char *path, char *userName, char *db)
/* Create a hub.txt file, optionally creating the directory holding it. For convenience, return
 * the file name of the created hub, which can be freed. */
{
int oldUmask = 00;
oldUmask = umask(0);
makeDirsOnPath(path);
// restore umask
umask(oldUmask);
// now make the hub.txt with some basic information
char *hubFile = NULL;
struct dyString *hubFileDy = dyStringCreate("%s%shub.txt", path, endsWith(path, "/") ? "" : "/");
hubFile = dyStringCannibalize(&hubFileDy);
if (fileExists(hubFile))
    return hubFile;

char *hubName = hubNameFromPath(path);
FILE *f = mustOpen(hubFile, "w");
fprintf(f, "hub %s\n"
    "email %s\n"
    "shortLabel %s\n"
    "longLabel %s\n"
    "useOneFile on\n"
    "\n"
    "genome %s\n"
    "\n",
    hubName, emailForUserName(userName), hubName, hubName, db);
carefulClose(&f);
return hubFile;
}

static char *hubPathFromParentDir(char *parentDir, char *userDataDir)
/* Assume parentDir does not have leading '/' or '.', parse out the first dir component
 * and add it to the users directory*/
{
char *copy = cloneString(parentDir);
char *firstSlash = strchr(copy, '/');
if (!firstSlash)
    {
    return copy;
    }
firstSlash = 0;
return catTwoStrings(userDataDir, copy);
}

static void writeTrackStanza(char *hubFileName, char *track, char *bigDataUrl, char *type, char *label, char *bigFileLocation)
{
if ( (sameString(type, "bamIndex") || sameString(type, "tabixIndex") || sameString(type, "text")) )
    // don't need to make track stanzas for these supporting files
    return;

FILE *f = mustOpen(hubFileName, "a");
char *trackDbType = type;
if (sameString(type, "bigBed"))
    {
    // don't errAbort if the file is actually not a bigBed
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        // figure out the type based on the bbiFile header
        struct bbiFile *bbi = bigBedFileOpen(bigFileLocation);
        char tdbType[32];
        safef(tdbType, sizeof(tdbType), "bigBed %d%s", bbi->definedFieldCount, bbi->fieldCount > bbi->definedFieldCount ? " +" : "");
        trackDbType = tdbType;
        bigBedFileClose(&bbi);
        }
    errCatchEnd(errCatch);
    errCatchFree(&errCatch);
    // NOTE: if the file was not actually a bigBed (and so bigBedFileOpen errAborted), we
    // just want to prevent the errAbort, not prevent creating the stanza itself, as that
    // would be majorly confusing to the user, so just continue on here
    }
fprintf(f, "track %s\n"
    "bigDataUrl %s\n"
    "type %s\n"
    "shortLabel %s\n"
    "longLabel %s\n"
    "\n",
    track, bigDataUrl, trackDbType, label, label);
carefulClose(&f);
}

static char *writeHubStanzasForFile(struct hubSpace *rowForFile, char *userDataDir, char *parentDir)
/* Create a hub.txt (if necessary) and add track stanzas for the file described by rowForFile.
 * Returns the path to the hub.txt */
{
char *hubFileName = NULL;
char *hubDir = hubPathFromParentDir(rowForFile->parentDir, userDataDir);
hubFileName = writeHubText(hubDir, rowForFile->userName, rowForFile->db);

// NOTE: even though rowForFile->fileName was already cgiEncoded by the pre-finish hook,
// we still must cgiEncode again to make the bigDataUrl setting work, as apache needs
// to look for a literal '%' in a filename if there was a character encoded. For example,
// if the filename from tus was &.bb, tus encodes this to "\u0026.bb", which we write to
// disk as %5Cu0026.bb, and apache needs to find at:
// https://url/hash/userName/%25Cu0026.bb in order to work in hgTracks
writeTrackStanza(hubFileName, rowForFile->fileName, cgiEncodeFull(rowForFile->fileName), rowForFile->fileType, rowForFile->fileName, rowForFile->location);
return hubFileName;
}

void createNewTempHubForUpload(char *requestId, struct hubSpace *rowForFile, char *userDataDir, char *parentDir)
/* Creates a hub.txt for this upload, and updates the hubSpace table for the
 * hub.txt and any parentDirs we need to create. */
{
// first create the hub.txt if necessary and write the stanza for this track
char *hubPath = writeHubStanzasForFile(rowForFile, userDataDir, parentDir);

// update the mysql table with a record of the hub.txt:
struct hubSpace *hubTextRow = NULL;
AllocVar(hubTextRow);
hubTextRow->userName = rowForFile->userName;
hubTextRow->fileName = "hub.txt";
hubTextRow->fileSize = fileSize(hubPath);
hubTextRow->fileType = "hub.txt";
hubTextRow->creationTime = NULL;
time_t lastModTime = fileModTime(hubPath);
hubTextRow->lastModified = sqlUnixTimeToDate(&lastModTime, TRUE);
hubTextRow->db = rowForFile->db;
hubTextRow->location = hubPath;
hubTextRow->md5sum = md5HexForFile(hubPath);
hubTextRow->parentDir = hubNameFromPath(hubPath);
if (!checkHubSpaceRowExists(hubTextRow))
    addHubSpaceRowForFile(hubTextRow);
}

static void deleteHubSpaceRow(char *fname, char *userName)
/* Deletes a row from the hubspace table for a given fname */
{
struct sqlConnection *conn = hConnectCentral();
struct dyString *deleteQuery = sqlDyStringCreate("delete from hubSpace where location='%s' and userName='%s'", fname, userName);
sqlUpdate(conn, dyStringCannibalize(&deleteQuery));
hDisconnectCentral(&conn);
}

void removeFileForUser(char *fname, char *userName)
/* Remove a file for this user if it exists */
{
// The file to remove must be prefixed by the hg.conf userDataDir
char canonicalPath[PATH_MAX];
realpath(fname, canonicalPath);
if (!startsWith(getDataDir(userName), canonicalPath))
    return;
if (fileExists(canonicalPath))
    {
    // delete the actual file
    mustRemove(canonicalPath);

    // delete the table row, which probably has the location based
    // on the other filesystem
    if (checkHubSpaceLocationExists(userName, canonicalPath))
        deleteHubSpaceRow(canonicalPath, userName);
    else
        {
        char *unswapped = unswapDataDir();
        if (checkHubSpaceLocationExists(userName, unswapped))
            deleteHubSpaceRow(unswapped, userName);
        }
    }
// TODO: we should also modify the hub.txt associated with this file
}

struct hubSpace *listFilesForUser(char *userName)
/* Return the files the user has uploaded */
{
struct sqlConnection *conn = hConnectCentral();
struct dyString *query = sqlDyStringCreate("select userName, fileName, fileSize, fileType, creationTime, DATE_FORMAT(lastModified, '%%c/%%d/%%Y, %%l:%%i:%%s %%p') as lastModified, db, location, md5sum, parentDir from hubSpace where userName='%s' order by location,creationTime", userName);
struct hubSpace *fileList = hubSpaceLoadByQuery(conn, dyStringCannibalize(&query));
hDisconnectCentral(&conn);
return fileList;
}

#define defaultHubName "defaultHub"
char *defaultHubNameForUser(char *userName)
/* Return a name to use as a default for a hub, starts with defaultHub, then defaultHub2, ... */
{
if (!userName)
    return defaultHubName;
struct dyString *query = sqlDyStringCreate("select distinct(fileName) from hubSpace where parentDir='' and fileName like '%s%%' and userName='%s'", defaultHubName, userName);
struct sqlConnection *conn = hConnectCentral();
struct slName *hubNames = sqlQuickList(conn, dyStringCannibalize(&query));;
hDisconnectCentral(&conn);
if (hubNames == NULL)
    // user has no hubs created
    return defaultHubName;
slSort(&hubNames,slNameCmpStringsWithEmbeddedNumbers);
slReverse(&hubNames);
// now the first element of the list has the most recent integer to use (or no integer)
char *currHubName = cloneString(hubNames->name);
int defaultLen = strlen(defaultHubName);
if (sameString(currHubName,defaultHubName))
    // probably a common case
    return "defaultHub2";
else
    {
    currHubName[defaultLen-1] = 0;
    currHubName += strlen(defaultHubName);
    eraseNonDigits(currHubName);
    if (strlen(currHubName) == 0)
        {
        // user has a hub like defaultHubblah, just assume defaultHub2 is ok instead of
        // going further and trying to figure out the next hub number
        return "defaultHub2";
        }
    else
        {
        int hubNum = sqlUnsigned(currHubName) + 1;
        struct dyString *hubName = dyStringCreate("%s%d", defaultHubName, hubNum);
        return dyStringCannibalize(&hubName);
        }
    }
}

long long getMaxUserQuota(char *userName)
/* Return how much space is allocated for this user or the default */
{
long long specialQuota = quotaForUserName(userName);
return specialQuota == 0 ? HUB_SPACE_DEFAULT_QUOTA : specialQuota;
}

long long checkUserQuota(char *userName)
/* Return the amount of space a user is currently using */
{
long long quota = 0;
struct hubSpace *hubSpace, *hubSpaceList = listFilesForUser(userName);
for (hubSpace = hubSpaceList; hubSpace != NULL; hubSpace = hubSpace->next)
    {
    quota += hubSpace->fileSize;
    }
return quota;
}
