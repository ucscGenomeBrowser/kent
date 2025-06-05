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
#include <limits.h>

static char *getMachineName()
/* What is the machine we are running on? */
{
// what server are we running on? this becomes a subdirectory for each upload
if (hIsPrivateHost())
    return "hgwdev";
else if (hIsBetaHost())
    return "hgwbeta";
else if (hIsPreviewHost())
    return "preview";
else
    {
    if (hHostHasPrefix("genome"))
        return hHttpHost();
    else
        // RR machines need one name (genome), genome-euro and genome-asia can go through
        return "genome";
    }
}

void removeOneFile(char *userName, char *cgiFileName, char *fullPath, char *db, char *fileType, char *serverName)
/* Remove one single file for userName */
{
// prefixUserFile returns a canonicalized path, or NULL if the
// canonicalized path does not begin with the hg.conf specified userDataDir
// TODO: make the debug information from stderr go to stdout so the user
// can know there is a mistake somewhere, and only print the debug
// information in the event that the filename actually begins with the
// userDataDir so we don't tell hackers what files do and do not exist
char *fileName = prefixUserFile(userName, fullPath, NULL, serverName);
if (fileName)
    {
    if (fileExists(fileName))
        {
        fprintf(stderr, "deleting file: '%s'\n", fileName);
        removeFileForUser(fileName, userName, serverName);
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
char *serverName = getMachineName();
if (!serverName)
    {
    char *commandJson = cgiOptionalString(CARTJSON_COMMAND);
    errAbort("Error: no hostname for request. Please email genome-www@soe.ucsc.edu with your"
            " username and the following text so we can debug:\n%s", commandJson);
    }
if (userName)
    {
    // our array of objects, each object represents a track file
    struct jsonElement *deleteJson = hashFindVal(paramHash, "fileList");
    char *reqServerName = cartJsonRequiredParam(paramHash, "serverName", cj->jw, NULL);
    if (!sameString(serverName, reqServerName))
        {
        errAbort("doRemoveFile: requested serverName does not match");
        }
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
                char *hubUrl = urlForFile(userName, fullPath, serverName);
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
            removeOneFile(userName, fileName, fullPath, db, fileType, serverName);
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
        removeOneFile(userName, fileName, fullPath, db, fileType, serverName);
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
char *machName = getMachineName();
jsonWriteObjectStart(jw, "userFiles");
if (userName)
    {
    // the url for this user:
    jsonWriteString(jw, "userUrl", webDataDir(userName, machName));
    jsonWriteListStart(jw, "fileList");
    struct hubSpace *file, *fileList = listFilesForUser(userName);
    for (file = fileList; file != NULL; file = file->next)
        {
        jsonWriteObjectStart(jw, NULL);
        jsonWriteString(jw, "fileName", file->fileName);
        jsonWriteNumber(jw, "fileSize", file->fileSize);
        jsonWriteString(jw, "fileType", file->fileType);
        jsonWriteString(jw, "parentDir", file->parentDir);
        jsonWriteString(jw, "genome", file->db);
        jsonWriteString(jw, "lastModified", file->lastModified);
        jsonWriteString(jw, "uploadTime", file->creationTime);
        jsonWriteString(jw, "fullPath", stripDataDir(file->location, userName, machName));
        jsonWriteString(jw, "md5sum", file->md5sum);
        jsonWriteObjectEnd(jw);
        }
    jsonWriteListEnd(jw);
    }
jsonWriteString(jw, "serverName", machName);
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

void doTrackHubWizard(char *database)
/* Offer an upload form so users can upload all their hub files */
{
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("lodash.3.10.0.compat.min.js", NULL);
jsIncludeFile("cart.js", NULL);
jsIncludeFile("autocompleteCat.js",NULL);
puts("<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/font-awesome/4.5.0/css/font-awesome.min.css\">\n");
puts("<link rel=\"stylesheet\" type=\"text/css\" "
    "href=\"https://cdn.datatables.net/2.2.2/css/dataTables.dataTables.min.css\">\n");
puts("<script type=\"text/javascript\" "
    "src=\"https://cdn.datatables.net/2.2.2/js/dataTables.min.js\"></script>");
puts("<link rel=\"stylesheet\" type=\"text/css\" "
    "href=\"https://cdn.datatables.net/buttons/3.2.2/css/buttons.dataTables.min.css\">\n");
puts("<script type=\"text/javascript\" "
    "src=\"https://cdn.datatables.net/buttons/3.2.2/js/dataTables.buttons.min.js\"></script>");
puts("<link rel=\"stylesheet\" type=\"text/css\" "
    "href=\"https://cdn.datatables.net/select/3.0.0/css/select.dataTables.min.css\">\n");
puts("<script type=\"text/javascript\" "
    "src=\"https://cdn.datatables.net/select/3.0.0/js/dataTables.select.min.js\"></script>");
puts("<link href=\"https://releases.transloadit.com/uppy/v4.5.0/uppy.min.css\" rel=\"stylesheet\">");
puts("<script type=\"text/javascript\" src=\"https://releases.transloadit.com/uppy/v4.5.0/uppy.min.js\"></script>");
jsIncludeFile("hgMyData.js", NULL);

// the skeleton HTML:
webIncludeFile("inc/hgMyData.html");
webIncludeResourceFile("hgMyData.css");

// get the current files and vars stored for this user
struct jsonWrite *jw = jsonWriteNew();
outUiDataForUser(jw);
jsInlineF("\nvar uiData = {%s}\n", jw->dy->string);
jsonWriteFree(&jw);
jsInlineF("\nvar cartDb=\"%s %s\";\n", trackHubSkipHubName(hGenome(database)), database);
jsInlineF("\nvar tusdEndpoint=\"%s\";\n", cfgOptionDefault("hubSpaceTusdEndpoint", NULL));
jsInline("$(document).ready(function() {\nhubCreate.init();\n})");
puts("</div>");
}

void revokeApiKey(struct cartJson *cj, struct hash *paramHash)
/* Remove any api keys for the user */
{
char *userName = getUserName();
struct sqlConnection *conn = hConnectCentral();
struct dyString *query = sqlDyStringCreate("delete from %s where userName='%s'", HUBSPACE_AUTH_TABLE, userName);
sqlUpdate(conn, dyStringCannibalize(&query));
hDisconnectCentral(&conn);
jsonWriteString(cj->jw, "revoke", "true");
}

void generateApiKey(struct cartJson *cj, struct hash *paramHash)
/* Make a random (but not crypto-secure api key for use of hubtools to upload to hubspace */
{
char *userName = getUserName();
if (!userName)
    {
    jsonWriteString(cj->jw, "error", "generateApiKey: not logged in");
    return;
    }
char *apiKey = makeRandomKey(256); // just needs some arbitrary length
// save this key to the database for this user, the 'on duplicate' part automatically revokes old keys
struct sqlConnection *conn = hConnectCentral();
struct dyString *query = sqlDyStringCreate("insert into %s values ('%s', '%s') on duplicate key update apiKey='%s'", HUBSPACE_AUTH_TABLE, userName, apiKey, apiKey);
sqlUpdate(conn, dyStringCannibalize(&query));
jsonWriteString(cj->jw, "apiKey", apiKey);
hDisconnectCentral(&conn);
}
