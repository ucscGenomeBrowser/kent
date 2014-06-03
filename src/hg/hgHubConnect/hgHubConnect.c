/* hgHubConnect - the CGI web-based program to select track data hubs to connect with. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "trix.h"


struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;

static char *pageTitle = "Track Data Hubs";
char *database = NULL;
char *organism = NULL;

static void ourCellStart()
{
fputs("<TD>", stdout);  // do not add a newline
}

static void ourCellEnd()
{
puts("</TD>");
}

static void ourPrintCellLink(char *str, char *url)
{
ourCellStart();
printf("<A HREF=\"%s\" TARGET=_BLANK>\n", url);
if (str != NULL)
    fputs(str, stdout); // do not add a newline -- was causing trailing blanks get copied in cut and paste 
puts("</A>");
ourCellEnd();
}

static void ourPrintCell(char *str)
{
ourCellStart();
if (str != NULL)
    fputs(str, stdout); // do not add a newline -- was causing trailing blanks get copied in cut and paste 
ourCellEnd();
}

static char *removeLastComma(char *string)
{
if (string != NULL)
    {
    int len = strlen(string);

    if ( string[len - 1] == ',')
	string[len - 1]  = 0;
    else if (len > 2 && endsWith(string,", "))
        string[len - 2] = 0;
    }
return string;
}

#define GENLISTWIDTH 40
static void printGenomeList(struct slName *genomes, int row)
/* print supported assembly names from sl list */
{
/* List of associated genomes. */
struct dyString *dy = newDyString(100);
struct dyString *dyShort = newDyString(100);
char *trimmedName = NULL;
for(; genomes; genomes = genomes->next)
    {
    trimmedName = trackHubSkipHubName(genomes->name);
    dyStringPrintf(dy,"%s, ", trimmedName);
    if (dyShort->stringSize == 0 || (dyShort->stringSize+strlen(trimmedName)<=GENLISTWIDTH))
	dyStringPrintf(dyShort,"%s, ", trimmedName);
    }
char *genomesString = removeLastComma( dyStringCannibalize(&dy));
char *genomesShort = removeLastComma( dyStringCannibalize(&dyShort));
char tempHtml[1024+strlen(genomesString)+strlen(genomesShort)];
if (strlen(genomesShort) > GENLISTWIDTH)  // If even the first element is too long, truncate it.
    genomesShort[GENLISTWIDTH] = 0;
if (strlen(genomesShort)==strlen(genomesString))
    {
    safef(tempHtml, sizeof tempHtml, "%s", genomesString);
    }
else
    {
    safef(tempHtml, sizeof tempHtml, 
	"<span id=Short%d "
	"onclick=\"javascript:"
	"document.getElementById('Short%d').style.display='none';"
	"document.getElementById('Full%d').style.display='inline';"
	"return false;\">[+]&nbsp;%s...</span>"

	"<span id=Full%d "
	"style=\"display:none\" "
	"onclick=\"javascript:"
	"document.getElementById('Full%d').style.display='none';"
	"document.getElementById('Short%d').style.display='inline';"
	"return false;\">[-]<br>%s</span>"

	, row, row, row, genomesShort 
	, row, row, row, genomesString);
    }
ourPrintCell(tempHtml);
//ourPrintCell(removeLastComma( dyStringCannibalize(&dy)));
}


static void printGenomes(struct trackHub *thub, int row)
/* print supported assembly names from trackHub */
{
/* List of associated genomes. */
struct trackHubGenome *genomes = thub->genomeList;
struct slName *list = NULL, *el;
for(; genomes; genomes = genomes->next)
    {
    el = slNameNew(genomes->name);
    slAddHead(&list, el);
    }
slReverse(&list);
printGenomeList(list, row);
}


static void hgHubConnectUnlisted(struct hubConnectStatus *hubList, 
    struct hash *publicHash)
