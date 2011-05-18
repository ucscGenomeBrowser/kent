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
#include "dystring.h"
#include "hPrint.h"


#define hgHub             "hgHub_"  /* prefix for all control variables */
#define hgHubDo            hgHub   "do_"    /* prefix for all commands */
#define hgHubDoAdd         hgHubDo "add"
#define hgHubDoClear       hgHubDo "clear"

struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;

static char *destUrl = "../cgi-bin/hgTracks";
static char *pageTitle = "Import Tracks from Data Hubs";
char *database = NULL;
char *organism = NULL;

boolean nameInCommaList(char *name, char *commaList);

static void hgHubConnectUnlisted()
/* Put up the list of unlisted hubs and other controls for the page. */
{
printf("<B>Unlisted Hubs</B><BR>");
struct hubConnectStatus *hub, *hubList =  hubConnectStatusListFromCart(cart);
int count = 0;
for(hub = hubList; hub; hub = hub->next)
    {
    /* if the hub is public, then don't list it here */
    if (!isHubUnlisted(hub))
	continue;

    if (count)
	webPrintLinkTableNewRow();
    else
	webPrintLinkTableStart();
    count++;

    if (isEmpty(hub->errorMessage))
	{
	webPrintLinkCellStart();
	char hubName[32];
	safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, hub->id);
	cartMakeCheckBox(cart, hubName, FALSE);
	webPrintLinkCellEnd();
	}
    else
	webPrintLinkCell("error");
    webPrintLinkCell(hub->shortLabel);
    if (isEmpty(hub->errorMessage))
	webPrintLinkCell(hub->longLabel);
    else
	webPrintLinkCell(hub->errorMessage);
    webPrintLinkCell(hub->hubUrl);
    }
if (count)
    webPrintLinkTableEnd();
else
    printf("No Unlisted Track Hubs for this genome assembly<BR>");
}

static void makeNewHubButton()
{
printf("<FORM ACTION=\"%s\" METHOD=\"POST\" NAME=\"secondForm\">\n", "../cgi-bin/hgHubConnect");
cartSaveSession(cart);
cgiMakeHiddenVar(hgHubDoAdd, "on");
cgiMakeButton("add", "Add new unlisted hub");
printf("</FORM>\n");
}

static void makeGenomePrint()
{
getDbAndGenome(cart, &database, &organism, oldVars);
printf("<B>genome:</B> %s &nbsp;&nbsp;&nbsp;<B>assembly:</B> %s  ",
	organism, hFreezeDate(database));
}

static void addIntro()
{
printf("Enter URL to remote hub.<BR>\n");
}

