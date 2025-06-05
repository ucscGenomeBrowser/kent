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

void fillOutHttpResponseError()
{
fprintf(stderr, "http response error!\n");
}

void fillOutHttpResponseSuccess()
{
fprintf(stderr, "http response success!\n");
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

