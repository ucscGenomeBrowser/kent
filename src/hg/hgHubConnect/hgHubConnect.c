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
#include "jsHelper.h"
#include "obscure.h"
#include "hgConfig.h"

#define hgHub             "hgHub_"  /* prefix for all control variables */
#define hgHubDo            hgHub   "do_"    /* prefix for all commands */
#define hgHubDoClear       hgHubDo "clear"
#define hgHubDoDisconnect  hgHubDo "disconnect"
#define hgHubDoReset       hgHubDo "reset"

struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;

static char *destUrl = "../cgi-bin/hgTracks";
static char *pageTitle = "Track Data Hubs";
char *database = NULL;
char *organism = NULL;

static void ourCellStart()
{
puts("<TD>");
}

static void ourCellEnd()
{
puts("</TD>");
}

static void ourPrintCell(char *str)
{
ourCellStart();
puts(str);
ourCellEnd();
}

static void hgHubConnectUnlisted()
/* Put up the list of unlisted hubs and other controls for the page. */
{
// put out the top of our page
printf("<div id=\"unlistedHubs\" class=\"hubList\"> "
    "<table id=\"unlistedHubsTable\"> "
    "<thead><tr> "
	"<th colspan=\"5\" id=\"addHubBar\"><label>URL:</label> "
	"<input name=\"hubText\" id=\"hubUrl\" class=\"hubField\""
	    "type=\"text\" size=\"65\"> "
	"<input name=\"hubAddButton\""
	    "onClick=\"document.addHubForm.elements['hubUrl'].value=hubText.value;"
		"document.addHubForm.submit();return true;\" "
		"class=\"hubField\" type=\"button\" value=\"Add Hub\">"
	"</th> "
    "</tr> ");

// count up the number of unlisted hubs we currently have
int count = 0;
struct hubConnectStatus *hub, *hubList =  hubConnectStatusListFromCartAll(cart);
for(hub = hubList; hub; hub = hub->next)
    {
    if (isHubUnlisted(hub))
	count++;
    }

if (count == 0)
    {
    // nothing to see here
    printf(
	"<tr><td>No Track Hubs for this genome assembly</td></tr>"
	"</td></table>");
    printf("</thead></div>");
    return;
    }

// time to output the big table.  First the header
printf(
    "<tr> "
	"<th>Display</th> "
	"<th>Hub Name</th> "
	"<th>Description</th> "
	"<th>URL</th> "
	"<th>Disconnect</th> "
    "</tr></thead>\n");

// start first row
printf("<tbody><tr>");

count = 0;
for(hub = hubList; hub; hub = hub->next)
    {
    /* if the hub is public, then don't list it here */
    if (!isHubUnlisted(hub))
	continue;

    if (count)
	webPrintLinkTableNewRow();  // ends last row and starts a new one
    count++;

    // if there's an error message, we don't let people select it
    if (isEmpty(hub->errorMessage))
	{
	ourCellStart();
	char hubName[32];
	safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, hub->id);
	cartMakeCheckBox(cart, hubName, FALSE);
	ourCellEnd();
	}
    else
	{
	// give people a chance to clear the error 
	ourCellStart();
	printf(
	"<input name=\"hubClearButton\""
	    "onClick=\"document.resetHubForm.elements['hubUrl'].value='%s';"
		"document.resetHubForm.submit();return true;\" "
		"class=\"hubField\" type=\"button\" value=\"clear error\">"
		, hub->hubUrl);
	ourCellEnd();
	}
    ourPrintCell(hub->shortLabel);
    if (isEmpty(hub->errorMessage))
	ourPrintCell(hub->longLabel);
    else
	printf("<TD><span class=\"hubError\">ERROR: %s </span>"
	    "<a href=\"../goldenPath/help/hgTrackHubHelp.html#Debug\">Debug</a></TD>", 
	    hub->errorMessage);

    ourPrintCell(hub->hubUrl);

    ourCellStart();
    printf(
    "<input name=\"hubDisconnectButton\""
	"onClick=\"document.disconnectHubForm.elements['hubId'].value='%d';"
	    "document.disconnectHubForm.submit();return true;\" "
	    "class=\"hubField\" type=\"button\" value=\"X\">"
	    , hub->id);
    ourCellEnd();
    }