/* Put up the list of unlisted hubs and other controls for the page. */
/* uses publicHash to distingusih public hubs from unlisted ones */
/* NOTE: Destroys hubList */
{
// put out the top of our page
printf("<div id=\"unlistedHubs\" class=\"hubList\"> \n"
    "<table id=\"unlistedHubsTable\"> \n"
    "<thead><tr> \n"
	"<th colspan=\"6\" id=\"addHubBar\"><label for=\"hubUrl\">URL:</label> \n"
	"<input name=\"hubText\" id=\"hubUrl\" class=\"hubField\""
	    "type=\"text\" size=\"65\"> \n"
	"<input name=\"hubAddButton\""
	    "onClick=\"hubText.value=$.trim(hubText.value);if(validateUrl($('#hubUrl').val())) { document.addHubForm.elements['hubUrl'].value=hubText.value;"
		"document.addHubForm.submit();return true;} else { return false;}\" "
		"class=\"hubField\" type=\"button\" value=\"Add Hub\">\n"
	"</th> \n"
    "</tr> \n");

// count up the number of unlisted hubs we currently have
int unlistedHubCount = 0;
struct hubConnectStatus *unlistedHubList = NULL;
struct hubConnectStatus *hub, *nextHub;

for(hub = hubList; hub; hub = nextHub)
    {
    nextHub = hub->next;
    // if url is not in publicHash, it's unlisted */
    if (!((publicHash != NULL) && hashLookup(publicHash, hub->hubUrl)))
	{
	unlistedHubCount++;
	slAddHead(&unlistedHubList, hub);
	}
    }

hubList = NULL;  // hubList no longer valid

if (unlistedHubCount == 0)
    {
    // nothing to see here
    printf("<tr><td>No Unlisted Track Hubs</td></tr>");
    printf("</thead></table></div>");
    return;
    }

// time to output the big table.  First the header
printf(
    "<tr> "
	"<th>Display</th> "
	"<th>Hub Name</th> "
	"<th>Description</th> "
	"<th>Assemblies</th> "
    "</tr>\n"
    "</thead>\n");

// start first row
printf("<tbody><tr>");

int count = 0;
for(hub = unlistedHubList; hub; hub = hub->next)
    {
    if (count)
	webPrintLinkTableNewRow();  // ends last row and starts a new one
    count++;

    // if there's an error message, we don't let people select it
    if (isEmpty(hub->errorMessage))
	{
	ourCellStart();
	char hubName[32];
	safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, hub->id);
	if (cartUsualBoolean(cart, hubName, FALSE))
	    printf("<input name=\"hubDisconnectButton\""
		"onClick="
		"\" document.disconnectHubForm.elements['hubId'].value= '%d';"
		"document.disconnectHubForm.submit();return true;\" "
		"class=\"hubDisconnectButton\" type=\"button\" value=\"Disconnect\">\n", hub->id);
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
		"class=\"hubField\" type=\"button\" value=\"check hub\">\n"
		, hub->hubUrl);
	ourCellEnd();
	}
    if (hub->trackHub != NULL)
	{
	ourPrintCellLink(hub->trackHub->shortLabel, hub->hubUrl);
	}
    else
	ourPrintCell("");

    if (!isEmpty(hub->errorMessage))
	printf("<TD><span class=\"hubError\">ERROR: %s </span>"
	    "<a href=\"../goldenPath/help/hgTrackHubHelp.html#Debug\">Debug</a></TD>\n", 
	    hub->errorMessage);
    else if (hub->trackHub != NULL)
	{
	if (hub->trackHub->descriptionUrl != NULL)
	    ourPrintCellLink(hub->trackHub->longLabel, hub->trackHub->descriptionUrl);
	else
	    ourPrintCell(hub->trackHub->longLabel);
	}
    else
	ourPrintCell("");

    if (hub->trackHub != NULL)
	printGenomes(hub->trackHub, count);
    else
	ourPrintCell("");
    }

printf("</TR></tbody></TABLE>\n");
printf("</div>");
}

static void addPublicHubsToHubStatus(struct sqlConnection *conn, char *publicTable, char  *statusTable)
/* add url's in the hubPublic table to the hubStatus table if they aren't there already */
{
char query[1024];
sqlSafef(query, sizeof(query), "select hubUrl from %s where hubUrl not in (select hubUrl from %s)\n", publicTable, statusTable); 
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *errorMessage = NULL;
    char *url = row[0];

    // add this url to the hubStatus table
    hubFindOrAddUrlInStatusTable(database, cart, url, &errorMessage);
    }
}

