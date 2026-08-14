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
#include "jsonWrite.h"
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

char *normalizeParentDir(char *parentDir)
/* Return parentDir with any surrounding whitespace trimmed off, or NULL if it was NULL.
 * The pre-create and pre-finish hooks each build paths from this metadata value
 * independently, so they have to normalize it identically or they end up disagreeing
 * about which directory the upload belongs in */
{
if (parentDir == NULL)
    return NULL;
return trimSpaces(cloneString(parentDir));
}

boolean isValidParentDir(char *parentDir)
/* Return TRUE if every '/' separated component of parentDir holds only alphanumeric,
 * period or underscore characters. NULL or empty is invalid, every upload belongs to
 * a hub. Mirrors the same named check in hgMyData.js, which the browser does first,
 * but hubtools and any other tus client come straight here */
{
if (isEmpty(parentDir))
    return FALSE;
if (startsWith("/", parentDir) || endsWith(parentDir, "/"))
    return FALSE;
int maxSeps = 256;
char *pathArr[maxSeps];
char *copy = cloneString(parentDir);
int numChops = chopString(copy, "/", pathArr, maxSeps);
int i = 0;
for (; i < numChops; i++)
    {
    char *component = pathArr[i];
    if (isEmpty(component) || sameString(component, ".") || sameString(component, ".."))
        return FALSE;
    char *c = component;
    for (; *c != '\0'; c++)
        {
        if (!isalnum((unsigned char)*c) && *c != '.' && *c != '_')
            return FALSE;
        }
    }
return TRUE;
}

char *setUploadPath(char *userName, char *fileName, char *parentDir, boolean forceOverwrite)
/* return the path, relative to hg.conf tusdDataDir, where we will store this upload
 * ensures all subdirectories on the final path will exist, and then returns
 * userPrefix/userName/parentDir/fileName
 * NOTE: This must be a relative path or tusd will complain  */
{
char *dataDir = getDataDir(userName);
struct dyString *fullFilePath = dyStringNew(0);
struct dyString *retPath = dyStringNew(0);
// if parentDir provided we are throwing the files in there. Stays empty when there
// is no parentDir so the path we return below is still correct
char *encodedParentDir = "";
if (parentDir)
    {
    encodedParentDir = encodePath(parentDir);
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
    // now we can construct the path relative to tusd uploadDir. This must use the
    // same encoded parentDir as the dataDir we just made above: if it doesn't, tusd
    // creates the unencoded directory itself when it writes the file, leaving two
    // directories for one hub and the file in the one we never made
    dyStringPrintf(retPath, "%s/%s%s", getEncodedUserNamePath(userName), encodedParentDir, fileName);
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

void setUploadedFileList(struct jsonElement *response, char *userName, struct hubSpace *fileList)
/* Put the hubSpace rows this upload created or changed into the response body. tusd
 * forwards the body to the client, which shows the rows the server actually holds */
{
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
hubSpaceWriteFileList(jw, userName, fileList);
jsonWriteObjectEnd(jw);
struct jsonElement *httpResponse = jsonMustFindNamedField(response, "", HTTP_NAME);
jsonObjectAdd(httpResponse, HTTP_BODY, newJsonString(dyStringContents(jw->dy)));
jsonWriteFree(&jw);
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
struct jsonElement *httpResponse = jsonMustFindNamedField(response, "", HTTP_NAME);
jsonObjectAdd(httpResponse, HTTP_STATUS, newJsonNumber(500));
jsonObjectAdd(httpResponse, HTTP_BODY, newJsonString(dyStringCannibalize(&ds)));
}

boolean isFileTypeRecognized(char *fileName)
/* Return true if this file one of our recognized types */
{
return TRUE;
}