printf("</TR></tbody></TABLE>\n");
printf("</div>");
}

static void makeGenomePrint()
/* print out the name of the current database etc. */
{
getDbAndGenome(cart, &database, &organism, oldVars);
printf("<div id=\"assemblyInfo\"> \n");
printf("<B>genome:</B> %s &nbsp;&nbsp;&nbsp;<B>assembly:</B> %s  ",
	organism, hFreezeDate(database));
printf("</div>\n");
}


void hgHubConnectPublic()
/* Put up the list of public hubs and other controls for the page. */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
safef(query, sizeof(query), "select hubUrl,shortLabel,longLabel,dbList from %s", 
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
	    /* output header */
	    printf("<div id=\"publicHubs\" class=\"hubList\"> \n");
	    printf("<table id=\"publicHubsTable\"> "
		"<thead><tr> "
		    "<th>Display</th> "
		    "<th>Hub Name</th> "
		    "<th>Description</th> "
		    "<th>URL</th> "
		"</tr></thead>\n");

	    // start first row
	    printf("<tbody> <tr>");
	    gotAnyRows = TRUE;
	    }

	char *errorMessage = NULL;
	// get an id for this hub
	unsigned id = hubFindOrAddUrlInStatusTable(database, cart, 
	    url, &errorMessage);

	if ((id != 0) && isEmpty(errorMessage)) 
	    {
	    ourCellStart();
	    char hubName[32];
	    safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, id);
	    cartMakeCheckBox(cart, hubName, FALSE);
	    ourCellEnd();
	    }
	else if (!isEmpty(errorMessage))
	    {
	    // give user a chance to clear the error
	    ourCellStart();
	    printf(
	    "<input name=\"hubClearButton\""
		"onClick=\"document.resetHubForm.elements['hubUrl'].value='%s';"
		    "document.resetHubForm.submit();return true;\" "
		    "class=\"hubField\" type=\"button\" value=\"clear error\">"
		    , url);
	    ourCellEnd();
	    }
	else
	    errAbort("cannot get id for hub with url %s\n", url);

	ourPrintCell(shortLabel);
	if (isEmpty(errorMessage))
	    ourPrintCell(longLabel);
	else
	    printf("<TD>ERROR: %s</TD>", errorMessage);

	ourPrintCell(url);
	}
    }
sqlFreeResult(&sr);

if (gotAnyRows)
    {
    printf("</TR></tbody></TABLE>\n");
    }
else
    {
    printf("<div id=\"publicHubs\" class=\"hubList\"> \n");
    printf("No Public Track Hubs for this genome assembly<BR>");
    }

printf("</div>");

hDisconnectCentral(&conn);
}

static void tryHubOpen(unsigned id)
/* try to open hub, leaks trackHub structure */
{
/* try opening this again to reset error */
struct errCatch *errCatch = errCatchNew();
struct trackHub *tHub;
if (errCatchStart(errCatch))
    tHub = trackHubFromId(id);
errCatchEnd(errCatch);
if (errCatch->gotError)
    hubSetErrorMessage( errCatch->message->string, id);
else
    hubSetErrorMessage(NULL, id);
errCatchFree(&errCatch);

tHub = NULL;
}


static void doResetHub(struct cart *theCart)
{
char *url = cartOptionalString(cart, hgHubDataText);

if (url != NULL)
    {
    unsigned id = hubResetError(url);
    tryHubOpen(id);
    }
else
    errAbort("must specify url in %s\n", hgHubDataText);
}

static void doClearHub(struct cart *theCart)
{
char *url = cartOptionalString(cart, hgHubDataText);

printf("<pre>clearing hub %s\n",url);
if (url != NULL)
    hubClearStatus(url);
else
    errAbort("must specify url in %s\n", hgHubDataText);
printf("<pre>Completed\n");
}

static void doDisconnectHub(struct cart *theCart)
{
char *id = cartOptionalString(cart, "hubId");

if (id != NULL)
    {
    char buffer[1024];
    safef(buffer, sizeof buffer, "hgHubConnect.hub.%s", id);
    cartRemove(cart, buffer);
    }

cartRemove(theCart, "hubId");
}

