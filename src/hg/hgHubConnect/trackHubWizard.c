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
#include "hubConnect.h"
#include "trackHub.h"

void removeOneFile(char *userName, char *cgiFileName, char *fullPath, char *db, char *fileType)
/* Remove one single file for userName */
{
char *fileName = prefixUserFile(userName, cgiEncodeFull(fullPath), NULL);
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

void removeHubDir(char *userName, char *cgiFileName)
/* Remove one single hub for userName */
{
char *hubDir = prefixUserFile(userName, cgiEncodeFull(cgiFileName), NULL);
if (isDirectory(hubDir))
    {
    fprintf(stderr, "deleting directory: '%s'\n", hubDir);
    removeHubForUser(hubDir, userName);
    fflush(stderr);
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
    struct slRef *f, *fileList = deleteJson->val.jeList;
    jsonWriteListStart(cj->jw, "deletedList");
    for (f = fileList; f != NULL; f = f->next)
        {
        struct jsonElement *fileObj = (struct jsonElement *)f->val;
        char *fileName = jsonStringField(fileObj, "fileName");
        char *fileType = jsonStringField(fileObj, "fileType");
        char *db = jsonStringField(fileObj, "genome");
        //char *parentDir = jsonStringField(fileObj, "parentDir");
        char *fullPath = jsonStringField(fileObj, "fullPath");
        removeOneFile(userName, fileName, fullPath, db, fileType);
        jsonWriteString(cj->jw, NULL, fileName);
        }
    jsonWriteListEnd(cj->jw);
    /*
    //for now don't worry about this:
    //if (isHub)
    //    removeHubDir(userName, fname);
    */
    }
}

void doMoveFile(struct cartJson *cj, struct hash *paramHash)
/* Move a file to a new hub */
{
}

void doCreateHub(struct cartJson *cj, struct hash *paramHash)
/* Make a new hub.txt with the parameters from the JSON request */
{
char *userName = getUserName();
if (userName)
    {
    struct jsonWrite *errors = jsonWriteNew();
    // verify the arguments:
    (void)cartJsonRequiredParam(paramHash, "createHub", errors, "doCreateHub");
    // paramHash is an object with everything necessary to create a hub: name and assembly
    char *db = jsonStringVal(hashFindVal(paramHash, "db"), "db");
    char *name = jsonStringVal(hashFindVal(paramHash, "name"), "name");
    char *encodedName = cgiEncodeFull(name);
    fprintf(stderr, "creating hub '%s' for db '%s'\n", encodedName, db);
    fflush(stderr);
    // check if this hub already exists, must have a directory and hub.txt already:
    char *path = prefixUserFile(userName, encodedName, NULL);
    if (isDirectory(path))
        {
        // can't make a hub that already exists!
        fprintf(stdout, "Status: 400 Bad Request\n\n");
        fprintf(stdout, "Hub already exists, select hub from dropdown or try a different name");
        fflush(stdout);
        exit(1);
        }
    else
        {
        // good we can make a new directory and stuff a hub.txt in it
        // the directory needs to be 777, so ignore umask for now
        writeHubText(path, userName, encodedName, db);
        // TODO: add a row to the hubspace table for the hub.txt
        //addHubTxtToTable(userName, path, name, db);
        // return json to fill out the table
        jsonWriteString(cj->jw, "hubName", name);
        jsonWriteString(cj->jw, "db", db);
        time_t now = time(NULL);
        jsonWriteString(cj->jw, "creationTime", sqlUnixTimeToDate(&now, FALSE));
        }
    }
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
jsIncludeFile("tus.js", NULL);
// this file must be included as a module for now as it needs to import:
//puts("<script src=\"../js/hgMyData.js\" type=\"module\"></script>");
puts("<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/font-awesome/4.5.0/css/font-awesome.min.css\">\n");
puts("<link rel=\"stylesheet\" type=\"text/css\" "
    "href=\"https://cdn.datatables.net/2.1.3/css/dataTables.dataTables.min.css\">\n");
puts("<script type=\"text/javascript\" "
    "src=\"https://cdn.datatables.net/2.1.3/js/dataTables.min.js\"></script>");
puts("<link rel=\"stylesheet\" type=\"text/css\" "
    "href=\"https://cdn.datatables.net/buttons/3.1.1/css/buttons.dataTables.min.css\">\n");
puts("<script type=\"text/javascript\" "
    "src=\"https://cdn.datatables.net/buttons/3.1.1/js/dataTables.buttons.min.js\"></script>");
puts("<link href=\"https://releases.transloadit.com/uppy/v4.5.0/uppy.min.css\" rel=\"stylesheet\">");
puts("<script type=\"text/javascript\" src=\"https://releases.transloadit.com/uppy/v4.5.0/uppy.min.js\"></script>");
jsIncludeFile("hgMyData.js", NULL);

// the skeleton HTML:
webIncludeFile("inc/hgMyData.html");
webIncludeResourceFile("hgMyData.css");

// get the current files stored for this user
outFilesForUser();
jsInlineF("\nvar cartDb=\"%s %s\";\n", trackHubSkipHubName(hGenome(database)), database);
jsInline("$(document).ready(function() {\nhubCreate.init();\n})");
puts("</div>");
}
