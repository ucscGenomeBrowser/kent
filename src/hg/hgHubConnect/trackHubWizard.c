/* trackHubWizard -- a user interface for creating a track hubs configuration files */

/* Copyright (C) 2019 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "cart.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "hgConfig.h"
#include "md5.h"
#include "trashDir.h"
#include "hgHubConnect.h"
#include "jsHelper.h"
#include "web.h"
#include "wikiLink.h"
#include "customTrack.h"
#include "userdata.h"
#include "jsonWrite.h"
#include "cartJson.h"
#include "hubSpace.h"
#include "hubSpaceKeys.h"
#include "hubConnect.h"
#include "trackHub.h"
#include "htmshell.h"
#include "geoMirror.h"
#include <limits.h>
#include "errCatch.h"

void removeOneFile(char *userName, char *cgiFileName, char *fullPath, char *db, char *fileType)
/* Remove one single file for userName */
{
// prefixUserFile returns a canonicalized path, or NULL if the
// canonicalized path does not begin with the hg.conf specified userDataDir
// TODO: make the debug information from stderr go to stdout so the user
// can know there is a mistake somewhere, and only print the debug
// information in the event that the filename actually begins with the
// userDataDir so we don't tell hackers what files do and do not exist
char *fileName = prefixUserFile(userName, fullPath, NULL);
if (fileName)
    {
    if (fileExists(fileName))
        {
        fprintf(stderr, "deleting file: '%s'\n", fileName);
        removeFileForUser(fileName, userName);
        fflush(stderr);
        }
    else
        {
        fprintf(stderr, "file '%s' does not exist\n", fileName);
        fflush(stderr);
        }
    }
}

int pathDepth(char *path)
{
// replace multiple occurences of '/' with just a single one to get a canonical path
// as path///to/file and path/to/file are the same path on Linux
char *deduped = replaceChars(path, "//", "/");
return countChars(deduped, '/');
}

int sortByFullPathCmp(const void *va, const void *vb)
/* Compare two fullPaths */
{
struct jsonElement *a = (struct jsonElement *)(*(struct slRef **)va)->val;
struct jsonElement *b = (struct jsonElement *)(*(struct slRef **)vb)->val;
char *aFullpath = jsonStringField(a, "fullPath");
char *bFullpath = jsonStringField(b, "fullPath");
int aDepth = pathDepth(aFullpath);
int bDepth = pathDepth(bFullpath);
// ensure subdirectories order before their parents:
if (aDepth != bDepth)
    return bDepth - aDepth;
// if equal depth than lexicographic sort is fine
return strcmp(jsonStringField(a,"fullPath"), jsonStringField(b, "fullPath"));
}

void sortByFullPath(struct jsonElement *listJson)
{
slSort(&(listJson->val.jeList), sortByFullPathCmp);
}

void doRemoveFile(struct cartJson *cj, struct hash *paramHash)
/* Process the request to remove a file */
{
char *userName = getUserName();
if (userName)
    {
    // our array of objects, each object represents a track file
    struct jsonElement *deleteJson = hashFindVal(paramHash, "fileList");
    struct slRef *copy, *f, *fileList = deleteJson->val.jeList;
    struct jsonElement *dirListJsonEle = newJsonList(NULL);
    jsonWriteListStart(cj->jw, "deletedList");
    for (f = fileList; f != NULL; )
        {
        struct jsonElement *fileObj = (struct jsonElement *)f->val;
        char *fileName = jsonStringField(fileObj, "fileName");
        char *fileType = jsonStringField(fileObj, "fileType");
        char *db = jsonStringField(fileObj, "genome");
        char *fullPath = jsonStringField(fileObj, "fullPath");
        copy = f->next;
        if (sameString(fileType, "dir"))
            {
            f->next = NULL;
            jsonListAdd(dirListJsonEle, fileObj);
            }
        else
            {
            if (sameString(fileType, "hub.txt"))
                {
                // disconnect this hub from the cart if it exists
                char *hubUrl = urlForFile(userName, fullPath);
                char *hubId = hubNameFromUrl(hubUrl);
                if (hubId)
                    {
                    /* remove the cart variable */
                    hubId += 4; // skip past the hub_ part
                    char buffer[1024];
                    safef(buffer, sizeof buffer, "hgHubConnect.hub.%s", hubId);
                    cartRemove(cj->cart, buffer);
                    }
                }
            removeOneFile(userName, fileName, fullPath, db, fileType);
            // write out the fullPath so the DataTable can remove the correct row:
            jsonWriteString(cj->jw, NULL, fullPath);
            }
        f = copy;
        }
    // now attempt to delete any requested directories, but don't die if they still have contents
    sortByFullPath(dirListJsonEle);
    struct slRef *dir = NULL;
    for (dir = dirListJsonEle->val.jeList; dir != NULL; dir = dir->next)
        {
        struct jsonElement *fileObj = (struct jsonElement *)dir->val;
        char *fileName = jsonStringField(fileObj, "fileName");
        char *fileType = jsonStringField(fileObj, "fileType");
        char *db = jsonStringField(fileObj, "genome");
        char *fullPath = jsonStringField(fileObj, "fullPath");
        removeOneFile(userName, fileName, fullPath, db, fileType);
        // write out the fullPath so the DataTable can remove the correct row:
        jsonWriteString(cj->jw, NULL, fullPath);
        }
    jsonWriteListEnd(cj->jw);
    }
}

