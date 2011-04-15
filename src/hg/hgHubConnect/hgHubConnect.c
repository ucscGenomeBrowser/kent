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
#include "trashDir.h"
#include "hPrint.h"

#define TEXT_ENTRY_ROWS 7
#define TEXT_ENTRY_COLS 73
#define CONFIG_ENTRY_ROWS 3
#define SAVED_LINE_COUNT  50

#define HUB_CUSTOM_TEXT_ALT_VAR  "hghub_customText"
#define HUB_CUSTOM_FILE_VAR      "hghub.customFile"
#define HUB_UPDATED_ID           "hghub_updatedId"



#define hgHubDataText      HUB_CUSTOM_TEXT_ALT_VAR
#define hgHubDataFile      HUB_CUSTOM_FILE_VAR
#define hgHubUpdatedId     HUB_UPDATED_ID

#define hgHub             "hgHub_"  /* prefix for all control variables */
#define hgHubDo            hgHub   "do_"    /* prefix for all commands */
#define hgHubDoAdd         hgHubDo "add"


struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;

static char *destUrl = "../cgi-bin/hgTracks";
static char *pageTitle = "Import Tracks from Data Hubs";
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

void hgHubConnectPrivate()
/* Put up the list of private hubs and other controls for the page. */
{
printf("<B>List of Private Hubs</B><BR>");
struct hubConnectStatus *hub, *hubList =  hubConnectStatusListFromCart(cart);
int count = 0;
for(hub = hubList; hub; hub = hub->next)
    {
    if (hub->id > 0)
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
	safef(hubName, sizeof(hubName), "%s%d", hgHubConnectHubVarPrefix, hub->id);
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
    printf("No Private Track Hubs for this genome assembly<BR>");
}

static void makeNewHubButton()
{
printf("<FORM ACTION=\"%s\" METHOD=\"POST\" NAME=\"secondForm\">\n", "../cgi-bin/hgHubConnect");
cartSaveSession(cart);
cgiMakeHiddenVar(hgHubDoAdd, "on");
cgiMakeButton("add", "add new private hub");
printf("</FORM>\n");
}

static void makeGenomePrint()
{
getDbAndGenome(cart, &database, &organism, oldVars);
printf("<B>genome:</B> %s &nbsp;&nbsp;&nbsp;<B>assembly:</B> %s &nbsp;&nbsp;&nbsp;[%s] ", 
	organism, hFreezeDate(database), database);
}

void hgHubConnectPublic()
/* Put up the list of external hubs and other controls for the page. */
{
printf("<B>List of Public Hubs</B><BR>");
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
hDisconnectCentral(&conn);
}

static void addIntro()
{
printf("Enter URL to remote hub.<BR>\n");
}

void makeClearButton(char *field)
/* UI button that clears a text field */
{
char javascript[1024];
safef(javascript, sizeof javascript,
        "document.mainForm.%s.value = '';", field);
cgiMakeOnClickButton(javascript, "&nbsp;Clear&nbsp;");
}

void addPrivateHubForm(struct hubConnectStatus *hub, char *err)
/* display UI for adding private hubs by URL or pasting data */
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
puts("Add your own private data hub for the browser.");
addIntro();
puts("<P>");

/* row for error message */
if (err)
    printf("<P><B>&nbsp;&nbsp;&nbsp;&nbsp;<I><FONT COLOR='RED'>Error</I></FONT>&nbsp;%s</B><P>", err);

printf("Enter URL:");

hTextVar(hgHubDataText, "", 60);
cgiMakeSubmitButton();

puts("</FORM>");
}

void helpPrivateHub()
{
printf("Private hubs are constructed the same way as public hubs, but they "
   "aren't listed in hgcentral<BR>\n");
}

void doAddPrivateHub(struct cart *theCart, char *err)
/* Write header and body of html page. */
{
cartWebStart(cart, database, "Add Private Hub");
addPrivateHubForm(NULL, err);
helpPrivateHub();
cartWebEnd(cart);
}

void hubSaveInCart(struct cart *cart, struct hubConnectStatus *hub)
{
char hubName[1024];
char *oldHubTrashName = cartOptionalString(cart, hubFileVar());
static struct tempName tn;
trashDirFile(&tn, "hub", "hub_", ".txt");
char *hubTrashName = tn.forCgi;
FILE *f = mustOpen(hubTrashName, "w");

if (oldHubTrashName == NULL)
    {
    hub->id = -1;
    }
else
    {
    struct lineFile *lf = lineFileOpen(oldHubTrashName, TRUE);
    int lineSize;
    char *line;
    int count = 1;

    while (lineFileNext(lf, &line, &lineSize))
	{
	count++;
	fprintf(f, "%s\n", line);
	}
    lineFileClose(&lf);
    unlink(oldHubTrashName);
    hub->id = -count;
    }
hubWriteToFile(f, hub);
carefulClose(&f);

safef(hubName, sizeof(hubName), "%s%d", hgHubConnectHubVarPrefix, hub->id);
cartSetString(cart, hubName, "1");

cartSetString(cart, hubFileVar(), hubTrashName);
}

void checkForNewHub(struct cart *cart)
{
char *url = cartOptionalString(cart, hgHubDataText);

if (url != NULL)
    {
    struct hubConnectStatus *hub = NULL;
    struct trackHub *tHub = NULL;

    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	tHub = trackHubOpen(url, "1");
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	warn(errCatch->message->string);
	return;
	}
    errCatchFree(&errCatch);
    AllocVar(hub);

    hub->hubUrl = cloneString(url);
    hub->errorMessage = "";
    hub->shortLabel = tHub->shortLabel;
    hub->longLabel = tHub->longLabel;
    hub->dbCount = 0;
    AllocArray(hub->dbArray, 1);
    hub->dbArray[0] = database;

    hubSaveInCart(cart, hub);
    }
}

void doMiddle(struct cart *theCart)
/* Write header and body of html page. */
{
cart = theCart;
setUdcCacheDir();
if (cartVarExists(cart, hgHubDoAdd))
    doAddPrivateHub(cart, NULL);
else
    {
    cartWebStart(cart, NULL, pageTitle);
    checkForNewHub(cart);
    printf("<FORM ACTION=\"%s\" METHOD=\"POST\" NAME=\"mainForm\">\n", destUrl);
    cartSaveSession(cart);

    cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");

    printf(
       "<P>Track data hubs are collections of tracks from outside of UCSC that can be imported into "
       "the Genome Browser.  To import a public hub check the box in the list below. "
       "After import the hub will show up as a group of tracks with its own blue "
       "bar and label underneath the main browser graphic, and in the configure page. To arrange "
       "for your own track data hub to appear in this list, please contact genome@soe.ucsc.edu.</P>\n"
       );
    makeGenomePrint();
    cgiMakeSubmitButton();

    printf("<BR>");
    hgHubConnectPublic();
    puts("<BR>");
    hgHubConnectPrivate();
    puts("</FORM>");

    makeNewHubButton();
    }
cartWebEnd();
}

char *excludeVars[] = {"Submit", "submit", "hc_one_url", hgHubConnectCgiDestUrl, hgHubDoAdd, hgHubDataText, NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(10);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}

