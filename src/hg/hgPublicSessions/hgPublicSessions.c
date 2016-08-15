/* hgPublicSessions - A gallery for hgTracks sessions. */

/* Copyright (C) 2016 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "ra.h"
#include "dystring.h"
#include "hPrint.h"
#include "hgConfig.h"
#include "sessionThumbnail.h"
#include "jsHelper.h"
#include "verbose.h"

struct galleryEntry
/* Holds data for a single session in the gallery*/
    {
    struct galleryEntry *next;
    char *userName;
    char *realName;
    char *sessionName;
    char *settings;
    char *db;
    char *firstUse;
    char *imgPath;
    char *imgUri;
    struct dyString *sessionUrl;
    unsigned long useCount;
    };

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;


struct galleryEntry *galLoad(char **row)
/* Load a session entry from a row.  A row consists of:
 * 0. gbMembers.realName
 * 1. namedSessionDb.userName
 * 2. gbMembers.idx
 * 3. namedSessionDb.sessionName
 * 4. namedSessionDb.useCount
 * 5. namedSessionDb.settings
 * 6. namedSessionDb.contents
 * 7. namedSessionDb.firstUse */
{
char *dbIdx, *dbEnd;
struct galleryEntry *ret;
AllocVar(ret);
ret->realName = cloneString(row[0]);
ret->userName = cloneString(row[1]);
cgiDecodeFull(ret->userName, ret->userName, strlen(ret->userName));
ret->sessionName = cloneString(row[3]);
cgiDecodeFull(ret->sessionName, ret->sessionName, strlen(ret->sessionName));
ret->sessionUrl = dyStringCreate("hgS_doOtherUser=submit&hgS_otherUserName=%s&hgS_otherUserSessionName=%s", row[1], row[3]);

ret->imgPath = sessionThumbnailFilePath(row[2], row[3], row[7]);
if (fileExists(ret->imgPath))
    ret->imgUri = sessionThumbnailFileUri(row[2], row[3], row[7]);
else
    ret->imgUri = NULL;
ret->useCount = sqlUnsignedLong(row[4]);
ret->settings = cloneString(row[5]);
if (startsWith("db=", row[6]))
    dbIdx = row[6] + 3;
else
    dbIdx = strstr(row[6], "&db=") + 4;
if (dbIdx != NULL)
    {
    dbEnd = strchr(dbIdx, '&');
    if (dbEnd != NULL)
        ret->db = cloneStringZ(dbIdx, dbEnd-dbIdx);
    else
        ret->db = cloneString(dbIdx);
    }
else
    ret->db = cloneString("n/a");
ret->firstUse = cloneString(row[7]);
char *spacePt = strchr(ret->firstUse, ' ');
if (spacePt != NULL) *spacePt = '\0';
return ret;
}

void deleteGallery (struct galleryEntry **pGal)
/* Free all memory associated with a gallery entry */
{
struct galleryEntry *gal;
if ((gal = *pGal) != NULL)
    {
    freeMem(gal->realName);
    freeMem(gal->userName);
    freeMem(gal->sessionName);
    freeMem(gal->settings);
    freeMem(gal->db);
    freeMem(gal->firstUse);
    freeMem(gal->imgPath);
    freeMem(gal->imgUri);
    dyStringFree(&(gal->sessionUrl));
    freez(pGal);
    }
}

struct galleryEntry *galleryFetch()
/* Return an slList of gallery entries fetched from hgcentral */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
struct galleryEntry *gal, *galList = NULL;
char otherConstraints[80] = "", query[2048], **row;

sqlSafef (query, sizeof(query),
    "select m.realName, s.userName, m.idx, s.sessionName, s.useCount, s.settings, s.contents, s.firstUse from "
    "%s s left join gbMembers m on m.userName = s.userName where shared = 2%s"
    , namedSessionTable, otherConstraints);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    gal = galLoad(row);
    slAddHead (&galList, gal);
    }

sqlFreeResult(&sr);
hDisconnectCentral(&conn);
return galList;
}

