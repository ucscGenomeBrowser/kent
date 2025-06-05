/* hooklib.c - functions common to all the different tusd hooks */

/* Copyright (C) 2014 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "wikiLink.h"
#include "customTrack.h"
#include "userdata.h"
#include "jsonQuery.h"
#include "jsHelper.h"
#include "errCatch.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "hooklib.h"

char *prettyFileSize(long size)
/* Return a string representing the size of a file */
{
char buf[32];
sprintWithGreekByte(buf, sizeof(buf), size);
return cloneString(buf);
}

char *encodePath(char *path)
/* Return a string where each individual component of a '/' separated
 * string has been cgiEncoded, but not the '/' chars themselves */
{
int maxSeps = 256;
char *pathArr[maxSeps]; // errAbort if more than maxSeps subdirs
char *copy = cloneString(path);
int numChops = chopString(copy, "/", pathArr, maxSeps);
if (numChops >= maxSeps)
    errAbort("Too many subdirectories. Fix filesystem layout of upload and try again");
struct dyString *ret = dyStringNew(0);
int i = 0;
for (; i < numChops; i++)
    {
    // we can ignore .. and . in paths, it is an error if hubtools is creating these names
    // don't errAbort right now because hubtools does send things like 'hubName/.'
    // as a parentDir, but that should be fixed soon
    if (sameString(pathArr[i], ".") || sameString(pathArr[i], ".."))
        {
        continue;
        }
    dyStringPrintf(ret, "%s/", cgiEncodeFull(pathArr[i]));
    }
return dyStringCannibalize(&ret);
}

char *setUploadPath(char *userName, char *fileName, char *parentDir, char *serverName, boolean forceOverwrite)
/* return the path, relative to hg.conf tusdDataDir, where we will store this upload
 * ensures all subdirectories on the final path will exist, and then returns
 * machineName/userPrefix/userName/parentDir/randomKey
 * NOTE: This must be a relative path or tusd will complain */
{
char *dataDir = getDataDir(userName, serverName);
struct dyString *fullFilePath = dyStringNew(0);
struct dyString *retPath = dyStringNew(0);
// if parentDir provided we are throwing the files in there
if (parentDir)
    {
    char *encodedParentDir = encodePath(parentDir);
    if (!endsWith(encodedParentDir, "/"))
        encodedParentDir = catTwoStrings(encodedParentDir, "/");
    dataDir = catTwoStrings(dataDir, encodedParentDir);
    }
dyStringPrintf(fullFilePath, "%s%s", dataDir, fileName);

fprintf(stderr, "DEBUG: setUploadPath of '%s' to '%s'\n", fileName, dyStringContents(fullFilePath));
// TODO: check if file exists or not and let user choose to overwrite
// and re-call this hook, for now just exit if the file exists
// hubtools uploads always overwrite because we assume those users
// know what they are doing
if (fileExists(dyStringContents(fullFilePath)) && !forceOverwrite)
    {
    errAbort("file '%s' exists already, not overwriting", dyStringContents(fullFilePath));
    }
else
    {
    // since we are returning a ChangeFileInfo response in pre-create, tusd will write
    // the uploaded file into the users directory for us, ensure the subdirs exist
    int oldUmask = 00;
    if (!isDirectory(dataDir))
        {
        fprintf(stderr, "making directory '%s'\n", dataDir);
        // the directory needs to be 777 for apache, ignore umask for now
        oldUmask = umask(0);
        makeDirsOnPath(dataDir);
        // restore umask
        umask(oldUmask);
        }
    // now we can construct the path relative to tusd uploadDir
    dyStringPrintf(retPath, "%s/%s/%s/%s", serverName, getEncodedUserNamePath(userName), parentDir, fileName);
    return dyStringCannibalize(&retPath);
    }
// on error return NULL
return NULL;
}

void fillOutHttpResponseError(struct jsonElement *response)
{
fprintf(stderr, "http response error!\n");
}

void fillOutHttpResponseSuccess(struct jsonElement *response)
{
fprintf(stderr, "http response success!\n");
// DEBUG: comment out after a few releases
jsonPrintToFile(response, NULL, stderr, 0);
}

struct jsonElement *makeDefaultResponse()
/* Create the default response json with some fields pre-filled */
{
struct hash *defHash = hashNew(0);
struct jsonElement *response = newJsonObject(defHash);
// only the HTTP Response object is important to have by default, the other
// fields will be created as needed
struct jsonElement *httpResponse = newJsonObject(hashNew(0));
jsonObjectAdd(httpResponse, HTTP_STATUS, newJsonNumber(200)); // default to a successful response
jsonObjectAdd(httpResponse, HTTP_BODY, newJsonString(""));
struct jsonElement *header = newJsonObject(hashNew(0));
jsonObjectAdd(header, HTTP_CONTENT_TYPE, newJsonString(HTTP_CONTENT_TYPE_STR));
jsonObjectAdd(httpResponse, HTTP_HEADER, header);
jsonObjectAdd(response, HTTP_NAME, httpResponse);
return response;
}

void rejectUpload(struct jsonElement *response, char *msg, ...)
/* Set the keys for stopping an upload */
{
// first set the necessary keys to reject the request
jsonObjectAdd(response, REJECT_SETTING, newJsonBoolean(TRUE));
jsonObjectAdd(response, STOP_SETTING, newJsonBoolean(TRUE));

// now format the message
va_list args;
va_start(args, msg);
struct dyString *ds = dyStringNew(0);
dyStringVaPrintf(ds, msg, args);
va_end(args);
// find the HTTPResponse object and fill it out with msg:
struct jsonElement *httpResponse = jsonFindNamedField(response, "", HTTP_NAME);
jsonObjectAdd(httpResponse, HTTP_STATUS, newJsonNumber(500));
jsonObjectAdd(httpResponse, HTTP_BODY, newJsonString(dyStringCannibalize(&ds)));
}

boolean isFileTypeRecognized(char *fileName)
/* Return true if this file one of our recognized types */
{
return TRUE;
}