void doMiddle(struct cart *theCart)
/* Write header and body of html page. */
{
boolean gotDisconnect = FALSE;

cart = theCart;
setUdcCacheDir();

if (cartVarExists(cart, hgHubDoClear))
    {
    doClearHub(cart);
    cartWebEnd();
    return;
    }

if (cartVarExists(cart, hgHubDoDisconnect))
    {
    gotDisconnect = TRUE;
    doDisconnectHub(cart);
    }

if (cartVarExists(cart, hgHubDoReset))
    {
    doResetHub(cart);
    }

cartWebStart(cart, NULL, "%s", pageTitle);
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("utils.js", NULL);
jsIncludeFile("jquery-ui.js", NULL);

webIncludeResourceFile("jquery-ui.css");

jsIncludeFile("ajax.js", NULL);
jsIncludeFile("hgHubConnect.js", NULL);
jsIncludeFile("jquery.cookie.js", NULL);
webIncludeResourceFile("hgHubConnect.css");

printf("<div id=\"hgHubConnectUI\"> <div id=\"description\"> \n");
printf(
   "<P>Track data hubs are collections of tracks from outside of UCSC that "
   "can be imported into the Genome Browser.  To import a public hub check "
   "the box in the list below. "
   "After import the hub will show up as a group of tracks with its own blue "
   "bar and label underneath the main browser graphic, and in the "
   "configure page. For more information, see the "
   "<A HREF=\"../goldenPath/help/hgTrackHubHelp.html\" TARGET=_blank>"
   "User's Guide</A>.</P>\n"
   "<P><B>NOTE: Because Track Hubs are created and maintained by external sources,"
   " UCSC cannot be held responsible for their content.</B></P>"
   );
printf("</div>\n");

// figure out and print out genome name
makeGenomePrint();

// check to see if we have any new hubs
unsigned newId = hubCheckForNew(database, cart);

if (newId)
    tryHubOpen(newId);

// here's a little form for the add new hub button
printf("<FORM ACTION=\"%s\" NAME=\"addHubForm\">\n",  "../cgi-bin/hgHubConnect");
cgiMakeHiddenVar("hubUrl", "");
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
puts("</FORM>");

// this the form for the disconnect hub button
printf("<FORM ACTION=\"%s\" NAME=\"disconnectHubForm\">\n",  "../cgi-bin/hgHubConnect");
cgiMakeHiddenVar("hubId", "");
cgiMakeHiddenVar(hgHubDoDisconnect, "on");
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
puts("</FORM>");

// this the form for the reset hub button
printf("<FORM ACTION=\"%s\" NAME=\"resetHubForm\">\n",  "../cgi-bin/hgHubConnect");
cgiMakeHiddenVar("hubUrl", "");
cgiMakeHiddenVar(hgHubDoReset, "on");
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
puts("</FORM>");


// ... and now the main form
if (cartVarExists(cart, hgHubConnectCgiDestUrl))
    destUrl = cartOptionalString(cart, hgHubConnectCgiDestUrl);
printf("<FORM ACTION=\"%s\" METHOD=\"POST\" NAME=\"mainForm\">\n", destUrl);
cartSaveSession(cart);

// we have two tabs for the public and unlisted hubs
printf("<div id=\"tabs\">"
       "<ul> <li><a href=\"#publicHubs\">Public Hubs</a></li>"
       "<li><a href=\"#unlistedHubs\">My Hubs</a></li> "
       "</ul> ");

hgHubConnectPublic();
hgHubConnectUnlisted();
printf("</div>");

printf("<div class=\"tabFooter\">");
cgiMakeButton("Submit", "Load Selected Hubs");

char *emailAddress = cfgOptionDefault("hub.emailAddress","genome@soe.ucsc.edu");
printf("<span class=\"small\">"
    "Contact <A HREF=\"mailto:%s\">%s</A> to add a public hub."
    "</span>\n", emailAddress,emailAddress);
printf("</div>");

cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");

printf("</div>\n");
puts("</FORM>");
cartWebEnd();
}

char *excludeVars[] = {"Submit", "submit", "hc_one_url", 
    hgHubDoReset, hgHubDoClear, hgHubDoDisconnect, hgHubDataText, 
    hgHubConnectRemakeTrackHub, NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(10);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}

