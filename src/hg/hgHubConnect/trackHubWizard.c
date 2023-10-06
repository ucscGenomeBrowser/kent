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

void doTrackHubWizard()
/* Offer an upload form so users can upload all their hub files */
{
char *tusdBasePath = cfgOption("tusdBasePath");
char *tusdPort = cfgOption("tusdPort");
/*
if (!(tusdPort && tusdBasePath))
    errAbort("tusd not installed or not running");
*/
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("lodash.3.10.0.compat.min.js", NULL);
jsIncludeFile("cart.js", NULL);
jsIncludeFile("tus.min.js", NULL);
jsIncludeFile("hgMyData.js", NULL);
webIncludeResourceFile("../style/bootstrap.min.css");
webIncludeResourceFile("../style/gb.css");
puts("<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/font-awesome/4.5.0/css/font-awesome.min.css\">\n");
puts("<div id='hubUpload' class='hubList'>\n");

puts("<div class='row'>\n");
puts("<div class='col-md-6'>\n");
puts("<div class='row'>\n");
puts("<div class='tabSection'>\n");
puts("<h4>Create your own hub</h4>\n");
puts("<p>After choosing files, click \"Create Hub\" to begin uploading the files to our server</p>\n");
puts("<div class='buttonDiv' id='chooseAndSendFilesRow'>\n");
puts("<label id='btnForInput' class='button' for=\"uploadedFiles\">Choose files</label>\n");
puts("</div>\n"); // .buttonDiv
puts("<div id='fileList'></div>\n");
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

puts("<div id='uploadedFilesSection' style=\"display: none\" class='col-md-6 tabSection'>");
puts("<h4>Your uploaded hubs</h4>");
webIncludeFile("inc/hgMyData.html");
//webIncludeResourceFile("../style/gbStatic.css");
puts("</div>\n");
puts("</div>\n"); // row
puts("</div>\n"); // row

jsInlineF("const tusdBasePath=\"%s\";\nconst tusdPort=\"%s\";\n", tusdBasePath, tusdPort);
jsInline("$(document).ready(function() {\nhubCreate.init();\n})");
puts("</div>");
}

char *prepBigData(struct cart *cart, char *fileName, char *binVar, char *fileVar)
{
fprintf(stderr, "prepping bigData for %s\n", fileName);
//return NULL;
//if (!customTrackIsBigData(fileName))
//    return NULL;
char buf[1024];
char *retFileName = NULL;
char *cFBin = cartOptionalString(cart, binVar);
char *cF = cartOptionalString(cart, fileVar);
char *offset = NULL;
unsigned long size = 0;
if (cFBin)
    {
    // cFBin already contains memory offset and size (search for __binary in cheapcgi.c)
    safef(buf,sizeof(buf),"memory://%s %s", fileName, cFBin);
    char *split[3];
    int splitCount = chopByWhite(cloneString(cFBin), split, sizeof(split));
    if (splitCount > 2) {errAbort("hgCustom: extra garbage in %s", binVar);}
    offset = (char *)sqlUnsignedLong(split[0]);
    size = sqlUnsignedLong(split[1]);
    }
else
    {
    safef(buf, sizeof(buf),"memory://%s %lu %lu",
	  fileName, (unsigned long) cF, (unsigned long) strlen(cF));
    offset = (char *)sqlUnsignedLong(cF);
    size = (unsigned long)strlen(cF);
    }
if (cfgOptionBooleanDefault("storeUserFiles", FALSE))
    {
    //dumpStack("prepBigData: fileName = '%s'\n", fileName);
    //errAbort("prepBigData: fileName = '%s'\n", fileName);
    // figure out if user is logged in:
    //   1. if so, save 'fileName', which is really a pointer to memory, to username encoded directory
    //   2. Turn buf into a web accessible url to this data so it will be parsed
    //   as a track correctly
    char *userName = (loginSystemEnabled() || wikiLinkEnabled()) ? wikiLinkUserName() : NULL;
    if (userName)
        {
        // storeUserFiles returns a URL to a track
        // after sucessfully saving the data into /userdata
        retFileName = storeUserFile(userName, fileName, offset, size);
        }
    }
return retFileName;
}

#define FILEVAR "userFile__filename"
#define FILEVARBIN "userFile__binary"
void doCreateHub(struct cart *cart)
/* Called asynchronously when the user has submitted some files, return a json response of where 
 * the files live and the new quota */
{
// cheapcgi.c renames form variables as var__filename or var__binary for binary data, why?
char *fileName = cartOptionalString(cart, FILEVAR);
/*
struct hashEl *hel, *helList = hashElListHash(cart->hash);
for (hel = helList; hel != NULL; hel = hel->next)
{
fprintf(stderr, "hashEl name: '%s', value: '%s'\n", (char *)hel->name, (char *)hel->val);
}
*/
fprintf(stderr, "fileName is: %s\n", fileName);
fflush(stderr);
char *pathToFile = prepBigData(cart, fileName, FILEVARBIN, FILEVAR);
puts("Content-Type:text/javascript\n");
printf("{\"status\": \"%s is uploaded to %s\"}\n", cgiOptionalString("userFile__filename"), pathToFile);
fflush(stdout);
}
