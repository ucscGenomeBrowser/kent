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

char *getDataDir(char *userName)
/* Return the full path to the user specific data directory, can be configured via hg.conf
 * on hgwdev, this is /data/apache/userdata/userStore/hash/userName/
 * on the RR, this is /userdata/userStore/hash/userName/ */
{
char *userDataBaseDir = cfgOption("userDataDir");
if (!userDataBaseDir  || isEmpty(userDataBaseDir))
    errAbort("trying to save user file but no userDataDir defined in hg.conf");
if (userDataBaseDir[0] != '/')
    errAbort("config setting userDataDir must be an absolute path (starting with '/')");

char *encUserName = cgiEncode(userName);
char *userPrefix = md5HexForString(encUserName);
userPrefix[2] = '\0';

struct dyString *newDataDir = dyStringNew(0);
dyStringPrintf(newDataDir, "%s/%s/%s/", 
    userDataBaseDir, userPrefix, encUserName);

fprintf(stderr, "userDataDir = '%s'\n", newDataDir->string);
return dyStringCannibalize(&newDataDir);
}

char *getHubDataDir(char *userName, char *hub)
{
char *dataDir = getDataDir(userName);
return catTwoStrings(dataDir, hub);
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
    dyStringPrintf(userDirDy, "%s/%s/%s/", HUB_SPACE_URL, userPrefix, encUserName);
    retUrl = dyStringCannibalize(&userDirDy);
    }
return retUrl;
}

char *prefixUserFile(char *userName, char *fname)
/* Allocate a new string that contains the full per-user path to fname, NULL otherwise */
{
char *pathPrefix = getDataDir(userName);
if (pathPrefix)
    return catTwoStrings(pathPrefix, fname);
else
    return NULL;
}

void addNewFileForUser(char *userName, char *fileName, long long fileSize, char *fileType,
        time_t lastModified, char *hubName, char *db, char *location)
/* We created a file for a user, now add an entry to the hubSpace table for it */
{
struct hubSpace *row = NULL;
AllocVar(row);
row->userName = userName;
row->fileName = fileName;
row->fileSize = fileSize;
row->fileType = fileType;
row->creationTime = NULL; // automatically handled by mysql
row->lastModified = sqlUnixTimeToDate(&lastModified, TRUE);
row->hubNameList = hubName;
row->db = db;
row->location = location;
row->md5sum = md5HexForFile(row->location);
struct sqlConnection *conn = hConnectCentral();

// now write out row to hubSpace table
if (!sqlTableExistsOnMain(conn, "hubSpace"))
    {
    errAbort("No hubSpace MySQL table is present. Please send an email to us describing the steps you took just before you got this error");
    }
struct dyString *sqlUpdateStmt = dyStringNew(0);
sqlDyStringPrintf(sqlUpdateStmt, "insert into hubSpace values ('%s', '%s', %llu, "
        "'%s', NULL, '%s', '', '%s', '%s', '%s')",
        row->userName, row->fileName, row->fileSize, row->fileType,
        row->lastModified, row->db, row->location, row->md5sum);
fprintf(stderr, "%s\n", sqlUpdateStmt->string);
fflush(stderr);
sqlUpdate(conn, sqlUpdateStmt->string);
hubSpaceFree(&row);
}

static void deleteHubSpaceRow(char *fname)
/* Deletes a row from the hubspace table for a given fname */
{
struct sqlConnection *conn = hConnectCentral();
struct dyString *deleteQuery = sqlDyStringCreate("delete from hubSpace where location='%s'", fname);
sqlUpdate(conn, dyStringCannibalize(&deleteQuery));
}

void removeFileForUser(char *fname, char *userName)
/* Remove a file for this user if it exists */
{
// The file to remove must be prefixed by the hg.conf userDataDir
if (!startsWith(getDataDir(userName), fname))
    return;
if (fileExists(fname))
    {
    // delete the actual file
    mustRemove(fname);
    // delete the table row
    deleteHubSpaceRow(fname);
    }
}

void removeHubForUser(char *path, char *userName)
/* Remove a hub directory for this user (and all files in the directory), if it exists */
{
if (!startsWith(getDataDir(userName), path))
    return;
if (isDirectory(path))
    {
    struct fileInfo *f, *flist = listDirX(path, NULL, TRUE);
    for (f = flist; f != NULL; f = f->next)
        mustRemove(f->name);
    // now we have deleted all the files in the dir we can safely rmdir
    mustRemove(path);
    deleteHubSpaceRow(path);
    }
}

void uploadTrack()
/* Saves a new track to the persistent storage for this user */
{
//char *userName = getUserName();
}

struct userFiles *listFilesForUserHub(char *userName, char *hubName)
/* Get all the files for a particular hub for a particular user */
{
struct userFiles *userListing;
AllocVar(userListing);
char *path = getHubDataDir(userName, hubName);
struct fileInfo *fiList = listDirX(path,NULL,FALSE);
userListing->userName = userName;
userListing->fileList = fiList;
return userListing;
}

struct userHubs *listHubsForUser(char *userName)
/* Lists the directories for a particular user */
{
struct userHubs *userHubs = NULL;
char *path = getDataDir(userName);
struct fileInfo *fi, *fiList = listDirX(path,NULL,FALSE);
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    if (fi->isDir)
        {
        struct userHubs *hub;
        AllocVar(hub);
        hub->hubName = cloneString(fi->name);
        hub->userName = cloneString(userName);
        char hubPath[PATH_LEN];
        safef(hubPath, sizeof(hubPath), "%s%s", path, fi->name);
        struct userFiles *hubFileList = listFilesForUserHub(userName, hub->hubName);
        hub->fileList = hubFileList;
        slAddHead(&userHubs, hub);
        }
    }
return userHubs;
}

struct hubSpace *listFilesForUser(char *userName)
/* Return the files the user has uploaded */
{
struct sqlConnection *conn = hConnectCentral();
struct dyString *query = sqlDyStringCreate("select * from hubSpace where userName='%s' order by creationTime, fileName", userName);
struct hubSpace *fileList = hubSpaceLoadByQuery(conn, dyStringCannibalize(&query));
return fileList;
}

long long getMaxUserQuota(char *userName)
/* Return how much space is allocated for this user or the default */
{
return HUB_SPACE_DEFAULT_QUOTA;
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

char *storeUserFile(char *userName, char *newFileName, void *data, size_t dataSize)
/* Give a fileName and a data stream, write the data to:
 * userDataDir/hashedUserName/userName/fileName
 * where userDataDir comes from hg.conf and
 * hashedUserName is based on the md5sum of the userName
 * to prevent proliferation of too many directories.
 *
 * After sucessfully saving the file, return a web accessible url
 * to the file. */
{
char *userDir = getDataDir(userName);
makeDirsOnPath(userDir);
char *pathToFile = catTwoStrings(userDir, newFileName);
FILE *newFile = mustOpen(pathToFile, "wb");
// the data will start with a line feed so get rid of that
mustWrite(newFile, data, dataSize);
// missing an EOF?
carefulClose(&newFile);
return pathToFile;
}
