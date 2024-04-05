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

char *getUserName()
{
return (loginSystemEnabled() || wikiLinkEnabled()) ? wikiLinkUserName() : NULL;
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

void removeFileForUser(char *fname, char *userName)
/* Remove a file for this user if it exists */
{
// The file to remove must be prefixed by the hg.conf userDataDir
if (!startsWith(getDataDir(userName), fname))
    return;
if (fileExists(fname))
    mustRemove(fname);
}

void uploadTrack()
/* Saves a new track to the persistent storage for this user */
{
//char *userName = getUserName();
}

struct userFiles *listFilesForUser(char *userName)
/* Get all the files for a particular user */
{
struct userFiles *userListing;
AllocVar(userListing);
char *path = getDataDir(userName);
struct fileInfo *fiList = listDirX(path,NULL,FALSE);
userListing->userName = userName;
userListing->file = fiList;
return userListing;
}

long long checkUserQuota(char *userName)
/* Return the amount of space a user is currently using */
{
long long quota = 0;
struct userFiles *ufList = listFilesForUser(userName);
struct fileInfo *fi;
if (ufList)
    {
    for (fi = ufList->file; fi != NULL; fi = fi->next)
        {
        quota += fi->size;
        }
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
