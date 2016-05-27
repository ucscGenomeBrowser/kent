/* hgPublicSessions - A gallery for hgTracks sessions. */
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

struct galleryEntry
/* Holds data for a single session in the gallery*/
    {
    struct galleryEntry *next;
    char *userName, *realName, *sessionName, *settings, *db, *firstUse;
    char *imgPath, *imgUri;
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
dbIdx = strstr(row[6], "db=") + 3;
dbEnd = strchr(dbIdx, '&');
if (dbEnd != NULL)
    ret->db = cloneStringZ(dbIdx, dbEnd-dbIdx);
else
    ret->db = cloneString(dbIdx);
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
    "%s s left join gbMembers m on m.userName = s.userName where shared = 2%s limit 30"
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

void doGalleryIncludes()
/* Print external links to the jquery js and css files used for this CGI.  Unfortunately
 * this conflicts a bit with the jquery we inherit from cartWebStart().  Hope
 * to resolve that some day by figuring out what's preventing us from updating
 * our jquery version elsewhere */
{
printf ("<link rel=\"stylesheet\" type=\"text/css\" "
        "href=\"https://cdn.datatables.net/1.10.12/css/jquery.dataTables.min.css\">\n");
printf ("<script type=\"text/javascript\" "
        "src=\"https://code.jquery.com/jquery-1.12.3.min.js\"></script>\n");
printf ("<script type=\"text/javascript\" charset=\"utf8\" "
        "src=\"https://cdn.datatables.net/1.10.12/js/jquery.dataTables.min.js\"></script>\n");
}


void galleryDisplay(struct galleryEntry *galList)
/* Print a table containing the gallery data from galList */
{
struct galleryEntry *thisSession = galList;

printf ("<script type=\"text/javascript\">"
"$(document).ready(function () {\n"
"    $('#sessionTable').DataTable({\"columnDefs\": [{\"visible\":false, \"targets\":[2,3]},\n"
"                                                   {\"orderable\":false, \"targets\":[0,1]}\n"
"                                                  ],\n"
"                                       \"order\":[3,'desc']});\n"
"} );\n"
"function changeSort() {\n"
"   var newSort = document.getElementById('sortMethod').value;\n"
"   var theTable = $('#sessionTable').DataTable();\n"
"   if (newSort == \"useDesc\") {theTable.order([3,'desc']).draw(); }\n"
"   if (newSort == \"useAsc\") {theTable.order([3,'asc']).draw(); }\n"
"   if (newSort == \"dateDesc\") {theTable.order([2,'desc']).draw(); }\n"
"   if (newSort == \"dateAsc\") {theTable.order([2,'asc']).draw(); }\n"
"}\n"
"</script>\n");

printf ("<p>\n");
printf ("<b>Sort by:</b> <select id=\"sortMethod\" onchange=\"changeSort()\">\n");
printf ("\t\t<option value=\"useDesc\">Popularity (descending)</option>\n");
printf ("\t\t<option value=\"useAsc\">Popularity (ascending)</option>\n");
printf ("\t\t<option value=\"dateDesc\">Creation (newest first)</option>\n");
printf ("\t\t<option value=\"dateAsc\">Creation (oldest first)</option>\n");
printf ("</select></p>\n");
printf ("<table id=\"sessionTable\" class=\"display compact\" width=\"100%%\">\n"
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

doGalleryIncludes();

printf("<p>Our users have marked the following sessions as being of "
    "interest to the community."
    "<br>See the <a href=\"../goldenPath/help/hgSessionHelp.html\" "
    "target=_blank>Sessions User's Guide</a> "
    "for more information on how to add your sessions to this page.<p/>\n");

showGalleryTab();

printf ("<p>You can adjust the settings for your own sessions on\n"
    "the <a href=\"hgSession\">Sessions</a> page.\n</p>");

cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