struct hash *getUrlSearchHash(char *trixFile, char *hubSearchTerms)
/* find hubs that match search term in trixFile */
{
struct hash *urlSearchHash = newHash(5);
struct trix *trix = trixOpen(trixFile);
int trixWordCount = chopString(hubSearchTerms, " ", NULL, 0);
char *trixWords[trixWordCount];
trixWordCount = chopString(hubSearchTerms, " ", trixWords, trixWordCount);

struct trixSearchResult *tsList = trixSearch(trix, trixWordCount, trixWords, TRUE);
for ( ; tsList != NULL; tsList = tsList->next)
    hashStore(urlSearchHash, tsList->itemId);

return urlSearchHash;
}

static struct hash *outputPublicTable(struct sqlConnection *conn, char *publicTable, char *statusTable)
/* Put up the list of public hubs and other controls for the page. */
{
char *trixFile = cfgOptionEnvDefault("HUBSEARCHTRIXFILE", "hubSearchTrixFile", "/gbdb/hubs/public.ix");
char *hubSearchTerms = cartOptionalString(cart, hgHubSearchTerms);
boolean haveTrixFile = fileExists(trixFile);
struct hash *urlSearchHash = NULL;

printf("<div id=\"publicHubs\" class=\"hubList\"> \n");

if (haveTrixFile && !isEmpty(hubSearchTerms))
    {
    strLower(hubSearchTerms);
    urlSearchHash = getUrlSearchHash(trixFile, hubSearchTerms);
    }

// if we have search terms, put out the line telling the user so
if (!isEmpty(hubSearchTerms))
    {
    printf("<BR>List restricted by search terms : %s\n", hubSearchTerms);
    puts("<input name=\"hubDeleteSearchButton\""
	"onClick="
	"\" document.searchHubForm.elements['hubSearchTerms'].value=\'\';"
	"document.searchHubForm.submit();return true;\" "
	"class=\"hubField\" type=\"button\" value=\"Show All Hubs\">\n");
    printf("<BR>\n");
    }

// if we have a trix file, draw the search box
if (haveTrixFile)
    {
    puts("<input name=\"hubSearchTerms\" id=\"hubSearchTerms\" class=\"hubField\""
	"type=\"text\" size=\"65\"> \n"
    "<input name=\"hubSearchButton\""
	"onClick="
	"\" document.searchHubForm.elements['hubSearchTerms'].value=hubSearchTerms.value;"
	    "document.searchHubForm.submit();return true;\" "
	    "class=\"hubField\" type=\"button\" value=\"Search Public Hubs\">\n");
    }

// make sure all the public hubs are in the hubStatus table.
addPublicHubsToHubStatus(conn, publicTable, statusTable);

struct hash *publicHash = NULL;
char query[512];
bool hasDescription = sqlColumnExists(conn, publicTable, "descriptionUrl");
if (hasDescription)
    sqlSafef(query, sizeof(query), "select p.hubUrl,p.shortLabel,p.longLabel,p.dbList,s.errorMessage,s.id,p.descriptionUrl from %s p,%s s where p.hubUrl = s.hubUrl", 
	  publicTable, statusTable); 
else
    sqlSafef(query, sizeof(query), "select p.hubUrl,p.shortLabel,p.longLabel,p.dbList,s.errorMessage,s.id from %s p,%s s where p.hubUrl = s.hubUrl", 
	 publicTable, statusTable); 

struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
int count = 0;
boolean gotAnyRows = FALSE;
while ((row = sqlNextRow(sr)) != NULL)
    {
    ++count;
    char *url = row[0], *shortLabel = row[1], *longLabel = row[2], 
    	  *dbList = row[3], *errorMessage = row[4], *descriptionUrl = row[6];
    int id = atoi(row[5]);

    if ((urlSearchHash != NULL) && (hashLookup(urlSearchHash, url) == NULL))
	continue;

    struct slName *dbListNames = slNameListFromComma(dbList);

    if (gotAnyRows)
	webPrintLinkTableNewRow();
    else
	{
	/* output header */

	printf("<table id=\"publicHubsTable\"> "
	    "<thead><tr> "
		"<th>Display</th> "
		"<th>Hub Name</th> "
		"<th>Description</th> "
		"<th>Assemblies</th> "
	    "</tr></thead>\n");

	// start first row
	printf("<tbody> <tr>");
	gotAnyRows = TRUE;

	// allocate the hash to store hubUrl's
	publicHash = newHash(5);
	}

    if ((id != 0) && isEmpty(errorMessage)) 
	{
	ourCellStart();
	char hubName[32];
	safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, id);
	if (cartUsualBoolean(cart, hubName, FALSE))
	    printf("<input name=\"hubDisconnectButton\""
		"onClick="
		"\" document.disconnectHubForm.elements['hubId'].value= '%d';"
		"document.disconnectHubForm.submit();return true;\" "
		"class=\"hubDisconnectButton\" type=\"button\" value=\"Disconnect\">\n", id);
	else
	    {
	    // get first name off of list of supported databases
	    char * name = dbListNames->name;

	    // if the name isn't currently loaded, we assume it's a hub
	    if (!hDbExists(name))
		{
		char buffer[512];

		safef(buffer, sizeof buffer, "hub_%d_%s",  id, name);
		name = cloneString(buffer);
		}

	    printf("<input name=\"hubConnectButton\""
	    "onClick="
		"\" document.connectHubForm.elements['hubUrl'].value= '%s';"
		"document.connectHubForm.elements['db'].value= '%s';"
		"document.connectHubForm.submit();return true;\" "
		"class=\"hubConnectButton\" type=\"button\" value=\"Connect\">\n", url,name);
	    }

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
		"class=\"hubField\" type=\"button\" value=\"check hub\">"
		, url);
	ourCellEnd();
	}
    else
	errAbort("cannot get id for hub with url %s\n", url);

    ourPrintCellLink(shortLabel, url);

    if (isEmpty(errorMessage))
	{
	if (hasDescription && !isEmpty(descriptionUrl))
	    ourPrintCellLink(longLabel, descriptionUrl);
	else
	    ourPrintCell(longLabel);
	}
    else
	printf("<TD><span class=\"hubError\">ERROR: %s </span>"
	    "<a href=\"../goldenPath/help/hgTrackHubHelp.html#Debug\">Debug</a></TD>", 
	    errorMessage);

    printGenomeList(dbListNames, count); 

    hashStore(publicHash, url);
    }
