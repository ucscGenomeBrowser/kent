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

void doRemoveFile(struct cartJson *cj, struct hash *paramHash)
/* Process the request to remove a file */
{
char *userName = getUserName();
if (userName)
    {
    // our array of objects, each object represents a track file
    struct jsonElement *deleteJson = hashFindVal(paramHash, "fileList");
    struct slRef *copy, *f, *fileList = deleteJson->val.jeList;
    struct slRef *dirList = NULL;
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
            slAddHead(&dirList, f);
            }
        else
            {
            removeOneFile(userName, fileName, fullPath, db, fileType);
            // write out the fullPath so the DataTable can remove the correct row:
            jsonWriteString(cj->jw, NULL, fullPath);
            }
        f = copy;
        }
    // now attempt to delete any requested directories, but don't die if they still have contents
    for (f = dirList; f != NULL; f = f->next)
        {
        struct jsonElement *fileObj = (struct jsonElement *)f->val;
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

static void outFilesForUser()
/* List out the currently stored files for the user and their sizes */
{
char *userName = getUserName();
struct jsonWrite *jw = jsonWriteNew(); // the JSON to return for the client javascript
jsonWriteObjectStart(jw, NULL);
if (userName)
    {
    // the url for this user:
    jsonWriteString(jw, "userUrl", webDataDir(userName));
    jsonWriteListStart(jw, "fileList");
    struct hubSpace *file, *fileList = listFilesForUser(userName);
    for (file = fileList; file != NULL; file = file->next)
        {
        jsonWriteObjectStart(jw, NULL);
        cgiDecodeFull(file->fileName, file->fileName, strlen(file->fileName));
        jsonWriteString(jw, "fileName", file->fileName);
        jsonWriteNumber(jw, "fileSize", file->fileSize);
        jsonWriteString(jw, "fileType", file->fileType);
        jsonWriteString(jw, "parentDir", file->parentDir);
        jsonWriteString(jw, "genome", file->db);
        jsonWriteString(jw, "lastModified", file->lastModified);
        jsonWriteString(jw, "uploadTime", file->creationTime);
        jsonWriteString(jw, "fullPath", stripDataDir(file->location, userName));
        jsonWriteString(jw, "md5sum", file->md5sum);
        jsonWriteObjectEnd(jw);
        }
    jsonWriteListEnd(jw);
    }
jsonWriteObjectEnd(jw);
jsInlineF("var isLoggedIn = %s\n", getUserName() ? "true" : "false");
jsInlineF("var userFiles = %s;\n", dyStringCannibalize(&jw->dy));
jsInlineF("var hubNameDefault = \"%s\";\n", defaultHubNameForUser(getUserName()));
// if the user is not logged, the 0 for the quota is ignored
jsInlineF("var userQuota = %llu\n", getUserName() ? checkUserQuota(getUserName()) : 0);
jsInlineF("var maxQuota = %llu\n", getUserName() ? getMaxUserQuota(getUserName()) : HUB_SPACE_DEFAULT_QUOTA);
jsonWriteFree(&jw);
}

void doTrackHubWizard(char *database)
/* Offer an upload form so users can upload all their hub files */
{
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("lodash.3.10.0.compat.min.js", NULL);
jsIncludeFile("cart.js", NULL);
puts("<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/font-awesome/4.5.0/css/font-awesome.min.css\">\n");
puts("<link rel=\"stylesheet\" type=\"text/css\" "
    "href=\"https://cdn.datatables.net/2.1.3/css/dataTables.dataTables.min.css\">\n");
puts("<script type=\"text/javascript\" "
    "src=\"https://cdn.datatables.net/2.1.3/js/dataTables.min.js\"></script>");
puts("<link rel=\"stylesheet\" type=\"text/css\" "
    "href=\"https://cdn.datatables.net/buttons/3.1.1/css/buttons.dataTables.min.css\">\n");
puts("<script type=\"text/javascript\" "
    "src=\"https://cdn.datatables.net/buttons/3.1.1/js/dataTables.buttons.min.js\"></script>");
puts("<link rel=\"stylesheet\" type=\"text/css\" "
    "href=\"https://cdn.datatables.net/select/2.1.0/css/select.dataTables.min.css\">\n");
puts("<script type=\"text/javascript\" "
    "src=\"https://cdn.datatables.net/select/2.1.0/js/dataTables.select.min.js\"></script>");
puts("<link href=\"https://releases.transloadit.com/uppy/v4.5.0/uppy.min.css\" rel=\"stylesheet\">");
puts("<script type=\"text/javascript\" src=\"https://releases.transloadit.com/uppy/v4.5.0/uppy.min.js\"></script>");
jsIncludeFile("hgMyData.js", NULL);

// the skeleton HTML:
webIncludeFile("inc/hgMyData.html");
webIncludeResourceFile("hgMyData.css");

// get the current files stored for this user
outFilesForUser();
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
