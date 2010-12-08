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

static char *pageTitle = "UCSC Track Hub Connect";


void hgHubConnect()
/* Put up the list of hubs and other controls for the page. */
{
printf("<FORM ACTION=\"../cgi-bin/hgHubConnect\" METHOD=\"GET\" NAME=\"mainForm\">\n");
cartSaveSession(cart);
printf(
   "<P>Track hubs are collections of tracks from outside of UCSC that can be imported into the "
   "Genome Browser.  To import a hub check the box in the list below. "
   "After import the hub will show up as a group of tracks with it's own blue "
   "bar and label underneath the main browser graphic, and in the configure page. To arrange "
   "for your own track hub to appear in this list, please contact genome@soe.ucsc.edu.</P>\n"
   );
struct sqlConnection *conn = hConnectCentral();
char query[512];
safef(query, sizeof(query), "select id,shortLabel,longLabel,errorMessage,hubUrl from %s", 
	hubConnectTableName); 
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;

webPrintLinkTableStart();
boolean firstRow = TRUE;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstRow)
        firstRow = FALSE;
    else
        webPrintLinkTableNewRow();
    char *id = row[0], *shortLabel = row[1], *longLabel = row[2], *errorMessage = row[3],
    	 *url = row[4];
    if (errorMessage)
        webPrintLinkCell("error");
    else
	{
        webPrintLinkCellStart();
	char hubName[32];
	safef(hubName, sizeof(hubName), "hub_%u", id);
	cartMakeCheckBox(cart, hubName, FALSE);
	webPrintLinkCellEnd();
	}
    webPrintLinkCell(shortLabel);
    if (errorMessage)
	webPrintLinkCell(errorMessage);
    else
        webPrintLinkCell(longLabel);
    webPrintLinkCell(url);
    }
sqlFreeResult(&sr);
webPrintLinkTableEnd();
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

char *excludeVars[] = {"Submit", "submit", "hc_one_url", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(10);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}