sqlFreeResult(&sr);

if (gotAnyRows)
    printf("</TR></tbody></TABLE>\n");

printf("</div>");
return publicHash;
}


struct hash *hgHubConnectPublic()
/* Put up the list of public hubs and other controls for the page. */
{
struct hash *retHash = NULL;
struct sqlConnection *conn = hConnectCentral();
char *publicTable = cfgOptionEnvDefault("HGDB_HUB_PUBLIC_TABLE", 
	hubPublicTableConfVariable, defaultHubPublicTableName);
char *statusTable = cfgOptionEnvDefault("HGDB_HUB_STATUS_TABLE", 
	hubStatusTableConfVariable, defaultHubStatusTableName);
if (!(sqlTableExists(conn, publicTable) && 
	(retHash = outputPublicTable(conn, publicTable,statusTable)) != NULL ))
    {
    printf("<div id=\"publicHubs\" class=\"hubList\"> \n");
    printf("No Public Track Hubs<BR>");
    printf("</div>");
    }
hDisconnectCentral(&conn);

return retHash;
}

static void tryHubOpen(unsigned id)
/* try to open hub, leaks trackHub structure */
{
/* try opening this again to reset error */
struct sqlConnection *conn = hConnectCentral();
struct errCatch *errCatch = errCatchNew();
struct hubConnectStatus *hub = NULL;
if (errCatchStart(errCatch))
    hub = hubConnectStatusForId(conn, id);
errCatchEnd(errCatch);
if (errCatch->gotError)
    hubUpdateStatus( errCatch->message->string, NULL);
else
    hubUpdateStatus(NULL, hub);
errCatchFree(&errCatch);

hDisconnectCentral(&conn);
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


static void checkTrackDbs(struct hubConnectStatus *hubList)
{
struct hubConnectStatus *hub = hubList;

for(; hub; hub = hub->next)
    {
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	{
	hubAddTracks(hub, database);
	}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	hub->errorMessage = cloneString(errCatch->message->string);
	hubUpdateStatus( errCatch->message->string, hub);
	}
    else
	hubUpdateStatus(NULL, hub);
    }
}