void addUnlistedHubForm(struct hubConnectStatus *hub, char *err)
/* display UI for adding unlisted hubs by URL */
{
getDbAndGenome(cart, &database, &organism, oldVars);
boolean gotClade = FALSE;
boolean isUpdateForm = FALSE;
if (hub)
    {
    isUpdateForm = TRUE;
    }
else
    /* add form needs clade for assembly menu */
    gotClade = hGotClade();

/* main form */
printf("<FORM ACTION=\"%s\" METHOD=\"%s\" "
    " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n",
    "../cgi-bin/hgHubConnect", cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");

/* intro text */
puts("<P>");
puts("Add your own unlisted data hub for the browser.");
addIntro();
puts("<P>");

/* row for error message */
if (err)
    printf("<P><B>&nbsp;&nbsp;&nbsp;&nbsp;<span style='color:red; font-style:italic;'>Error</span>&nbsp;%s</B><P>", err);

printf("Enter URL:");

hTextVar(hgHubDataText, "", 60);
cgiMakeSubmitButton();

puts("</FORM>");
}

void helpUnlistedHub()
{
printf("Unlisted hubs are constructed the same way as public hubs, but they "
   "aren't listed in hgcentral<BR>\n");
}

void doAddUnlistedHub(struct cart *theCart, char *err)
/* Write header and body of html page. */
{
cartWebStart(cart, database, "Add Unlisted Hub");
addUnlistedHubForm(NULL, err);
helpUnlistedHub();
cartWebEnd(cart);
}


void hgHubConnectPublic()
/* Put up the list of external hubs and other controls for the page. */
{
printf("<B>Public Hubs</B><BR>");
struct sqlConnection *conn = hConnectCentral();
char query[512];
safef(query, sizeof(query), "select hubUrl, shortLabel,longLabel,dbList from %s", 
	hubPublicTableName); 
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;

boolean gotAnyRows = FALSE;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *url = row[0], *shortLabel = row[1], *longLabel = row[2], 
    	  *dbList = row[3];
    if (nameInCommaList(database, dbList))
	{
	if (gotAnyRows)
	    webPrintLinkTableNewRow();
	else
	    {
	    webPrintLinkTableStart();
	    gotAnyRows = TRUE;
	    }
	char *errorMessage = NULL;
	unsigned id = hubFindOrAddUrlInStatusTable(database, cart, 
	    url, &errorMessage);

	if ((id != 0) && isEmpty(errorMessage)) 
	    {
	    webPrintLinkCellStart();
	    char hubName[32];
	    safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, id);
	    cartMakeCheckBox(cart, hubName, FALSE);
	    webPrintLinkCellEnd();
	    }
	else if (!isEmpty(errorMessage))
	    webPrintLinkCell("error");
	else
	    errAbort("cannot get id for hub with url %s\n", url);

	webPrintLinkCell(shortLabel);
	if (isEmpty(errorMessage))
	    webPrintLinkCell(longLabel);
	else
	    {
	    char errorBuf[4*1024];
	    safef(errorBuf, sizeof errorBuf, "Error: %s", errorMessage);
	    webPrintLinkCell(errorBuf);
	    }
	webPrintLinkCell(url);
	}
    }
sqlFreeResult(&sr);

if (gotAnyRows)
    webPrintLinkTableEnd();
else
    printf("No Public Track Hubs for this genome assembly<BR>");
hDisconnectCentral(&conn);
}

static void doClearHub(struct cart *theCart)
{
char *url = cartOptionalString(cart, hgHubDataText);

if (url != NULL)
    hubClearStatus(url);
else
    errAbort("must specify url in %s\n", hgHubDataText);
}

void doMiddle(struct cart *theCart)
/* Write header and body of html page. */
{
cart = theCart;
setUdcCacheDir();
if (cartVarExists(cart, hgHubDoAdd))
    doAddUnlistedHub(cart, NULL);
else if (cartVarExists(cart, hgHubDoClear))
    doClearHub(cart);
else
    {
    cartWebStart(cart, NULL, pageTitle);

    printf(
       "<P>Track data hubs are collections of tracks from outside of UCSC that can be imported into "
       "the Genome Browser.  To import a public hub check the box in the list below. "
       "After import the hub will show up as a group of tracks with its own blue "
       "bar and label underneath the main browser graphic, and in the configure page. </P>\n"
       );
    makeGenomePrint();

    hubCheckForNew(database, cart);
    printf("<BR><P>");
    printf("<FORM ACTION=\"%s\" METHOD=\"POST\" NAME=\"mainForm\">\n", destUrl);
    cartSaveSession(cart);
    hgHubConnectPublic();
    printf("<BR>Contact <A HREF=\"mailto:genome@soe.ucsc.edu\"> genome@soe.ucsc.edu </A>to add a public hub.</P>\n");
    puts("<BR>");
    hgHubConnectUnlisted();
    puts("<BR>");

    cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
    cgiMakeButton("Submit", "Use Selected Hubs");
    puts("</FORM>");

    makeNewHubButton();
    }
cartWebEnd();
}

char *excludeVars[] = {"Submit", "submit", "hc_one_url", hgHubConnectCgiDestUrl, hgHubDoAdd, hgHubDoClear hgHubDataText, NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(10);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}

