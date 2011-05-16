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

#define hgHubDataText      "hgHub_customText"

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

static boolean nameInCommaList(char *name, char *commaList)
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

static void enterHubInStatus(struct trackHub *tHub, boolean unlisted)
/* put the hub status in the hubStatus table */
{
struct sqlConnection *conn = hConnectCentral();

/* calculate dbList */
struct dyString *dy = newDyString(1024);
struct hashEl *hel;
struct hashCookie cookie = hashFirst(tHub->genomeHash);
int dbCount = 0;

while ((hel = hashNext(&cookie)) != NULL)
    {
    dbCount++;
    dyStringPrintf(dy,"%s,", hel->name);
    }


char query[512];
safef(query, sizeof(query), "insert into %s (hubUrl,status,shortLabel, longLabel, dbList, dbCount) values (\"%s\",%d,\"%s\",\"%s\", \"%s\", %d)",
    hubStatusTableName, tHub->url, unlisted ? 1 : 0,
    tHub->shortLabel, tHub->longLabel,
    dy->string, dbCount);
sqlUpdate(conn, query);
hDisconnectCentral(&conn);
}

static unsigned getHubId(char *url, char **errorMessage)
/* find id for url in hubStatus table */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
char **row;
boolean foundOne = FALSE;
int id = 0;

safef(query, sizeof(query), "select id,errorMessage from %s where hubUrl = \"%s\"", hubStatusTableName, url);

struct sqlResult *sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    if (foundOne)
	errAbort("more than one line in %s with hubUrl %s\n", 
	    hubStatusTableName, url);

    foundOne = TRUE;

    char *thisId = row[0], *thisError = row[1];

    if (!isEmpty(thisError))
	*errorMessage = cloneString(thisError);

    id = sqlUnsigned(thisId);
    }
sqlFreeResult(&sr);

hDisconnectCentral(&conn);

return id;
}

static boolean hubHasDatabase(unsigned id, char *database)
/* check to see if hub specified by id supports database */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];

safef(query, sizeof(query), "select dbList from %s where id=%d", 
    hubStatusTableName, id); 
char *dbList = sqlQuickString(conn, query);
boolean gotIt = FALSE;

if (nameInCommaList(database, dbList))
    gotIt = TRUE;

hDisconnectCentral(&conn);

freeMem(dbList);

return gotIt;
}

static boolean fetchHub(char *url, boolean unlisted)
{
struct errCatch *errCatch = errCatchNew();
struct trackHub *tHub = NULL;
boolean gotWarning = FALSE;
unsigned id = 0;

if (errCatchStart(errCatch))
    tHub = trackHubOpen(url, "1"); // open hub.. it'll get renamed later
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    gotWarning = TRUE;
    warn(errCatch->message->string);
    }
errCatchFree(&errCatch);

if (gotWarning)
    {
    return 0;
    }

if (hashLookup(tHub->genomeHash, database) != NULL)
    {
    enterHubInStatus(tHub, unlisted);
    }
else
    {
    warn("requested hub at %s does not have data for %s\n", url, database);
    return 0;
    }

trackHubClose(&tHub);

char *errorMessage = NULL;
id = getHubId(url, &errorMessage);
return id;
}

static void getAndSetHubStatus(char *url, boolean set, boolean unlisted)
{
char *errorMessage = NULL;
unsigned id;

if ((id = getHubId(url, &errorMessage)) == 0)
    {
    if ((id = fetchHub(url, unlisted)) == 0)
	return;
    }
else if (!hubHasDatabase(id, database))
    {
    warn("requested hub at %s does not have data for %s\n", url, database);
    return;
    }

char hubName[32];
safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, id);
if (set)
    cartSetString(cart, hubName, "1");
}

static unsigned findOrAddUrlInStatusTable( char *url, char **errorMessage)
/* find this url in the status table, and return its id and errorMessage (if an errorMessage exists) */
{
int id = 0;

*errorMessage = NULL;

if ((id = getHubId(url, errorMessage)) > 0)
    return id;

getAndSetHubStatus(url, FALSE, FALSE);

if ((id = getHubId(url, errorMessage)) == 0)
    errAbort("inserted new hubUrl %s, but cannot find it", url);

return id;
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
	unsigned id = findOrAddUrlInStatusTable( url, &errorMessage);

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

void checkForNewHub(struct cart *cart)
/* see if the user just typed in a new hub url */
{
char *url = cartOptionalString(cart, hgHubDataText);

if (url != NULL)
    {
    getAndSetHubStatus(url, TRUE, TRUE);
    }
}

static void clearHubStatus(char *url)
{
struct sqlConnection *conn = hConnectCentral();
char query[512];

safef(query, sizeof(query), "select id from %s where hubUrl = \"%s\"", hubStatusTableName, url);
unsigned id = sqlQuickNum(conn, query);

if (id == 0)
    errAbort("could not find url %s in status table (%s)\n", 
	url, hubStatusTableName);

safef(query, sizeof(query), "delete from %s where hubUrl = \"%s\"", hubStatusTableName, url);

sqlUpdate(conn, query);
hDisconnectCentral(&conn);

printf("%s status has been cleared\n", url);
}

static void doClearHub(struct cart *theCart)
{
char *url = cartOptionalString(cart, hgHubDataText);

if (url != NULL)
    clearHubStatus(url);
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

    checkForNewHub(cart);
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