void galleryDisplay(struct galleryEntry *galList)
/* Print a table containing the gallery data from galList */
{
struct galleryEntry *thisSession = galList;

/* Hide the orderable columns and disable ordering on the visible columns
 * https://datatables.net/reference/option/columnDefs for more info.
 * Then set up the ordering drop-down menu */
printf ("<script type=\"text/javascript\">");
printf("$(document).ready(function () {\n"
    "    $('#sessionTable').DataTable({\"columnDefs\": [{\"visible\":false, \"targets\":[2,3]},\n"
    "                                                   {\"orderable\":false, \"targets\":[0,1]}\n"
    "                                                  ],\n"
    "                                       \"dom\":\"lftip\",\n"
    "                                       \"stateSave\":true,\n"
    "                                       \"stateSaveCallback\": %s,\n"
    "                                       \"stateLoadCallback\": %s,\n"
    "                                });\n"
    /* Recover previous sorting choice from the cart settings, if available */
    "    var startOrder = $('#sessionTable').DataTable().order();\n"
    "    if (startOrder[0][0] == 3) {\n"
    "        if (startOrder[0][1] == \"asc\") {\n"
    "            $('#sortMethod').val(\"useAsc\");\n"
    "        } else {\n"
    "            $('#sortMethod').val(\"useDesc\");\n"
    "        }\n"
    "    } else {\n"
    "        if (startOrder[0][0] == 2) {\n"
    "            if (startOrder[0][1] == \"asc\") {\n"
    "                $('#sortMethod').val(\"dateAsc\");\n"
    "            } else {\n"
    "                $('#sortMethod').val(\"dateDesc\");\n"
    "            }\n"
    "        } else {\n"
    "            $('#sessionTable').DataTable().order([3,'desc']).draw();\n"
    "            $('#sortMethod').val(\"useDesc\");\n"
    "        }\n"
    "    }\n"
    "});\n",
    jsDataTableStateSave(hgPublicSessionsPrefix), jsDataTableStateLoad(hgPublicSessionsPrefix, cart));
printf ("function changeSort() {\n"
    "    var newSort = document.getElementById('sortMethod').value;\n"
    "    var theTable = $('#sessionTable').DataTable();\n"
    "    if (newSort == \"useDesc\") {theTable.order([3,'desc']).draw(); }\n"
    "    if (newSort == \"useAsc\") {theTable.order([3,'asc']).draw(); }\n"
    "    if (newSort == \"dateDesc\") {theTable.order([2,'desc']).draw(); }\n"
    "    if (newSort == \"dateAsc\") {theTable.order([2,'asc']).draw(); }\n"
    "}\n");
printf("</script>\n");

printf ("<p>\n");
printf ("<b>Sort by:</b> <select id=\"sortMethod\" onchange=\"changeSort()\">\n");
printf ("\t\t<option value=\"useDesc\">Popularity (descending)</option>\n");
printf ("\t\t<option value=\"useAsc\">Popularity (ascending)</option>\n");
printf ("\t\t<option value=\"dateDesc\">Creation (newest first)</option>\n");
printf ("\t\t<option value=\"dateAsc\">Creation (oldest first)</option>\n");
printf ("</select></p>\n");
printf ("<table id=\"sessionTable\" class=\"sessionTable stripe hover row-border compact\" width=\"100%%\">\n"
    "    <thead>"
    "        <tr>"
    "            <th>Screenshot</th>\n"
    "            <th>Session Properties</th>\n"
    "            <th>Creation Date</th>\n"
    "            <th>Use Count</th>\n"
    "        </tr>\n"
    "    </thead>\n");

printf ("<tbody>\n");

while (thisSession != NULL)
    {
    char *settingString = NULL;
    printf ("\t<tr>\n");
    if (isNotEmpty(thisSession->imgUri))
        {
        printf ("\t\t<td><a href=\"../cgi-bin/hgTracks?%s\">",
            dyStringContents(thisSession->sessionUrl));
        printf ("<img src=\"%s\" class=\"sessionThumbnail\"></a></td>\n", thisSession->imgUri);
        }
    else
        {
        printf ("\t\t<td><center><nobr>Screenshot not available</nobr><br>\n");
        printf ("\t\t<a href=\"../cgi-bin/hgTracks?%s\">Click Here</a> to view</center></td>\n",
            dyStringContents(thisSession->sessionUrl));
        }

    struct hash *settingsHash = raFromString(thisSession->settings);
    settingString = (char*) hashFindVal(settingsHash, "description");
    if (settingString == NULL)
        settingString = "";
    else
        {
        settingString = replaceChars(settingString, "\\\\", "\\__ESC__");
        settingString = replaceChars(settingString, "\\r", "\r");
        settingString = replaceChars(settingString, "\\n", "\n");
        settingString = replaceChars(settingString, "\\__ESC__", "\\");
        }
    printf ("\t\t<td><b>Description:</b> %s<br>\n", settingString);
    printf ("\t\t<b>Author:</b> %s<br>\n", thisSession->userName);
    printf ("\t\t<b>Session Name:</b> %s<br>\n", thisSession->sessionName);
    printf ("\t\t<b>Genome Assembly:</b> %s<br>\n", thisSession->db);
    printf ("\t\t<b>Creation Date:</b> %s<br>\n", thisSession->firstUse);
    printf ("\t\t<b>Views:</b> %ld\n", thisSession->useCount);
    printf ("\t\t</td>\n");
    struct tm creationDate;
    strptime(thisSession->firstUse, "%Y-%m-%d", &creationDate);
    /* Hidden columns */
    printf ("\t\t<td>%ld</td>\n", mktime(&creationDate));
    printf ("\t\t<td>%ld</td>\n", thisSession->useCount);
    printf ("\t</tr>\n");
    thisSession = thisSession->next;
    }

printf ("</tbody>\n");
printf ("</table>\n");
}

void showGalleryTab ()
/* Rather boring now, but a placeholder against the day that there's also a "favorites" tab */
{
struct galleryEntry *galList = galleryFetch();
galleryDisplay(galList);
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;
char *db = cartUsualString(cart, "db", hDefaultDb());
cartWebStart(cart, db, "Public Sessions");

/* Not in a form; can't use cartSaveSession() to set up an hgsid input */
printf ("<script>var common = {hgsid:\"%s\"};</script>\n", cartSessionId(cart));

jsIncludeDataTablesLibs();

printf("<p>Sessions allow users to save snapshots of the Genome Browser "
"and its current configuration, including displayed tracks, position, "
"and custom track data. The Public Sessions tool allows users to easily "
"share those sessions that they deem interesting with the rest of the "
"world's researchers. You can add your own sessions to this list by "
"checking the appropriate box on the "
"<a href=\"../cgi-bin/hgSession?%s\">Session Management</a> page.</p>\n"
"<p>See the "
"<a href=\"../goldenPath/help/hgSessionHelp.html\">Sessions User's Guide</a> "
"for more information.\n</p>", cartSidUrlString(cart));

showGalleryTab();

cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
setUdcCacheDir();
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
