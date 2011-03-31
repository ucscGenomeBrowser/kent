/* hgHubConnect - the CGI web-based program to select track data hubs to connect with. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "errabort.h"
#include "errCatch.h"
#include "hCommon.h"
#include "dystring.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "dbDb.h"
#include "web.h"
#include "trackHub.h"
#include "hubConnect.h"

static char const rcsid[] = "$Id: hgPcr.c,v 1.29 2009/09/23 18:42:17 angie Exp $";

struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;

static char *destUrl = "../cgi-bin/hgTracks";
static char *pageTitle = "Import Tracks from External Data Hubs";
char *database = NULL;
char *organism = NULL;

boolean nameInCommaList(char *name, char *commaList)
/* Return TRUE if name is in comma separated list. */
{
if (commaList == NULL)
    return FALSE;
int nameLen = strlen(name);
for (;;)
    {
    char c = *commaList;
    if (c == 0)
        return FALSE;
    if (memcmp(name, commaList, nameLen) == 0)
        {
	c = commaList[nameLen];
	if (c == 0 || c == ',')
	    return TRUE;
	}
    commaList = strchr(commaList, ',');
    if (commaList == NULL)
        return FALSE;
    commaList += 1;
    }
}

void hgHubConnect()
/* Put up the list of hubs and other controls for the page. */
{
destUrl = cartUsualString(cart, hgHubConnectCgiDestUrl, destUrl);
printf("<FORM ACTION=\"%s\" METHOD=\"POST\" NAME=\"mainForm\">\n", destUrl);
cartSaveSession(cart);
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
printf(
   "<P>Track data hubs are collections of tracks from outside of UCSC that can be imported into "
   "the Genome Browser.  To import a hub check the box in the list below. "
   "After import the hub will show up as a group of tracks with its own blue "
   "bar and label underneath the main browser graphic, and in the configure page. To arrange "
   "for your own track data hub to appear in this list, please contact genome@soe.ucsc.edu.</P>\n"
   );
getDbAndGenome(cart, &database, &organism, oldVars);
printf("<B>genome:</B> %s &nbsp;&nbsp;&nbsp;<B>assembly:</B> %s &nbsp;&nbsp;&nbsp;[%s] ", 
	organism, hFreezeDate(database), database);
cgiMakeButton("submit", "submit");
printf("<BR>");
struct sqlConnection *conn = hConnectCentral();
char query[512];
safef(query, sizeof(query), "select id,shortLabel,longLabel,errorMessage,hubUrl,dbList from %s", 
	hubConnectTableName); 
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;

boolean gotAnyRows = FALSE;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *id = row[0], *shortLabel = row[1], *longLabel = row[2], *errorMessage = row[3],
    	 *url = row[4], *dbList = row[5];
    if (nameInCommaList(database, dbList))
	{
	if (gotAnyRows)
	    webPrintLinkTableNewRow();
	else
	    {
	    webPrintLinkTableStart();
	    gotAnyRows = TRUE;
	    }
	if (isEmpty(errorMessage))
	    {
	    webPrintLinkCellStart();
	    char hubName[32];
	    safef(hubName, sizeof(hubName), "%s%s", hgHubConnectHubVarPrefix, id);
	    cartMakeCheckBox(cart, hubName, FALSE);
	    webPrintLinkCellEnd();
	    }
	else
	    webPrintLinkCell("error");
	webPrintLinkCell(shortLabel);
	if (isEmpty(errorMessage))
	    webPrintLinkCell(longLabel);
	else
	    webPrintLinkCell(errorMessage);
	webPrintLinkCell(url);
	}
    }
sqlFreeResult(&sr);
if (gotAnyRows)
    webPrintLinkTableEnd();
else
    printf("No Track Hubs for this genome assembly");
printf("</FORM>\n");
hDisconnectCentral(&conn);
}

void doMiddle(struct cart *theCart)
/* Write header and body of html page. */
{
cart = theCart;
setUdcCacheDir();
cartWebStart(cart, NULL, pageTitle);
hgHubConnect();
cartWebEnd();
}

char *excludeVars[] = {"Submit", "submit", "hc_one_url", hgHubConnectCgiDestUrl, NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(10);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}