void doMoveFile(struct cartJson *cj, struct hash *paramHash)
/* Move a file to a new hub */
{
}

static void outUiDataForUser(struct jsonWrite *jw)
/* List out the currently stored files for the user as well as other data
 * needed to create the hubSpace table */
{
char *userName = getUserName();
jsonWriteObjectStart(jw, "userFiles");
if (userName)
    {
    // the url for this user:
    jsonWriteString(jw, "userUrl", webDataDir(userName));
    hubSpaceWriteFileList(jw, userName, listFilesForUser(userName));
    }
jsonWriteBoolean(jw, "isLoggedIn", getUserName() ? TRUE : FALSE);
jsonWriteString(jw, "hubNameDefault", defaultHubNameForUser(getUserName()));
// if the user is not logged, the 0 for the quota is ignored
jsonWriteNumber(jw, "userQuota", getUserName() ? checkUserQuota(getUserName()) : 0);
jsonWriteNumber(jw, "maxQuota", getUserName() ? getMaxUserQuota(getUserName()) : HUB_SPACE_DEFAULT_QUOTA);
jsonWriteObjectEnd(jw);
}

void getHubSpaceUIState(struct cartJson *cj, struct hash *paramHash)
/* Get all the data we need to make a users hubSpace UI table. The cartJson library
 * deals with printing the json */
{
outUiDataForUser(cj->jw);
}

static void reportSyncResults(struct slPair *results)
/* Log what each peer said about the sync.  A peer that refuses the request answers with a
 * body carrying an "error", which is not a connection failure and so has to be looked at
 * here, or a misconfigured mirror would look exactly like a working one.  stderr rather
 * than warn(): this is between servers, and the person who clicked Generate has no way to
 * act on it, while an admin reading error_log does. */
{
struct slPair *result;
for (result = results; result != NULL; result = result->next)
    {
    char *body = (char *)result->val;
    if (body == NULL)
        continue;   // already logged by geoMirrorNotifyOtherNodes
    if (stringIn("\"error\"", body) || !stringIn("\"synced\"", body))
        fprintf(stderr, "hgHubSyncApiKey: %s refused the api key sync: %s\n",
                result->name, body);
    }
}

static void syncApiKeyToOtherNodes(char *userName, char *apiKey)
/* Tell every other geo mirror node about this user's new key (apiKey non-NULL) or that it was
 * revoked (apiKey NULL), so a key generated on any UCSC mirror works on all of them.  Best
 * effort: a peer that is slow, down or refusing is logged and skipped, never fails the local
 * action, which has already succeeded by the time this is called. */
{
if (!cfgOptionBooleanDefault("syncHubApiKeys", FALSE))
    return;
char *apiKeyOrEmpty = apiKey ? apiKey : "";
long now = (long)time(NULL);
char nowString[32];
safef(nowString, sizeof nowString, "%ld", now);
char *sig = hubSpaceApiKeySyncSig(userName, apiKeyOrEmpty, now);
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
jsonWriteObjectStart(jw, hgHubSyncApiKey);
jsonWriteString(jw, "userName", userName);
jsonWriteString(jw, "apiKey", apiKeyOrEmpty);
// a string, not a number, so that both sides sign and check the same characters
jsonWriteString(jw, "time", nowString);
jsonWriteString(jw, "sig", sig);
jsonWriteObjectEnd(jw);
jsonWriteObjectEnd(jw);
struct slPair *cgiVars = slPairNew(CARTJSON_COMMAND, jw->dy->string);
struct slPair *results = geoMirrorNotifyOtherNodes("hgHubConnect", cgiVars);
reportSyncResults(results);
slPairFreeValsAndList(&results);
slPairFree(&cgiVars);
jsonWriteFree(&jw);
}

