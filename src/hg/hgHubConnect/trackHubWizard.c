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

void removeOneFile(char *userName, char *cgiFileName)
/* Remove one single file for userName */
{
char *fileName = prefixUserFile(userName, cgiEncodeFull(cgiFileName));
if (fileExists(fileName))
    {
    fprintf(stderr, "deleting file: '%s'\n", fileName);
    removeFileForUser(fileName, userName);
    fflush(stderr);
    }
}

void removeHubDir(char *userName, char *cgiFileName)
/* Remove one single file for userName */
{
char *fileName = prefixUserFile(userName, cgiEncodeFull(cgiFileName));
if (fileExists(fileName))
    {
    fprintf(stderr, "deleting file: '%s'\n", fileName);
    removeHubForUser(fileName, userName);
    fflush(stderr);
    }
}

void doRemoveFile(struct cartJson *cj, struct hash *paramHash)
/* Process the request to remove a file */
{
char *userName = getUserName();
if (userName)
    {
    struct jsonElement *deleteJson = hashFindVal(paramHash, "fileNameList");
    //struct jsonWrite *errors = jsonWriteNew();
    // TODO: Check request is well-formed
    char *fname = ((struct jsonElement *)(deleteJson->val.jeList->val))->val.jeString;
    boolean isHub = sameString("hub", ((struct jsonElement *)(deleteJson->val.jeList->next->val))->val.jeString);
    jsonWriteListStart(cj->jw, "deletedList");
    if (isHub)
        removeHubDir(userName, fname);
    else
        removeOneFile(userName, fname);
    jsonWriteString(cj->jw, NULL, fname);
    jsonWriteListEnd(cj->jw);
    }
}

void doMoveFile(struct cartJson *cj, struct hash *paramHash)
/* Move a file to a new hub */
{
}

static void writeHubText(char *path, char *userName, char *hubName, char *db)
/* Create a hub.txt file, optionally creating the directory holding it */
{
int oldUmask = 00;
oldUmask = umask(0);
makeDirsOnPath(path);
// restore umask
umask(oldUmask);
// now make the hub.txt with some basic information
char *hubFile = catTwoStrings(path, "/hub.txt");
FILE *f = mustOpen(hubFile, "w");
//fprintf(stderr, "would write \"hub %s\nemail %s\nshortLabel %s\nlongLabel %s\nuseOneFile on\n\ngenome %s\n\n\" to %s", hubName, emailForUserName(userName), hubName, hubName, db, hubFile);
fprintf(f, "hub %s\n"
    "email %s\n"
    "shortLabel %s\n"
    "longLabel %s\n"
    "useOneFile on\n\n"
    "genome %s\n\n",
    hubName, emailForUserName(userName), hubName, hubName, db);
carefulClose(&f);
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
    char *path = prefixUserFile(userName, encodedName);
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
        jsonWriteString(jw, "fileName", file->fileName);
        jsonWriteNumber(jw, "fileSize", file->fileSize);
        jsonWriteString(jw, "fileType", file->fileType);
        jsonWriteString(jw, "hub", "");
        jsonWriteString(jw, "genome", file->db);
        jsonWriteString(jw, "createTime", file->creationTime);
        jsonWriteObjectEnd(jw);
        }
    jsonWriteListEnd(jw);
    jsonWriteListStart(jw, "hubList");
    struct userHubs *hub, *hubList = listHubsForUser(userName);
    for (hub = hubList; hub != NULL; hub = hub->next)
        {
        jsonWriteObjectStart(jw, NULL);
        jsonWriteString(jw, "hubName", hub->hubName);
        jsonWriteString(jw, "genome", hub->genome);
        jsonWriteObjectEnd(jw);
        }
    jsonWriteListEnd(jw);
    }
jsonWriteObjectEnd(jw);
jsInlineF("var userFiles = %s;\n", dyStringCannibalize(&jw->dy));
jsonWriteFree(&jw);
}

void doTrackHubWizard()
/* Offer an upload form so users can upload all their hub files */
{
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("lodash.3.10.0.compat.min.js", NULL);
jsIncludeFile("cart.js", NULL);
jsIncludeFile("tus.js", NULL);
jsIncludeFile("hgMyData.js", NULL);
puts("<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/font-awesome/4.5.0/css/font-awesome.min.css\">\n");
puts("<link rel=\"stylesheet\" type=\"text/css\" "
    "href=\"https://cdn.datatables.net/2.1.3/css/dataTables.dataTables.min.css\">\n");
puts("<script type=\"text/javascript\" "
    "src=\"https://cdn.datatables.net/2.1.3/js/dataTables.min.js\"></script>");
puts("<link rel=\"stylesheet\" type=\"text/css\" "
    "href=\"https://cdn.datatables.net/buttons/3.1.1/css/buttons.dataTables.min.css\">\n");
puts("<script type=\"text/javascript\" "
    "src=\"https://cdn.datatables.net/buttons/3.1.1/js/dataTables.buttons.min.js\"></script>");

// the skeleton HTML:
webIncludeFile("inc/hgMyData.html");

// get the current files stored for this user
outFilesForUser();
jsInline("$(document).ready(function() {\nhubCreate.init();\n})");
puts("</div>");
}
