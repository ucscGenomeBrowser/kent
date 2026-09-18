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

static void syncApiKeyToOtherNodes(char *userName, char *apiKey)
/* Tell every other geo mirror node about this user's new key (apiKey non-NULL) or that it was
 * revoked (apiKey NULL), so a key generated on any UCSC mirror works on all of them.  Best
 * effort: a peer that is slow or down is logged and skipped, never fails the local action,
 * which has already succeeded by the time this is called. */
{
if (!cfgOptionBooleanDefault("syncHubApiKeys", FALSE))
    return;
char *apiKeyOrEmpty = apiKey ? apiKey : "";
char *sig = hubSpaceApiKeySyncSig(userName, apiKeyOrEmpty);
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
jsonWriteObjectStart(jw, hgHubSyncApiKey);
jsonWriteString(jw, "userName", userName);
jsonWriteString(jw, "apiKey", apiKeyOrEmpty);
jsonWriteString(jw, "sig", sig);
jsonWriteObjectEnd(jw);
jsonWriteObjectEnd(jw);
struct slPair *cgiVars = slPairNew(CARTJSON_COMMAND, jw->dy->string);
geoMirrorNotifyOtherNodes("hgHubConnect", cgiVars);
slPairFree(&cgiVars);
jsonWriteFree(&jw);
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
    syncApiKeyToOtherNodes(userName, NULL);
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
    syncApiKeyToOtherNodes(userName, apiKey);
    }
else if (errCatch->gotError)
    jsonWriteStringf(cj->jw, "error", "generateApiKey() error: '%s'", errCatch->message->string);
errCatchFree(&errCatch);
}

void cjSyncApiKey(struct cartJson *cj, struct hash *paramHash)
/* Adopt an api key (or a revocation) that a peer geo mirror is telling us about, so the key
 * works the same on every UCSC mirror.  Only ever called by geoMirrorNotifyOtherNodes() on
 * another mirror -- never call syncApiKeyToOtherNodes() from in here, or mirrors would keep
 * re-notifying each other forever. Rejects the request unless sig proves it was signed with
 * this site's login.cookieSalt, which every geo mirror of a site already shares. */
{
if (!cfgOptionBooleanDefault("syncHubApiKeys", FALSE))
    {
    jsonWriteString(cj->jw, "error", "hgHubSyncApiKey: not enabled on this site");
    return;
    }
char *userName = cartJsonRequiredParam(paramHash, "userName", cj->jw, "hgHubSyncApiKey");
char *apiKey = cartJsonRequiredParam(paramHash, "apiKey", cj->jw, "hgHubSyncApiKey");
char *sig = cartJsonRequiredParam(paramHash, "sig", cj->jw, "hgHubSyncApiKey");
if (!userName || !apiKey || !sig)
    return;

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    char *expectedSig = hubSpaceApiKeySyncSig(userName, apiKey);
    if (!sameString(sig, expectedSig))
        errAbort("hgHubSyncApiKey: bad signature");
    if (isNotEmpty(apiKey))
        hubSpaceSetApiKey(userName, apiKey);
    else
        hubSpaceRevokeApiKey(userName);
    }
errCatchEnd(errCatch);
if (!(errCatch->gotError))
    jsonWriteString(cj->jw, "synced", "true");
else
    jsonWriteStringf(cj->jw, "error", "hgHubSyncApiKey error: '%s'", errCatch->message->string);
errCatchFree(&errCatch);
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