static void syncApiKeyBestEffort(char *userName, char *apiKey)
/* syncApiKeyToOtherNodes() in an errCatch of its own.  The local generate or revoke has
 * already succeeded and been written into the response by the time we get here, so an
 * errAbort in the sync (login.cookieSalt unset, say) must not escape: cartJsonExecute's
 * outer catch would throw the response away and replace it with an error, leaving the user
 * looking at a failure over a key that is sitting in the database. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    syncApiKeyToOtherNodes(userName, apiKey);
errCatchEnd(errCatch);
if (errCatch->gotError)
    fprintf(stderr, "hgHubSyncApiKey: could not sync the api key to the other mirrors: %s\n",
            errCatch->message->string);
errCatchFree(&errCatch);
}

void cjRevokeApiKey(struct cartJson *cj, struct hash *paramHash)
/* Wrapper for cartJson to call lib function revokeApiKey, removes any api keys for the user */
{
struct errCatch *errCatch = errCatchNew();
char *userName = getUserName();
if (errCatchStart(errCatch))
    {
    hubSpaceRevokeApiKey(userName);
    }
errCatchEnd(errCatch);
if (!(errCatch->gotError))
    {
    jsonWriteString(cj->jw, "revoke", "true");
    syncApiKeyBestEffort(userName, NULL);
    }
else
    jsonWriteStringf(cj->jw, "error", "revokeApiKey() error: '%s'", errCatch->message->string);
errCatchFree(&errCatch);
}

void cjGenerateApiKey(struct cartJson *cj, struct hash *paramHash)
/* Wrapper for cartJson to call lib function generateApiKey, makes a random (but not crypto-secure api key for use of hubtools to upload to hubspace, or for skipping cloudflare */
{
struct errCatch *errCatch = errCatchNew();
char *apiKey = NULL;
char *userName = getUserName();

if (errCatchStart(errCatch))
    {
    apiKey = hubSpaceGenerateApiKey(userName);
    }

errCatchEnd(errCatch);
if (apiKey)
    {
    jsonWriteString(cj->jw, "apiKey", apiKey);
    syncApiKeyBestEffort(userName, apiKey);
    }
else if (errCatch->gotError)
    jsonWriteStringf(cj->jw, "error", "generateApiKey() error: '%s'", errCatch->message->string);
errCatchFree(&errCatch);
}

static void syncApiKeyFromPeer(struct jsonWrite *jw, char *userName, char *apiKey,
                               char *timeStamp, char *sig)
/* Adopt an api key (or a revocation, when apiKey is empty) that a peer geo mirror is telling
 * us about, so the key works the same on every UCSC mirror.  Never notify the other nodes
 * from in here, or the mirrors would keep re-notifying each other forever.  Refuses the
 * request unless sig proves it was signed, recently, with this site's login.cookieSalt,
 * which every geo mirror of a site already shares.  Writes the answer into jw. */
{
if (!cfgOptionBooleanDefault("syncHubApiKeys", FALSE))
    {
    jsonWriteString(jw, "error", "hgHubSyncApiKey: not enabled on this site");
    return;
    }
if (isEmpty(userName) || apiKey == NULL || isEmpty(timeStamp) || isEmpty(sig))
    {
    jsonWriteString(jw, "error", "hgHubSyncApiKey: need userName, apiKey, time and sig");
    return;
    }
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    // One message for a bad signature and for a stale one, and nothing about which it was:
    // a caller guessing at the salt learns nothing from the answer it gets back
    if (!hubSpaceApiKeySyncSigOk(userName, apiKey, timeStamp, sig))
        errAbort("hgHubSyncApiKey: bad signature");
    if (isNotEmpty(apiKey))
        hubSpaceSetApiKey(userName, apiKey);
    else
        hubSpaceRevokeApiKey(userName);
    }
errCatchEnd(errCatch);
if (!(errCatch->gotError))
    jsonWriteString(jw, "synced", "true");
else
    jsonWriteStringf(jw, "error", "hgHubSyncApiKey error: '%s'", errCatch->message->string);
errCatchFree(&errCatch);
}

