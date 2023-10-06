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

char *getUserName()
{
return (loginSystemEnabled() || wikiLinkEnabled()) ? wikiLinkUserName() : NULL;
}

static char *getDataDir(char *userName)
/* Return the full path to the user specific data directory, can be configured via hg.conf
 * on hgwdev, this is /data/apache/userdata/userStore/hash/userName/
 * on the RR, this is /userdata/userStore/hash/userName/ */
{
char *userDataBaseDir = cfgOption("userDataDir");
if (!userDataBaseDir  || isEmpty(userDataBaseDir))
    errAbort("hgCustom: trying to save user file but no userDataDir defined in hg.conf");
if (userDataBaseDir[0] != '/')
    errAbort("config setting userDataDir must be an absolute path (starting with '/')");

char *encUserName = cgiEncode(userName);
char *userPrefix = md5HexForString(encUserName);
userPrefix[2] = '\0';

struct dyString *newDataDir = dyStringNew(0);
dyStringPrintf(newDataDir, "%s/%s/%s/", 
    userDataBaseDir, userPrefix, encUserName);

return dyStringCannibalize(&newDataDir);
}

void removeTrack()
/* Removes a custom track for this user */
{
//char *userName = getUserName();
}

void uploadTrack()
/* Saves a new track to the persistent storage for this user */
{
//char *userName = getUserName();
}

struct userFiles *getUserFiles()
/* Return the list of:
 *   - Only if logged in:
 *       - custom tracks in saved sessions
 *       - non-public hubs in saved sessions
 *       - any other files stored in the per user directory
 *   - custom tracks in non-saved sessions
 *   - non-public hubs in non-saved sessions
 * Present data as a plain array for a table view */
{
char *userName = getUserName();
if (userName)
    {
    return NULL;
    }
// throw in the custom tracks that are in the current cart that may not be saved yet
// throw in the attached hubs that are non-public hubs
return NULL;
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
