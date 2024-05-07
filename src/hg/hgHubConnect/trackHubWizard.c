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

void removeOneFile(char *userName, char *cgiFileName)
/* Remove one single file for userName */
{
char *fileName = prefixUserFile(userName, cgiFileName);
if (fileExists(fileName))
    {
    fprintf(stderr, "deleting file: '%s'\n", fileName);
    removeFileForUser(fileName, userName);
    fflush(stderr);
    }
}

void doRemoveFile(struct cartJson *cj, struct hash *paramHash)
/* Process the request to remove a file */
{
char *userName = getUserName();
if (userName)
    {
    struct jsonWrite *errors = jsonWriteNew();
    // verify the argument is present:
    (void)cartJsonRequiredParam(paramHash, "deleteFile", errors, "doRemoveFile");
    struct jsonElement *deleteJson = hashFindVal(paramHash, "deleteFile");
    struct slRef *ele, *deleteList = deleteJson->val.jeList;
    for (ele = deleteList; ele != NULL; ele = ele->next)
        {
        struct jsonElement *jsonVal = ele->val;
        removeOneFile(userName, jsonVal->val.jeString);
        }
    fprintf(stdout, "Status: 204 No Content\n\n");
    fflush(stdout);
    exit(0);
    }
fprintf(stdout, "Status: 404 Not Found\n\n");
fflush(stdout);
exit(0);
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
    // params is an object with everything necessary to create a hub: name and assembly
    struct hash *params = jsonObjectVal(hashFindVal(paramHash, "createHub"), "createHub");
    char *db = hashFindVal(params, "db");
    char *name = hashFindVal(params, "name");
    fprintf(stderr, "creating hub '%s' for db '%s'\n", name, db);
    fflush(stderr);
    }
fprintf(stderr, "Status: 204 No Content\n\n");
fflush(stdout);
exit(0);
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
    struct userHubs *hub, *hubList = listHubsForUser(userName);
    // unpack hub directories into a flat list of files
    jsonWriteListStart(jw, "fileList");
    for (hub = hubList; hub != NULL; hub = hub->next)
        {
        struct fileInfo *file;
        struct userFiles *uf = listFilesForUserHub(userName, hub->hubName);
        for (file = uf->fileList; file != NULL; file = file->next)
            {
            jsonWriteObjectStart(jw, NULL);
            jsonWriteString(jw, "name", file->name);
            jsonWriteNumber(jw, "size", file->size);
            jsonWriteString(jw, "hub", hub->hubName);
            jsonWriteString(jw, "genome", hub->genome);
            jsonWriteDateFromUnix(jw, "createTime", file->creationTime);
            jsonWriteObjectEnd(jw);
            }
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
webIncludeResourceFile("../style/bootstrap.min.css");
webIncludeResourceFile("../style/gb.css");
puts("<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/font-awesome/4.5.0/css/font-awesome.min.css\">\n");
puts("<link rel=\"stylesheet\" type=\"text/css\" "
    "href=\"https://cdn.datatables.net/1.10.12/css/jquery.dataTables.min.css\">\n");
puts("<script type=\"text/javascript\" "
    "src=\"https://cdn.datatables.net/1.10.12/js/jquery.dataTables.min.js\"></script>");
puts("<div id='hubUpload' class='hubList'>\n");

puts("<div class='row'>\n");
puts("<div class='col-md-6'>\n");
puts("<div class='row'>\n");
puts("<div class='tabSection'>\n");
puts("<h4>Create your own hub</h4>\n");
puts("<p>After choosing files, click \"Create Hub\" to begin uploading the files to our server</p>\n");
puts("<div class='buttonDiv' id='chooseAndSendFilesRow'>\n");
puts("<button  id='btnForInput' class='button' for=\"chosenFiles\">Choose files</button>\n");
puts("</div>\n"); // .buttonDiv
puts("<div id='fileList' style=\"clear: right\"></div>\n");
puts("</div>"); // .tabSection
puts("</div>\n"); // row

puts("<div class='row'>\n");
puts("<div class='tabSection'>");
puts("For information on making track hubs, see the following pages: \n "
    "<ul>\n"
    "<li><a href='../goldenPath/help/hubQuickStart.html' style='color:#121E9A' target=_blank>Quick Start Guide</a></li>\n"
    "<li><a href=\"../goldenPath/help/hgTrackHubHelp.html\" style='color:#121E9A' TARGET=_blank>Track Hub User's Guide</a></li>\n"
    "<li><a href=\"../goldenPath/help/hgTrackHubHelp#Hosting\" style='color:#121E9A' target=_blank>Where to Host Your Track Hub</a></li>\n"
    "<li><a href=\"../goldenPath/help/trackDb/trackDbHub.html\" style='color:#121E9A' target=_blank>Track Hub Settings Reference</a></li>\n"
    "<li><a href=\"../goldenPath/help/publicHubGuidelines.html\" style='color:#121E9A' target=_blank>Guidelines for Submitting a Public Hub</a></li>\n"
    "</ul>\n"
    "<BR>You may also <a href='../contacts.html' style='color:#121E9A'>contact us</a> if you have any "
    "issues or questions on hub development.");
puts("</div>"); // .tabSection
puts("</div>\n"); // col-md-6
puts("</div>\n"); // row

puts("<div id='chosenFilesSection' style=\"display: none\" class='col-md-6 tabSection'>");
puts("<h4>Your uploaded hubs</h4>");
webIncludeFile("inc/hgMyData.html");
puts("</div>\n");
puts("</div>\n"); // row
puts("</div>\n"); // row

// get the current files stored for this user
outFilesForUser();
jsInline("$(document).ready(function() {\nhubCreate.init();\n})");
puts("</div>");
}