boolean doApiKeySyncIfRequested()
/* If this request is a peer geo mirror telling us about an api key, answer it and return TRUE,
 * else return FALSE and leave the request to the normal cart path.
 *   This runs before the cart exists, and has to.  A peer's request carries no hguid cookie
 * and no apiKey= variable, so building a cart for it would hand it a captcha page instead of
 * running this (forceUserIdOrCaptcha in cart.c) on any site that sets cloudFlareSiteKey --
 * which is every site that needs api keys in the first place -- and would leave behind a junk
 * userDb and sessionDb row for every sync besides.  There is no user session here to build a
 * cart from: the request is signed, not logged in.
 *   Any other cartJson command sent along in the same request is ignored, so this path can
 * only ever reach the one handler. */
{
char *commandJson = cgiOptionalString(CARTJSON_COMMAND);
// cheap test first, so an ordinary cartJson request is not parsed twice
if (isEmpty(commandJson) || !stringIn(hgHubSyncApiKey, commandJson))
    return FALSE;

struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
struct errCatch *errCatch = errCatchNew();
boolean isSync = FALSE;
if (errCatchStart(errCatch))
    {
    struct jsonElement *commandObj = jsonParse(commandJson);
    struct jsonElement *syncObj = jsonFindNamedField(commandObj, "", hgHubSyncApiKey);
    if (syncObj != NULL)
        {
        isSync = TRUE;
        syncApiKeyFromPeer(jw, jsonOptionalStringField(syncObj, "userName", NULL),
                           jsonOptionalStringField(syncObj, "apiKey", NULL),
                           jsonOptionalStringField(syncObj, "time", NULL),
                           jsonOptionalStringField(syncObj, "sig", NULL));
        }
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    // the string only looked like a sync command: say so rather than falling through to the
    // cart path, which would try to run whatever this is a second time
    isSync = TRUE;
    jsonWritePopToLevel(jw, 1);
    jsonWriteStringf(jw, "error", "hgHubSyncApiKey: %s", errCatch->message->string);
    }
errCatchFree(&errCatch);

if (isSync)
    {
    cgiPrintContentType("text/javascript");
    jsonWriteObjectEnd(jw);
    puts(jw->dy->string);
    }
jsonWriteFree(&jw);
return isSync;
}


void doTrackHubWizard(char *database)
/* Offer an upload form so users can upload all their hub files */
{
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("lodash.3.10.0.compat.min.js", NULL);
jsIncludeFile("cart.js", NULL);
jsIncludeFile("autocompleteCat.js",NULL);
webIncludeResourceFile("font-awesome.min.css");
webIncludeResourceFile("dataTables-2.2.2.min.css");
jsIncludeFile("dataTables-2.2.2.min.js", NULL);
webIncludeResourceFile("dataTables.buttons-3.2.2.min.css");
jsIncludeFile("dataTables.buttons-3.2.2.min.js", NULL);
webIncludeResourceFile("dataTables.select-3.0.0.min.css");
jsIncludeFile("dataTables.select-3.0.0.min.js", NULL);
puts("<link href=\"https://releases.transloadit.com/uppy/v4.5.0/uppy.min.css\" rel=\"stylesheet\">");
puts("<script type=\"text/javascript\" src=\"https://releases.transloadit.com/uppy/v4.5.0/uppy.min.js\"></script>");
jsIncludeFile("hgMyData.js", NULL);

// the skeleton HTML:
webIncludeFile("inc/hgMyData.html");
webIncludeResourceFile("hgMyData.css");

jsInlineF("\nvar isLoggedIn = %s;\n", getUserName() ? "true" : "false");
jsInlineF("\nvar cartDb=\"%s %s\";\n", trackHubSkipHubName(hGenome(database)), database);
jsInlineF("\nvar tusdEndpoint=\"%s\";\n", cfgOptionDefault("hubSpaceTusdEndpoint", NULL));
jsInlineF("\nvar fileListEndpoint=\"%shgHubConnect\";\n", hLoginHostCgiBinUrl());
jsInlineF("\nvar loginHost=\"http%s://%s\";\n", loginUseHttps() ? "s" : "", wikiLinkHost());
jsInlineF("\nvar hubGenomeCollisionErrFrag=\"%s\";\n", HUB_GENOME_COLLISION_ERR_FRAG);
jsInline("$(document).ready(function() {\nhubCreate.init();\n})");
puts("</div>");
}