void doMiddle(struct cart *theCart)
/* Write header and body of html page. */
{
cart = theCart;

if (cartVarExists(cart, hgHubDoClear))
    {
    doClearHub(cart);
    cartWebEnd();
    return;
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
   " UCSC is not responsible for their content.</B></P>"
   );
printf("</div>\n");

getDbAndGenome(cart, &database, &organism, oldVars);

char *survey = cfgOptionEnv("HGDB_HUB_SURVEY", "hubSurvey");
char *surveyLabel = cfgOptionEnv("HGDB_HUB_SURVEY_LABEL", "hubSurveyLabel");

if (survey && differentWord(survey, "off"))
    hPrintf("<span style='background-color:yellow;'><A HREF='%s' TARGET=_BLANK><EM><B>%s</EM></B></A></span>\n", survey, surveyLabel ? surveyLabel : "Take survey");
hPutc('\n');

// grab all the hubs that are listed in the cart
struct hubConnectStatus *hubList =  hubConnectStatusListFromCartAll(cart);

checkTrackDbs(hubList);

// here's a little form for the add new hub button
printf("<FORM ACTION=\"%s\" NAME=\"addHubForm\">\n",  "../cgi-bin/hgGateway");
cgiMakeHiddenVar("hubUrl", "");
cgiMakeHiddenVar( hgHubDoFirstDb, "on");
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
puts("</FORM>");

// this is the form for the connect hub button
printf("<FORM ACTION=\"%s\" NAME=\"connectHubForm\">\n",  "../cgi-bin/hgGateway");
cgiMakeHiddenVar("hubUrl", "");
cgiMakeHiddenVar("db", "");
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
puts("</FORM>");

// this is the form for the disconnect hub button
printf("<FORM ACTION=\"%s\" NAME=\"disconnectHubForm\">\n",  "../cgi-bin/hgGateway");
cgiMakeHiddenVar("db", "hg19");
cgiMakeHiddenVar("hubId", "");
cgiMakeHiddenVar(hgHubDoDisconnect, "on");
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
puts("</FORM>");

// this is the form for the reset hub button
printf("<FORM ACTION=\"%s\" NAME=\"resetHubForm\">\n",  "../cgi-bin/hgHubConnect");
cgiMakeHiddenVar("hubUrl", "");
cgiMakeHiddenVar(hgHubDoReset, "on");
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
puts("</FORM>");

// this is the form for the search hub button
printf("<FORM ACTION=\"%s\" NAME=\"searchHubForm\">\n",  "../cgi-bin/hgHubConnect");
cgiMakeHiddenVar(hgHubSearchTerms, "");
cgiMakeHiddenVar(hgHubDoSearch, "on");
puts("</FORM>");

// ... and now the main form
printf("<FORM ACTION=\"%s\" METHOD=\"POST\" NAME=\"mainForm\">\n", "../cgi-bin/hgGateway");
cartSaveSession(cart);

// we have two tabs for the public and unlisted hubs
printf("<div id=\"tabs\">"
       "<ul> <li><a href=\"#publicHubs\">Public Hubs</a></li>"
       "<li><a href=\"#unlistedHubs\">My Hubs</a></li> "
       "</ul> ");

struct hash *publicHash = hgHubConnectPublic();
hgHubConnectUnlisted(hubList, publicHash);
printf("</div>");

printf("<div class=\"tabFooter\">");

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
long enteredMainTime = clock1000();
oldVars = hashNew(10);
setUdcCacheDir();
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgHubConnect", enteredMainTime);
return 0;
}

