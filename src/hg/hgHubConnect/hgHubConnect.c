/* hgHubConnect - the CGI web-based program to select track data hubs to connect with. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "errAbort.h"
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
#include "net.h"
#include "hubSearchText.h"

struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;

static char *pageTitle = "Track Data Hubs";
char *database = NULL;
char *organism = NULL;

struct hubOutputStructure
    {
    struct hubOutputStructure *next;
    struct dyString *metaTags;
    struct dyString *descriptionMatch;
    struct genomeOutputStructure *genomes;
    int genomeCount;
    struct hash *genomeOutHash;
    };

struct genomeOutputStructure
    {
    struct genomeOutputStructure *next;
    struct dyString *shortLabel;
    struct dyString *metaTags;
    struct dyString *descriptionMatch;
    struct tdbOutputStructure *tracks;
    struct dyString *assemblyLink;
    char *genomeName;
    char *positionString;
    int trackCount;
    struct hash *tdbOutHash;
    int hitCount;
    };

struct tdbOutputStructure
    {
    struct tdbOutputStructure *next;
    struct dyString *shortLabel;
    struct dyString *metaTags;
    struct dyString *descriptionMatch;
    struct dyString *configUrl;
    struct tdbOutputStructure *children;
    int childCount;
    };

struct hubEntry
// for entries pulled from hubPublic
    {
    struct hubEntry *next;
    char *hubUrl;
    char *shortLabel;
    char *longLabel;
    char *dbList;
    char *errorMessage;
    int id;
    char *descriptionUrl;
    bool tableHasDescriptionField;
    };

struct hubEntry *hubEntryTextLoad(char **row, bool hasDescription)
{
struct hubEntry *ret;
AllocVar(ret);
ret->hubUrl = cloneString(row[0]);
ret->shortLabel = cloneString(row[1]);
ret->longLabel = cloneString(row[2]);
ret->dbList = cloneString(row[3]);
ret->errorMessage = cloneString(row[4]);
ret->id = sqlUnsigned(row[5]);
if (hasDescription)
    ret->descriptionUrl = cloneString(row[6]);
else
    ret->descriptionUrl = NULL;
return ret;
}


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
printf("<A class=\"cv\" HREF=\"%s\" TARGET=_BLANK>\n", url);
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
static void printGenomeList(char *hubUrl, struct slName *genomes, int row, boolean withLink)
/* print supported assembly names from sl list */
{
struct dyString *dyHtml = newDyString(1024);
struct dyString *dyShortHtml = newDyString(1024);

// create two strings: one shortened to GENLISTWIDTH characters
// and another one with all genomes
int charCount = 0;
struct slName *genome = genomes;
for(; genome; genome = genome->next)
    {
    char *trimmedName = trackHubSkipHubName(genome->name);
    char *shortName = cloneString(trimmedName);
    // If even the first element is too long, truncate its short name.
    if (genome==genomes && strlen(trimmedName) > GENLISTWIDTH)  
        shortName[GENLISTWIDTH] = 0;

    // append to dyShortHtml if necessary
    if (charCount == 0 || (charCount+strlen(trimmedName)<=GENLISTWIDTH))
        { 
        if (withLink)
            dyStringPrintf(dyShortHtml,"<a title='Connect hub and open the %s assembly' href='hgTracks?hubUrl=%s&genome=%s&position=lastDbPos'>%s</a>" , genome->name, hubUrl, genome->name, shortName);
        else
            dyStringPrintf(dyShortHtml,"%s" , shortName);
        dyStringPrintf(dyShortHtml,", ");
        }
    freeMem(shortName); 

    charCount += strlen(trimmedName);

    // always append to dyHtml
    if (withLink)
        dyStringPrintf(dyHtml,"<a title='Connect hub and open the %s assembly' href='hgTracks?hubUrl=%s&genome=%s&position=lastDbPos'>%s</a>" , genome->name, hubUrl, genome->name, trimmedName);
    else
        dyStringPrintf(dyHtml,"%s" , trimmedName);

    if (genome->next)
        {
        dyStringPrintf(dyHtml,", ");
        }

    }

char *longHtml = dyStringCannibalize(&dyHtml);
char *shortHtml = dyStringCannibalize(&dyShortHtml);
shortHtml = removeLastComma(shortHtml);

if (charCount < GENLISTWIDTH)
    ourPrintCell(shortHtml);
else
    {
    char id[256];
    char tempHtml[1024+strlen(longHtml)+strlen(shortHtml)];
    safef(tempHtml, sizeof tempHtml, 
	"<span id=Short%d><span style='cursor:default' id='Short%dPlus'>[+]&nbsp;</span>%s...</span>"
	"<span id=Full%d style=\"display:none\"><span style='cursor:default' id='Full%dMinus'>[-]<br></span>%s</span>"
	, row, row, shortHtml
	, row, row, longHtml);

    safef(id, sizeof id, "Short%dPlus", row);
    jsOnEventByIdF("click", id,
	"document.getElementById('Short%d').style.display='none';"
	"document.getElementById('Full%d').style.display='inline';"
	"return false;"
	, row, row);

    safef(id, sizeof id, "Full%dMinus", row);
    jsOnEventByIdF("click", id, 
	"document.getElementById('Full%d').style.display='none';"
	"document.getElementById('Short%d').style.display='inline';"
	"return false;"
	, row, row);
    ourPrintCell(tempHtml);
    }

freeMem(longHtml);
freeMem(shortHtml);
}


static void printGenomes(struct trackHub *thub, int row, boolean withLink)
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
printGenomeList(thub->url, list, row, withLink);
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
	"<input name=\"hubText\" id=\"hubUrl\" class=\"hubField\" "
	    "type=\"text\" size=\"65\"> \n"
	"<input name=\"hubAddButton\" id='hubAddButton' "
		"class=\"hubField\" type=\"button\" value=\"Add Hub\">\n"
	"</th> \n"
    "</tr> \n");
jsOnEventById("click", "hubAddButton", 
    "var hubText = document.getElementById('hubUrl');"
    "hubText.value=$.trim(hubText.value);"
    "if(validateUrl($('#hubUrl').val())) { "
    " document.addHubForm.elements['hubUrl'].value=hubText.value;"
    " document.addHubForm.submit(); return true; } "
    "else { return false; }"
    );

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
printf("<tbody>");

char id[256];
int count = 0;
for(hub = unlistedHubList; hub; hub = hub->next)
    {
    char hubName[64];
    safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, hub->id);
    if (!cartUsualBoolean(cart, hubName, FALSE))
	continue;

    if (count)
	webPrintLinkTableNewRow();  // ends last row and starts a new one
    count++;

    puts("<tr>");

    ourCellStart();
    safef(id, sizeof id, "hubDisconnectButtonU%d", count);
    printf("<input name=\"hubDisconnectButton\" id='%s' "
	"class=\"hubDisconnectButton\" type=\"button\" value=\"Disconnect\">\n", id);
    jsOnEventByIdF("click", id, 
	"document.disconnectHubForm.elements['hubId'].value='%d';"
	"document.disconnectHubForm.submit(); return true;", hub->id);
    ourCellEnd();

    if (hub->trackHub != NULL)
	{
	ourPrintCellLink(hub->trackHub->shortLabel, hub->hubUrl);
	}
    else
	ourPrintCell("");

    boolean hubHasError = (!isEmpty(hub->errorMessage));
    if (hubHasError)
	{
	ourCellStart();
	printf("<span class=\"hubError\">ERROR: %s </span>"
	    "<a TARGET=_BLANK href=\"../goldenPath/help/hgTrackHubHelp.html#Debug\">Debug Help</a>\n", 
	    hub->errorMessage);
	
	safef(id, sizeof id, "hubClearButton%d", count);
	// give people a chance to clear the error 
	printf("<input name=\"hubClearButton\"  id='%s' "
		"class=\"hubButton\" type=\"button\" value=\"Retry Hub\">"
		, id);
	jsOnEventByIdF("click", id,
	    "document.resetHubForm.elements['hubCheckUrl'].value='%s';"
	    "document.resetHubForm.submit(); return true;", hub->hubUrl);
	ourCellEnd();
	}
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
	printGenomes(hub->trackHub, count, !hubHasError);
    else
	ourPrintCell("");

    puts("</tr>");
    }

printf("</tbody></TABLE>\n");
printf("</div>");
}

void doValidateNewHub(char *hubUrl)
/* Run hubCheck on a hub. */
{
struct dyString *cmd = dyStringNew(0);
udcSetCacheTimeout(1);
dyStringPrintf(cmd, "loader/hubCheck -htmlOut -noTracks %s", hubUrl);
printf("<div id=\"validateHubResult\" class=\"hubTdbTree\" style=\"overflow: auto\"></div>");
FILE *f = popen(cmd->string, "r");
if (f == NULL)
    errAbort("popen: error running command: \"%s\"", cmd->string);
char buf[1024];
while (fgets(buf, sizeof(buf), f))
    {
    jsInlineF("%s", buf);
    }
if (pclose(f) == -1)
    errAbort("pclose: error for command \"%s\"", cmd->string);
// the 'false' below prevents a few hub-search specific jstree configuration options
jsInline("hubSearchTree.init(false);");
dyStringFree(&cmd);
}

void hgHubConnectDeveloperMode()
/* Put up the controls for the "Hub Development" Tab, which includes a button to run the
 * hubCheck utility on a hub and load a hub with the udcTimeout and measureTiming
 * variables turned on */
{
// put out the top of our page
char *hubUrl = cartOptionalString(cart, "validateHubUrl");
boolean doHubValidate = cartVarExists(cart, hgHubDoHubCheck);

// the outer div for all the elements in the tab
printf("\n<div id=\"hubDeveloper\" class=\"hubList\">\n");

// the row to enter in the url and the button and settings to validate/load
printf("<div class=\"addHubBar roundCorners\" id=\"validateHubUrlRow\">\n");
printf("<label for=\"validateHubUrl\"><b>URL:</b></label>\n");
if (hubUrl != NULL)
    {
    printf("<input name=\"validateText\" id=\"validateHubUrl\" class=\"hubField\" "
        "type=\"text\" size=\"65\" value=\"%s\"> \n", hubUrl);
    }
else
    {
    printf("<input name=\"validateText\" id=\"validateHubUrl\" class=\"hubField\" "
        "type=\"text\" size=\"65\"> \n");
    }
printf("<input name=\"hubValidateButton\" id='hubValidateButton' "
    "class=\"hubField\" type=\"button\" value=\"Check Hub settings\">\n");
printf(" or \n");
printf("<input name=\"hubLoadMaybeTiming\" id='hubLoadMaybeTiming' "
    "class=\"hubField\" type=\"button\" value=\"View Hub on Genome Browser\">\n");
printf("</div>\n"); // validateHubUrlRow div

printf("<div id=\"extraSettingsContainer\" class=\"addHubBar\">\n");
printf("<img id=\"advancedSettingsButton\" src=\"../images/add_sm.gif\">\n");
printf("Optional View Hub Settings");

char *measureTiming = cartCgiUsualString(cart, "measureTiming", NULL);
char *udcTimeout = cartCgiUsualString(cart, "udcTimeout", NULL);
boolean doMeasureTiming = isNotEmpty(measureTiming);
boolean doUdcTimeout = isNotEmpty(udcTimeout);

printf("<div id=\"extraSettingsList\" style=\"display: none\">\n");
printf("<ul style=\"list-style-type:none\">\n<li>\n");
// measureTiming first
printf("<input name=\"addMeasureTiming\" id=\"addMeasureTiming\" "
    "class=\"hubField\" type=\"checkbox\" %s>", doMeasureTiming ? "checked": "");
printf("<label for=\"addMeasureTiming\">Display load times</label>\n");

// and a tooltip explaining this checkbox
printf("<div class=\"tooltip\"> (?)\n");
printf("<span class=\"tooltiptext\">"
    "Checking this box shows the timing measurements at the bottom of the Genome Browser page. "
    "Useful for determining slowdowns to loading or drawing tracks."
    "</span>\n");
printf("</div></li>\n"); // tooltip div

printf("<li>\n");
// udcTimeout enable/disable
printf("<input name=\"disableUdcTimeout\" id=\"disableUdcTimeout\" "
    "class=\"hubField\" type=\"checkbox\" %s >", doUdcTimeout ? "checked" : "");
printf("<label for=\"disableUdcTimeout\">Enable hub refresh</label>\n");
// add a tooltip explaining these checkboxes
printf("<div class=\"tooltip\"> (?)\n");
printf("<span class=\"tooltiptext\">"
    "Checking this box changes the cache expiration time (default of 5 minutes) "
    "and allows the Genome Browser to reload Hub configuration and data files with each refresh."
    "</span>\n");
printf("</div></li>\n"); // tooltip div
printf("</ul>\n");
printf("</div>\n"); // extraSettingsList div
printf("</div>\n"); // extraSettingsContainer div

if (hubUrl != NULL && doHubValidate)
    doValidateNewHub(hubUrl);
else
    printf("<div id=\"hubDeveloperInstructions\">Enter URL to hub to check configuration settings "
        "or load hub </div> \n");
printf("</div>"); // hubDeveloper div

jsOnEventById("click", "hubValidateButton",
    "var validateText = document.getElementById('validateHubUrl');"
    "var udcTimeout = document.getElementById('disableUdcTimeout').checked === true;"
    "var doMeasureTiming = document.getElementById('addMeasureTiming').checked === true;"
    "validateText.value=$.trim(validateText.value);"
    "if(validateUrl($('#validateHubUrl').val())) { "
    " document.validateHubForm.elements['validateHubUrl'].value=validateText.value;"
    " document.validateHubForm.elements['" hgHubDoHubCheck "'].value='on';"
    " if (doMeasureTiming) { document.validateHubForm.elements['measureTiming'].value='1'; }"
    " else { document.validateHubForm.elements['measureTiming'].value=''}"
    " if (udcTimeout) { document.validateHubForm.elements['udcTimeout'].value='1'; }"
    " else { document.validateHubForm.elements['udcTimeout'].value=''}"
    " document.validateHubForm.submit(); return true; }"
    "else { return false; }"
    );
jsOnEventById("click", "hubLoadMaybeTiming",
    "var validateText = document.getElementById('validateHubUrl');"
    "var udcTimeout = document.getElementById('disableUdcTimeout').checked === true;"
    "var doMeasureTiming = document.getElementById('addMeasureTiming').checked === true;"
    "validateText.value=$.trim(validateText.value);"
    "if(validateUrl($('#validateHubUrl').val())) {"
    "   loc = \"../cgi-bin/hgTracks?hgHub_do_firstDb=on&hgHub_do_redirect=on&hgHubConnect.remakeTrackHub=on\" + \"&hubUrl=\" + validateText.value;"
    "   if (doMeasureTiming) { loc += \"&measureTiming=1\";} else { loc += \"&measureTiming=[]\"; }"
    "   if (udcTimeout) { loc += \"&udcTimeout=5\"; } else { loc += \"&udcTimeout=[]\"; }"
    "   window.location.href=loc; "
    "} else { return false; }"
    );
}

static void addPublicHubsToHubStatus(struct sqlConnection *conn, char *publicTable, char  *statusTable)
/* Add urls in the hubPublic table to the hubStatus table if they aren't there already */
{
char query[1024];
sqlSafef(query, sizeof(query),
        "select %s.hubUrl from %s left join %s on %s.hubUrl = %s.hubUrl where %s.hubUrl is NULL",
        publicTable, publicTable, statusTable, publicTable, statusTable, statusTable); 
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


char *modifyTermsForHubSearch(char *hubSearchTerms, bool isStrictSearch)
/* This won't exactly be pretty.  MySQL treats any sequence of alphanumerics and underscores
 * as a word, and single apostrophes are allowed as long as they don't come back-to-back.
 * Cut down to those characters, then add initial + (for requiring word) and * (for word
 * expansion) as appropriate. */
{
char *cloneTerms = cloneString(hubSearchTerms);
struct dyString *modifiedTerms = dyStringNew(0);
if (isNotEmpty(cloneTerms))
    {
    int i;
    for (i=0; i<strlen(cloneTerms); i++)
        {
        // allowed punctuation is underscore and apostrophe, and we'll do special handling for hyphen
        if (!isalnum(cloneTerms[i]) && cloneTerms[i] != '_' && cloneTerms[i] != '\'' &&
                cloneTerms[i] != '-')
            cloneTerms[i] = ' ';
        }
    char *splitTerms[1024];
    int termCount = chopByWhite(cloneTerms, splitTerms, sizeof(splitTerms));
    for (i=0; i<termCount; i++)
        {
        char *hyphenatedTerms[1024];
        int hyphenTerms = chopString(splitTerms[i], "-", hyphenatedTerms, sizeof(hyphenatedTerms));
        int j;
        for (j=0; j<hyphenTerms-1; j++)
            {
            dyStringPrintf(modifiedTerms, "+%s ", hyphenatedTerms[j]);
            }
        if (isStrictSearch)
            dyStringPrintf(modifiedTerms, "+%s ", hyphenatedTerms[j]);
        else
            {
            dyStringPrintf(modifiedTerms, "+%s* ", hyphenatedTerms[j]);
            }
        }
    }
return dyStringCannibalize(&modifiedTerms);
}


struct hubSearchText *getHubSearchResults(struct sqlConnection *conn, char *hubSearchTableName,
        char *hubSearchTerms, bool checkLongText, char *dbFilter, struct hash *hubLookup)
/* Find hubs, genomes, and tracks that match the provided search terms.
 * Return all hits that satisfy the (optional) supplied assembly filter.
 * if checkLongText is FALSE, skip searching within the long description text entries */
{
char *cleanSearchTerms = cloneString(hubSearchTerms);
if (isNotEmpty(cleanSearchTerms))
    tolowers(cleanSearchTerms);
bool isStrictSearch = FALSE;
char *modifiedSearchTerms = modifyTermsForHubSearch(cleanSearchTerms, isStrictSearch);
struct hubSearchText *hubSearchResultsList = NULL;
struct dyString *query = dyStringNew(100);
char *noLongText = NULL;

if (!checkLongText)
    noLongText = cloneString("textLength = 'Short' and");
else
    noLongText = cloneString("");

sqlDyStringPrintf(query, "select * from %s where %s match(text) against ('%s' in boolean mode)"
        " order by match(text) against ('%s' in boolean mode)",
        hubSearchTableName, noLongText, modifiedSearchTerms, modifiedSearchTerms);

struct sqlResult *sr = sqlGetResult(conn, dyStringContents(query));
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hubSearchText *hst = hubSearchTextLoadWithNullGiveContext(row, cleanSearchTerms);
    // Skip rows where the long text matched the more lax MySQL search (punctuation just
    // splits terms into two words, so "rna-seq" finds "rna" and "seq" separately, but
    // not the more strict rules used to find context for the search terms.
    if ((hst->textLength == hubSearchTextLong) && isEmpty(hst->text))
        continue;
    char *hubUrl = hst->hubUrl;
    struct hubEntry *hubInfo = hashFindVal(hubLookup, hubUrl);
    if (hubInfo == NULL)
        continue; // Search table evidently includes a hub that's not on this server.  Skip it.
    char *db = cloneString(hst->db);
    tolowers(db);
    if (isNotEmpty(dbFilter))
        {
        if (isNotEmpty(db))
            {
            if (stringIn(dbFilter, db) == NULL)
                continue;
            }
        else
            {
            // no db in the hubSearchText means this is a top-level hub hit.
            // filter by the db list associated with the hub instead
            char *dbList = cloneString(hubInfo->dbList);
            tolowers(dbList);
            if (stringIn(dbFilter, dbList) == NULL)
                continue;
            }
        }
    // Add hst to the list to be returned
    slAddHead(&hubSearchResultsList, hst);
    }
slReverse(&hubSearchResultsList);
return hubSearchResultsList;
}


void printSearchAndFilterBoxes(int searchEnabled, char *hubSearchTerms, char *dbFilter)
/* Create the text boxes for search and database filtering along with the required
 * javscript */
{
char event[4096];
if (searchEnabled)
    {
    safef(event, sizeof(event), 
            "document.searchHubForm.elements['hubSearchTerms'].value=$('#hubSearchTerms').val();"
            "document.searchHubForm.elements['hubDbFilter'].value=$('#hubDbFilter').val();"
            "document.searchHubForm.submit();return true;");
    printf("Enter search terms to find in public track hub description pages:<BR>"
            "<input name=\"hubSearchTerms\" id=\"hubSearchTerms\" class=\"hubField\" "
            "type=\"text\" size=\"65\" value=\"%s\"> \n",
            hubSearchTerms!=NULL?hubSearchTerms:"");
    printf("<br>\n");
    }
else
    {
    safef(event, sizeof(event), 
            "document.searchHubForm.elements['hubDbFilter'].value=$('#hubDbFilter').val();"
            "document.searchHubForm.submit();return true;");
    }

printf("Filter hubs by assembly: "
        "<input name=\"%s\" id=\"hubDbFilter\" class=\"hubField\" "
        "type=\"text\" size=\"10\" value=\"%s\"> \n"
        "<input name=\"hubSearchButton\" id='hubSearchButton' "
        "class=\"hubField\" type=\"button\" value=\"Search Public Hubs\">\n",
        hgHubDbFilter, dbFilter!=NULL?dbFilter:"");
jsOnEventById("click", "hubSearchButton", event);
puts("<br><br>\n");
}


void printSearchTerms(char *hubSearchTerms)
/* Write out a reminder about the current search terms and a note about
 * how to navigate detailed search results */
{
printf("Displayed list <B>restricted by search terms:</B> %s\n", hubSearchTerms);
puts("<input name=\"hubDeleteSearchButton\" id='hubDeleteSearchButton' "
        "class=\"hubField\" type=\"button\" value=\"Show All Hubs\">\n");

jsOnEventById("click", "hubDeleteSearchButton",
        "document.searchHubForm.elements['hubSearchTerms'].value='';"
        "document.searchHubForm.elements['hubDbFilter'].value='';"
        "document.searchHubForm.submit();return true;");
puts("<BR><BR>\n");
printf("When exploring the detailed search results for a hub, you may right-click "
        "on an assembly or track line to open it in a new window.\n");
puts("<BR><BR>\n");
}


void printHubListHeader()
/* Write out the header for a list of hubs in its own table */
{
puts("<I>Clicking Connect redirects to the gateway page of the selected hub's default assembly.</I><BR>");
printf("<table id=\"publicHubsTable\" class=\"hubList\"> "
        "<thead><tr> "
            "<th>Display</th> "
            "<th>Hub Name</th> "
            "<th>Description</th> "
            "<th>Assemblies</th> "
        "</tr></thead></table>\n");
}


struct hash *buildPublicLookupHash(struct sqlConnection *conn, char *publicTable, char *statusTable,
        struct hash **pHash)
/* Return a hash linking hub URLs to struct hubEntries.  Also make pHash point to a hash that just stores
 * the names of the public hubs (for use later when determining if hubs were added by the user) */
{
struct hash *hubLookup = newHash(5);
struct hash *publicLookup = newHash(5);
char query[512];
bool hasDescription = sqlColumnExists(conn, publicTable, "descriptionUrl");
if (hasDescription)
    sqlSafef(query, sizeof(query), "select p.hubUrl,p.shortLabel,p.longLabel,p.dbList,"
            "s.errorMessage,s.id,p.descriptionUrl from %s p,%s s where p.hubUrl = s.hubUrl", 
	    publicTable, statusTable); 
else
    sqlSafef(query, sizeof(query), "select p.hubUrl,p.shortLabel,p.longLabel,p.dbList,"
            "s.errorMessage,s.id from %s p,%s s where p.hubUrl = s.hubUrl", 
	    publicTable, statusTable); 

struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hubEntry *hubInfo = hubEntryTextLoad(row, hasDescription);
    hubInfo->tableHasDescriptionField = hasDescription;
    hashAddUnique(hubLookup, hubInfo->hubUrl, hubInfo);
    hashStore(publicLookup, hubInfo->hubUrl);
    }
sqlFreeResult(&sr);
*pHash = publicLookup;
return hubLookup;
}


void outputPublicTableRow(struct hubEntry *hubInfo, int count)
/* Prints out a table row with basic information about a hub and a button
 * to connect to that hub */
{
int id = hubInfo->id;
char jsId[256];
struct slName *dbListNames = slNameListFromComma(hubInfo->dbList);
printf("<tr>\n");
if (id != 0)
    {
    ourCellStart();
    char hubName[32];
    safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, id);
    if (cartUsualBoolean(cart, hubName, FALSE))
        {
        safef(jsId, sizeof jsId, "hubDisconnectButtonP%d", count);
        printf("<input name=\"hubDisconnectButton\" id='%s' "
            "class=\"hubDisconnectButton\" type=\"button\" value=\"Disconnect\">\n", jsId);
        jsOnEventByIdF("click", jsId, 
            "document.disconnectHubForm.elements['hubId'].value= '%d';"
            "document.disconnectHubForm.submit();return true;", id);
        }
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

        safef(jsId, sizeof jsId, "hubConnectButton%d", count);
        printf("<input name=\"hubConnectButton\" id='%s' "
            "class=\"hubButton\" type=\"button\" value=\"Connect\">\n", jsId);
        jsOnEventByIdF("click", jsId, 
            "document.connectHubForm.elements['hubUrl'].value= '%s';"
            "document.connectHubForm.elements['db'].value= '%s';"
            "document.connectHubForm.submit();return true;", hubInfo->hubUrl,name);
        }

    ourCellEnd();
    }
else
    errAbort("cannot get id for hub with url %s\n", hubInfo->hubUrl);

ourPrintCellLink(hubInfo->shortLabel, hubInfo->hubUrl);

boolean hubHasNoError = isEmpty(hubInfo->errorMessage);
if (hubHasNoError)
    {
    if (hubInfo->tableHasDescriptionField && !isEmpty(hubInfo->descriptionUrl))
        ourPrintCellLink(hubInfo->longLabel, hubInfo->descriptionUrl);
    else
        ourPrintCell(hubInfo->longLabel);
    }
else
    {
    ourCellStart();
    printf("<span class=\"hubError\">ERROR: %s </span>"
        "<a href=\"../goldenPath/help/hgTrackHubHelp.html#Debug\">Debug Help</a>", 
        hubInfo->errorMessage);
    safef(jsId, sizeof jsId, "hubClearButton%d", count);
    printf(
    "<input name=\"hubClearButton\" id='%s' "
            "class=\"hubButton\" type=\"button\" value=\"Retry Hub\">"
            , jsId);
    jsOnEventByIdF("click", jsId, 
        "document.resetHubForm.elements['hubCheckUrl'].value='%s';"
        "document.resetHubForm.submit();return true;", hubInfo->hubUrl);
    ourCellEnd();
    }

printGenomeList(hubInfo->hubUrl, dbListNames, count, hubHasNoError); 
printf("</tr>\n");
}

struct trackHub *fetchTrackHub(struct hubEntry *hubInfo)
/* Fetch the hub structure for a public hub, trapping the error
 * if the hub cannot be reached */
{
struct errCatch *errCatch = errCatchNew();
struct trackHub *hub = NULL;
if (errCatchStart(errCatch))
    {
    char hubName[4096];
    safef(hubName, sizeof(hubName), "hub_%d", hubInfo->id);
    hub = trackHubOpen(hubInfo->hubUrl, hubName);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    fprintf(stderr, "%s\n", errCatch->message->string);
    }
errCatchFree(&errCatch);
return hub;
}

char *getPositionStringForDb(struct trackHubGenome *genome)
{
char positionVar[1024];
safef(positionVar, sizeof(positionVar), "position.%s", genome->name);
char *position = cartOptionalString(cart, positionVar);
if (position == NULL)
    {
    struct dyString *tmp = dyStringCreate("position=");
    if (genome->defaultPos != NULL)
        dyStringAppend(tmp, genome->defaultPos);
    else
        dyStringAppend(tmp, hDefaultPos(genome->name)); // memory leak from hDefaultPos return value
    position = dyStringCannibalize(&tmp);
    }
return position;
}


struct tdbOutputStructure *hstToTdbOutput(struct hubSearchText *hst, struct genomeOutputStructure *genomeOut, struct trackHub *hub)
/* Convert a hubSearchText entry to a (list of) tdbOutputStructure(s) */
{
struct tdbOutputStructure *tdbOut = hashFindVal(genomeOut->tdbOutHash, hst->track);
if (tdbOut == NULL)
    {
    genomeOut->trackCount++;
    AllocVar(tdbOut);
    tdbOut->shortLabel = dyStringNew(0);
    tdbOut->metaTags = dyStringNew(0);
    tdbOut->descriptionMatch = dyStringNew(0);
    tdbOut->configUrl = dyStringNew(0);
    dyStringPrintf(tdbOut->shortLabel, "%s", hst->label);
    char *hubId = hubNameFromUrl(hub->url);
    if (isNotEmpty(hst->parents))
        {
        // hst->parents is a comma-sep list like "track1","track1Label","track2","track2Label"
        int i;
        int parentCount;
        int parentTypesCount;
        char *parentTrack = NULL;
        char *parentLabel = NULL;
        char *parentTrackLabels[16]; // 2 slots per parent, can tracks nest more than 8 deep?
        char *parentTypes[16]; // the types of parents, "comp", "super", "view", "other" for each track in parentTrackLabels
        struct tdbOutputStructure *parentTdbOut = NULL;
        struct tdbOutputStructure *savedParent = NULL;

        parentCount = chopByCharRespectDoubleQuotes(cloneString(hst->parents), ',', parentTrackLabels, sizeof(parentTrackLabels));
        parentTypesCount = chopCommas(cloneString(hst->parentTypes), parentTypes);
        if (parentCount == 0 || parentCount % 2 != 0)
            {
            errAbort("error parsing hubSearchText->parents for %s.%s in hub: '%s'",
                genomeOut->genomeName, hst->track, hub->url);
            }
        if (parentTypesCount != (parentCount / 2))
            {
            errAbort("error parsing hubSearchText->parentTypes: '%s' for %s.%s in hub: '%s'",
                hst->parentTypes, genomeOut->genomeName, hst->track, hub->url);
            }

        boolean foundParent = FALSE;
        boolean doAddSaveParent = FALSE;
        for (i = 0; i < parentCount; i += 2)
            {
            parentTrack = stripEnclosingDoubleQuotes(cloneString(parentTrackLabels[i]));
            parentLabel = stripEnclosingDoubleQuotes(cloneString(parentTrackLabels[i+1]));
            // wait until the first valid trackui page for the track hit
            if (isEmpty(dyStringContents(tdbOut->configUrl)) && sameString(parentTypes[i/2], "comp"))
                {
                dyStringPrintf(tdbOut->configUrl, "../cgi-bin/hgTrackUi?hubUrl=%s&db=%s&g=%s_%s&hgsid=%s&%s",
                    hub->url, genomeOut->genomeName, hubId, parentTrack, cartSessionId(cart),
                    genomeOut->positionString);
                }
            else if (isEmpty(dyStringContents(tdbOut->configUrl)) && sameString(parentTypes[i/2], "super"))
                {
                dyStringPrintf(tdbOut->configUrl, "../cgi-bin/hgTrackUi?hubUrl=%s&db=%s&g=%s_%s&hgsid=%s&%s",
                    hub->url, genomeOut->genomeName, hubId, hst->track, cartSessionId(cart),
                    genomeOut->positionString);
                }
            parentTdbOut = hashFindVal(genomeOut->tdbOutHash, parentTrack);
            if (parentTdbOut == NULL)
                {
                AllocVar(parentTdbOut);
                parentTdbOut->shortLabel = dyStringNew(0);
                parentTdbOut->metaTags = dyStringNew(0);
                parentTdbOut->descriptionMatch = dyStringNew(0);
                parentTdbOut->configUrl = dyStringNew(0);
                // views will be in the parent list, but the &g parameter to trackUi should be the view's parent
                if (sameString(parentTypes[(i/2)], "view"))
                    dyStringPrintf(parentTdbOut->configUrl,
                        "../cgi-bin/hgTrackUi?hubUrl=%s&db=%s&g=%s_%s&hgsid=%s&%s",
                        hub->url, genomeOut->genomeName, hubId, stripEnclosingDoubleQuotes(parentTrackLabels[(i/2)+2]), cartSessionId(cart), genomeOut->positionString);
                else // everything else has the correct &g param
                    dyStringPrintf(parentTdbOut->configUrl,
                        "../cgi-bin/hgTrackUi?hubUrl=%s&db=%s&g=%s_%s&hgsid=%s&%s",
                        hub->url, genomeOut->genomeName, hubId, parentTrack, cartSessionId(cart), genomeOut->positionString);
                dyStringPrintf(parentTdbOut->shortLabel, "%s", parentLabel);
                parentTdbOut->childCount += 1;
                if (savedParent)
                    slAddHead(&(parentTdbOut->children), savedParent);
                else
                    slAddHead(&(parentTdbOut->children), tdbOut);
                savedParent = parentTdbOut;
                doAddSaveParent = TRUE;
                hashAdd(genomeOut->tdbOutHash, parentTrack, parentTdbOut);
                }
            else
                {
                foundParent = TRUE; // don't add this track to the genomeOut->tracks hash again
                if (savedParent && doAddSaveParent)
                    {
                    parentTdbOut->childCount += 1;
                    slAddHead(&(parentTdbOut->children), savedParent);
                    }
                else if (!savedParent)
                    {
                    parentTdbOut->childCount += 1;
                    slAddHead(&(parentTdbOut->children), tdbOut);
                    }
                savedParent = parentTdbOut;
                doAddSaveParent = FALSE;
                }
            }
        if (!foundParent)
            {
            slAddHead(&(genomeOut->tracks), parentTdbOut);
            }
        }
    else
        {
        dyStringPrintf(tdbOut->configUrl, "../cgi-bin/hgTrackUi?hubUrl=%s&db=%s&g=%s_%s&hgsid=%s&%s",
            hub->url, genomeOut->genomeName, hubId, hst->track, cartSessionId(cart),
            genomeOut->positionString);
        slAddHead(&(genomeOut->tracks), tdbOut);
        }
    hashAdd(genomeOut->tdbOutHash, hst->track, tdbOut);
    }
return tdbOut;
}

struct hubOutputStructure *buildHubSearchOutputStructure(struct trackHub *hub,
        struct hubSearchText *searchResults)
/* Build a structure that contains the data for writing out the hub search results for this hub */
{
struct hash *missingGenomes = hashNew(0);
struct hubOutputStructure *hubOut = NULL;
AllocVar(hubOut);
hubOut->metaTags = dyStringNew(0);
hubOut->descriptionMatch = dyStringNew(0);
hubOut->genomeOutHash = newHash(5);


struct hubSearchText *hst = NULL;
for (hst = searchResults; hst != NULL; hst = hst->next)
    {
    if (isEmpty(hst->db))
        {
        // must be a hit to the hub itself, not an assembly or track within it
        if (hst->textLength == hubSearchTextLong)
            {
            dyStringPrintf(hubOut->descriptionMatch, "%s", hst->text);
            }
        else if (hst->textLength == hubSearchTextMeta)
            {
            if (isNotEmpty(dyStringContents(hubOut->metaTags)))
                dyStringPrintf(hubOut->metaTags, ", %s", hst->text);
            else
                dyStringPrintf(hubOut->metaTags, "%s", hst->text);
            }
        continue;
        }

    char *db = cloneString(hst->db);
    if (hashLookup(missingGenomes, db) != NULL)
        continue;
    struct trackHubGenome *genome = hashFindVal(hub->genomeHash, db);
    if (genome == NULL)
        {
        // assembly hub genomes are stored with a prefix; try that
        char withHubName[4096];
        safef(withHubName, sizeof(withHubName), "%s_%s", hub->name, db);
        genome = hashFindVal(hub->genomeHash, withHubName);
        if (genome == NULL)
            {
            hashStoreName(missingGenomes, db);
            warn("Error: Unable to find info for matching assembly '%s'.  Skipping ...\n", withHubName);
            continue;
            }
        }
    struct genomeOutputStructure *genomeOut = hashFindVal(hubOut->genomeOutHash, db);
    if (genomeOut == NULL)
        {
        AllocVar(genomeOut);
        genomeOut->tdbOutHash = newHash(5);
        genomeOut->metaTags = dyStringNew(0);
        genomeOut->descriptionMatch = dyStringNew(0);
        genomeOut->shortLabel = dyStringNew(0);
        genomeOut->assemblyLink = dyStringNew(0);
        genomeOut->positionString = getPositionStringForDb(genome);
        dyStringPrintf(genomeOut->assemblyLink, "../cgi-bin/hgTracks?hubUrl=%s&db=%s&hgsid=%s&%s",
                hub->url, genome->name, cartSessionId(cart), genomeOut->positionString);
        char *name = trackHubSkipHubName(genome->name);
        if (isNotEmpty(genome->description))
            dyStringPrintf(genomeOut->shortLabel, "%s (%s)", genome->description, name);
        else if (isNotEmpty(genome->organism))
            dyStringPrintf(genomeOut->shortLabel, "%s %s", genome->organism, name);
        else
            dyStringPrintf(genomeOut->shortLabel, "%s", name);
        genomeOut->genomeName = cloneString(genome->name);
        hashAdd(hubOut->genomeOutHash, db, genomeOut);
        slAddTail(&(hubOut->genomes), genomeOut);
        hubOut->genomeCount++;
        }
    if (isEmpty(hst->track))
        {
        if (hst->textLength == hubSearchTextLong) // Genome description match
            dyStringPrintf(genomeOut->descriptionMatch, "%s", hst->text);
        else if (hst->textLength == hubSearchTextMeta)
            {
            if (isNotEmpty(dyStringContents(genomeOut->metaTags)))
                dyStringPrintf(genomeOut->metaTags, ", %s", hst->text);
            else
                dyStringPrintf(genomeOut->metaTags, "%s", hst->text);
            }
        }

    if (isNotEmpty(hst->track))
        {
        // Time to add a track! (or add info to one, maybe)
        struct tdbOutputStructure *tdbOut = hstToTdbOutput(hst, genomeOut, hub);
        if (tdbOut != NULL)
            {
            if (hst->textLength == hubSearchTextLong)
                dyStringPrintf(tdbOut->descriptionMatch, "%s", hst->text);
            else if (hst->textLength == hubSearchTextMeta)
                {
                if (isNotEmpty(dyStringContents(tdbOut->metaTags)))
                    dyStringPrintf(tdbOut->metaTags, ", %s", hst->text);
                else
                    dyStringPrintf(tdbOut->metaTags, "%s", hst->text);
                }
            }
        }
    }
return hubOut;
}

static char *tdbOutputStructureLabelToId(struct tdbOutputStructure *tdbOut)
/* Make an array name out of a tdbOutputStruct */
{
struct dyString *id = dyStringNew(0);
dyStringPrintf(id, "%s", htmlEncode(dyStringContents(tdbOut->shortLabel)));
if (tdbOut->childCount > 0)
    {
    dyStringPrintf(id, " (%d subtrack%s)", tdbOut->childCount,
        tdbOut->childCount == 1 ? "" : "s");
    }
return dyStringCannibalize(&id);
}

static void printTdbOutputStructureToDyString(struct tdbOutputStructure *tdbOut, struct dyString *dy, char *arrayName)
/* Print a tdbOutputStructure to a dyString, recursive for subtracks. */
{
dyStringPrintf(dy, "trackData['%s'] = [", arrayName);

if (tdbOut->childCount > 0)
    {
    struct dyString *subtrackDy = dyStringNew(0);
    struct tdbOutputStructure *child = tdbOut->children;
    while (child != NULL)
        {
        char *childId = tdbOutputStructureLabelToId(child);
        dyStringPrintf(dy, "\n\t{\n\tid: '%s',\n\tparent: '%s',\n\t"
            "li_attr: {nodetype:'track', configlink:'%s'},\n\ttext: \'%s ",
            childId, arrayName, dyStringContents(child->configUrl), childId);
        if (isNotEmpty(dyStringContents(child->metaTags)))
            {
            dyStringPrintf(dy, "<br><span class=\\'descriptionMatch\\'><em>Metadata: %s</em></span>",
                htmlEncode(dyStringContents(child->metaTags)));
            }
        if (isNotEmpty(dyStringContents(child->descriptionMatch)))
            {
            dyStringPrintf(dy, "<br><span class=\\'descriptionMatch\\'><em>Description: %s</em></span>",
                htmlEncode(dyStringContents(child->descriptionMatch)));
            }
        dyStringPrintf(dy, "\'");
        if (child->childCount > 0)
            {
            dyStringPrintf(dy, ",\n\tchildren: true");
            printTdbOutputStructureToDyString(child, subtrackDy, childId);
            }
        dyStringPrintf(dy, "\n\t},");
        child = child->next;
        }
    dyStringPrintf(dy, "];\n");
    if (isNotEmpty(dyStringContents(subtrackDy)))
        dyStringPrintf(dy, "%s", subtrackDy->string);
    }
else
    dyStringPrintf(dy, "];\n");
}

void printGenomeOutputStructureToDyString(struct genomeOutputStructure *genomeOut, struct dyString *dy, char *genomeNameId)
/* Print a genomeOutputStructure to a dyString. The structure here is:
 * trackData[genome] = [{track 1 obj}, {track2 obj}, {track3 obj}, ... ]
 * trackData[track1] = [{search hit text}, {subtrack1 obj}, {subtrack2 obj}, ... ]
 *
 * if track1, track2, track3 are container tracks, then the recursive function
 * tdbOutputStructureToDystring creates the above trackData[track1] = [{}] for
 * each of the containers, otherwise a single child of the genome is sufficient */
{
struct tdbOutputStructure *tdbOut = NULL;
static  struct dyString *tdbArrayDy = NULL; // the dyString for all of the tdb objects
static struct dyString *idString = NULL; // the special id of this track
if (tdbArrayDy == NULL)
    tdbArrayDy = dyStringNew(0);
if (idString == NULL)
    idString = dyStringNew(0);

dyStringPrintf(dy, "trackData['%s'] = [", genomeNameId);
if (genomeOut->tracks != NULL)
    {
    tdbOut = genomeOut->tracks;
    slReverse(&tdbOut);
    while (tdbOut != NULL)
        {
        dyStringPrintf(idString, "%s", tdbOutputStructureLabelToId(tdbOut));
        dyStringPrintf(dy, "\n\t{\n\t'id': '%s',\n\t'parent': '%s',\n\t"
            "'li_attr': {'nodetype':'track', configlink: '%s'},\n\t'text': \'%s ",
            idString->string, genomeNameId, dyStringContents(tdbOut->configUrl), idString->string);
        if (isNotEmpty(dyStringContents(tdbOut->metaTags)))
            {
            dyStringPrintf(dy, "<br><span class=\\'descriptionMatch\\'><em>Metadata: %s</em></span>",
                htmlEncode(dyStringContents(tdbOut->metaTags)));
            }
        if (isNotEmpty(dyStringContents(tdbOut->descriptionMatch)))
            {
            dyStringPrintf(dy, "<br><span class=\\'descriptionMatch\\'><em>Description: %s</em></span>",
                htmlEncode(dyStringContents(tdbOut->descriptionMatch)));
            }
        dyStringPrintf(dy, "\'");

        // above we took care of both non-heirarchical tracks and the top-level containers,
        // now do container children, which also takes care of any deeper heirarchies
        if (tdbOut->childCount > 0)
            dyStringPrintf(dy, ",\n\t'children': true");
        dyStringPrintf(dy, "\n\t},\n");

        if (tdbOut->childCount > 0)
            printTdbOutputStructureToDyString(tdbOut, tdbArrayDy, idString->string);
        tdbOut = tdbOut->next;
        dyStringClear(idString);
        }
    }
dyStringPrintf(dy, "];\n"); // close off genome node
dyStringPrintf(dy, "%s\n", tdbArrayDy->string);
dyStringClear(tdbArrayDy);
}

void printHubOutputStructure(struct hubOutputStructure *hubOut, struct hubEntry *hubInfo)
/* Convert a hubOutputStructure to a jstree-readable string. This function forms the root
 * node for each hub search tree, whose children are the hub description match and each individual
 * genome node. A simplified structure is:
 * trackData['#_hubId'] = [{id:'descriptionMatch',...},{id:'assembly1',...},...]
 *
 * The id's become new "trackData[id]" entries with their own arrays later if they have
 * sub-trees (via printGenomeOutputStructureToDyString() and printTdbOutputStructureToDyString(). */
{
struct dyString *dy = dyStringNew(0);
// The leading '#' tells the javascript this is a 'root' node
dyStringPrintf(dy, "trackData['#_%d'] = [", hubInfo->id);
if (isNotEmpty(dyStringContents(hubOut->descriptionMatch)))
    {
    dyStringPrintf(dy, "{'id':'%d_descriptionMatchText','parent':'#_%d',"
        "'state':{'opened': true},'text': 'Hub Description: "
        "<span class=\"descriptionMatch\"><em>%s</em></span>'},",
        hubInfo->id, hubInfo->id, htmlEncode(dyStringContents(hubOut->descriptionMatch)));
    }
struct genomeOutputStructure *genomeOut = hubOut->genomes;
struct dyString *genomeDy = dyStringNew(0);
if (genomeOut != NULL)
    {
    dyStringPrintf(dy, "{'id':'%d_assemblies', 'text':'%d Matching Assembl%s', 'parent':'#_%d', "
        "'children':true, 'li_attr': {'state':{'opened': 'false'}}}];\n",
        hubInfo->id, hubOut->genomeCount, hubOut->genomeCount == 1 ? "y" : "ies", hubInfo->id);
    dyStringPrintf(dy, "trackData['%d_assemblies'] = [", hubInfo->id);

    while (genomeOut != NULL)
        {
        char *assemblyName = htmlEncode(dyStringContents(genomeOut->shortLabel));
        char genomeNameId[512];
        safef(genomeNameId, sizeof(genomeNameId), "%d_%s", hubInfo->id, assemblyName);
        dyStringPrintf(dy, "{'id': '%s', 'parent': '%d_assemblies', 'children': true, "
            "'li_attr': {'assemblylink': '%s','nodetype': 'assembly'},"
            "'text': \"%s",
            genomeNameId, hubInfo->id, dyStringContents(genomeOut->assemblyLink), assemblyName);
        if (genomeOut->trackCount > 0)
            {
            dyStringPrintf(dy, " (%d track%s) ", genomeOut->trackCount,
                genomeOut->trackCount == 1 ? "" : "s");
            }
        if (isNotEmpty(dyStringContents(genomeOut->metaTags)))
            {
            dyStringPrintf(dy, "<br><span class='descriptionMatch'><em>%s</em></span>",
                htmlEncode(dyStringContents(genomeOut->metaTags)));
            }
        if (isNotEmpty(dyStringContents(genomeOut->descriptionMatch)))
            {
            dyStringPrintf(dy, "<br><em>Assembly Description:</em> %s",
                htmlEncode(dyStringContents(genomeOut->descriptionMatch)));
            }
        dyStringPrintf(dy, "\"},");
        printGenomeOutputStructureToDyString(genomeOut, genomeDy, genomeNameId);
        genomeOut = genomeOut->next;
        }
    }
dyStringPrintf(dy, "];\n");
dyStringPrintf(dy, "%s", genomeDy->string);
jsInline(dy->string);
dyStringClear(dy);
}

static void printOutputForHub(struct hubEntry *hubInfo, struct hubSearchText *hubSearchResult, int count)
/* Given a hub's info and a structure listing the search hits within the hub, first print
 * a basic line of hub information with a "connect" button.  Then, if the search results
 * are non-NULL, write out information about the genomes and tracks from the search hits that
 * match the db filter.
 * If there are no search results to print, the basic hub lines are combined into a single HTML table
 * that is defined outside this function.
 * Otherwise, each hub line is printed in its own table followed by a <ul> containing details
 * about the search results. */
{
if (hubSearchResult != NULL)
    printf("<table class='hubList'><tbody>\n");
outputPublicTableRow(hubInfo, count);
if (hubSearchResult != NULL)
    {
    printf("</tbody></table>\n");
    struct trackHub *hub = fetchTrackHub(hubInfo);
    if (hub != NULL)
        {
        printf("<div class=\"hubTdbTree\">\n");
        printf("<div id='tracks%d'></div>", hubInfo->id); // div for the jstree for this hub's search result(s)
        printf("</div>\n");
        struct hubOutputStructure *hubOut = buildHubSearchOutputStructure(hub, hubSearchResult);
        if (dyStringIsEmpty(hubOut->descriptionMatch) && (hubOut->genomes == NULL))
            return; // no detailed search results; hit must have been to hub short label or something
        printHubOutputStructure(hubOut, hubInfo);
        }
    }
}

int hubEntryCmp(const void *va, const void *vb)
/* Compare to sort based on shortLabel */
{
const struct hubEntry *a = *((struct hubEntry **)va);
const struct hubEntry *b = *((struct hubEntry **)vb);

return strcasecmp(a->shortLabel, b->shortLabel);
}


void printHubList(struct slName *hubsToPrint, struct hash *hubLookup, struct hash *searchResultHash)
/* Print out a list of hubs, possibly along with search hits to those hubs.
 * hubLookup takes hub URLs to struct hubEntry
 * searchResultHash takes hub URLs to struct hubSearchText * (list of hits on that hub)
 */
{
int count = 0;
struct hubEntry *hubList = NULL;
struct hubEntry *hubInfo;
long slTime;
long printOutputForHubTime;
boolean measureTiming = cartUsualBoolean(cart, "measureTiming", FALSE);
if (hubsToPrint != NULL)
    {
    printHubListHeader();

    if (searchResultHash == NULL) // if not displaying search results, join the hub <tr>s into one table
        printf("<table class='hubList'><tbody>\n");
    struct slName *thisHubName = NULL;
    for (thisHubName = hubsToPrint; thisHubName != NULL; thisHubName = thisHubName->next)
        {
        hubInfo = (struct hubEntry *) hashFindVal(hubLookup, thisHubName->name);
        if (hubInfo == NULL)
            {
            /* This shouldn't happen often, but maybe the search hits list was built from an outdated
             * search text file that includes hubs for which no info is available.
             * Skip this hub. */
            continue;
            }
        slAddHead(&hubList, hubInfo);
        }
    slSort(&hubList, hubEntryCmp);
    slTime = clock1000();

    for (hubInfo = hubList; hubInfo != NULL; hubInfo = hubInfo->next)
        {
        struct hubSearchText *searchResult = NULL;
        if (searchResultHash != NULL)
            {
            searchResult = (struct hubSearchText *) hashMustFindVal(searchResultHash, hubInfo->hubUrl);
            }
        printOutputForHub(hubInfo, searchResult, count);
        count++;
        }
    printOutputForHubTime = clock1000();
    if (measureTiming)
        printf("hgHubConnect: printOutputForHubTime before js execution: %lu millis<BR>\n", printOutputForHubTime - slTime);
    if (searchResultHash == NULL)
        printf("</tbody></table>\n");
    }
if (hubsToPrint != NULL)
    {
    /* Write out the list of hubs in a single table inside a div that will be hidden by
     * javascript.  This table is used (before being hidden) to set common column widths for
     * the individual hub tables when they're split by detailed search results. */
    printf("<div id='hideThisDiv'>\n");
    printf("<table class='hubList' id='hideThisTable'><tbody>\n");
    for (hubInfo = hubList; hubInfo != NULL; hubInfo = hubInfo->next)
        {
        printOutputForHub(hubInfo, NULL, count);
        count++;
        }
    printf("</tbody></table>\n");
    printf("</div>\n");
    }
jsInline(
        "function lineUpCols()\n"
        "    {\n"
        "    var tableList = $('table.hubList');\n"
        "    if (tableList.length == 0)\n"
        "        return;\n"
        "    var colWidths = new Array();\n"
        "    var combinedTrackTable = $('#hideThisTable');\n"
        "    for (i=0; i<combinedTrackTable[0].rows[0].cells.length; i++)\n"
        "        colWidths[i] = combinedTrackTable[0].rows[0].cells[i].clientWidth;\n"
        "    $('#hideThisDiv')[0].style.display = 'none';\n"
        "    for(i=0; i<tableList.length; i++)\n"
        "        {\n"
        "        for(j=0; j<tableList[i].rows[0].cells.length; j++)\n"
        "            tableList[i].rows[0].cells[j].style.width = colWidths[j]+'px';\n"
        "        }\n"
        "    }\n"
        "window.onload = lineUpCols();\n"
        );
if (searchResultHash != NULL)
    jsInline("hubSearchTree.init(true);\n");
}


static bool outputPublicTable(struct sqlConnection *conn, char *publicTable, char *statusTable,
        struct hash **pHash)
/* Put up the list of public hubs and other controls for the page. */
{
char *hubSearchTerms = cartOptionalString(cart, hgHubSearchTerms);
char *dbFilter = cartOptionalString(cart, hgHubDbFilter);
char *lcDbFilter = cloneString(dbFilter);
if (isNotEmpty(lcDbFilter))
    tolowers(lcDbFilter);

// make sure all the public hubs are in the hubStatus table.
addPublicHubsToHubStatus(conn, publicTable, statusTable);

// build full public hub lookup hash, taking each URL to struct hubEntry * for that hub
struct hash *hubLookup = buildPublicLookupHash(conn, publicTable, statusTable, pHash);

printf("<div id=\"publicHubs\" class=\"hubList\"> \n");

char *hubSearchTableName = cfgOptionDefault("hubSearchTextTable", "hubSearchText");
int searchEnabled = sqlTableExists(conn, hubSearchTableName);

printSearchAndFilterBoxes(searchEnabled, hubSearchTerms, dbFilter);

struct hash *searchResultHash = NULL;
struct slName *hubsToPrint = NULL;
if (searchEnabled && !isEmpty(hubSearchTerms))
    {
    printSearchTerms(hubSearchTerms);
    // Forcing checkDescriptions to TRUE right now, but we might want to add this as a
    // checkbox option for users in the near future.
    bool checkDescriptions = TRUE;
    struct hubSearchText *hubSearchResults = getHubSearchResults(conn, hubSearchTableName,
            hubSearchTerms, checkDescriptions, lcDbFilter, hubLookup);
    searchResultHash = newHash(5);
    struct hubSearchText *hst = hubSearchResults;
    while (hst != NULL)
        {
        struct hubSearchText *nextHst = hst->next;
        hst->next = NULL;
        struct hashEl *hubHashEnt = hashLookup(searchResultHash, hst->hubUrl);
        if (hubHashEnt == NULL)
            {
            slNameAddHead(&hubsToPrint, hst->hubUrl);
            hashAdd(searchResultHash, hst->hubUrl, hst);
            }
        else
            slAddHead(&(hubHashEnt->val), hst);
        hst = nextHst;
        }
    struct hashEl *hel;
    struct hashCookie cookie = hashFirst(searchResultHash);
    while ((hel = hashNext(&cookie)) != NULL)
        slReverse(&(hel->val));
    }
else
    {
    // There is no active search, so just add all hubs to the list
    struct hashEl *hel;
    struct hashEl *helList;
    helList = hashElListHash(hubLookup);
    for (hel = helList; hel != NULL; hel = hel->next)
        {
        if (isNotEmpty(lcDbFilter))
            {
            struct hubEntry *hubEnt = (struct hubEntry *) hel->val;
            char *lcDbList = cloneString(hubEnt->dbList);
            if (isNotEmpty(lcDbList))
                tolowers(lcDbList);
            if ((lcDbList == NULL) || (stringIn(lcDbFilter, lcDbList) == NULL))
                continue;
            }
        slNameAddHead(&hubsToPrint, hel->name);
        }
    }
slReverse(&hubsToPrint);

printHubList(hubsToPrint, hubLookup, searchResultHash);
printf("</div>");
return (hubsToPrint != NULL);
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
	outputPublicTable(conn, publicTable,statusTable, &retHash)) )
    {
    printf("<div id=\"publicHubs\" class=\"hubList\"> \n");
    printf("No Public Track Hubs found that match search criteria.<BR>");
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


static int doRedirect(struct cart *theCart)
{
struct hubConnectStatus *hub = hubConnectNewHub();
if (hub == NULL)
    return 0;

char headerText[1024];

char *errorMessage;
hubFindOrAddUrlInStatusTable(database, cart, hub->hubUrl, &errorMessage);

// if there is an error message, we stay in hgHubConnect
if (errorMessage != NULL)
    return 0;

getDbAndGenome(cart, &database, &organism, oldVars);

int redirDelay = 3;
printf( "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"%d;URL=%s?%s\">",
	  redirDelay,"../cgi-bin/hgGateway",cartSidUrlString(cart));
safef(headerText, sizeof(headerText), "Hub Connect Successful");
cartWebStart(cart, NULL, "%s", headerText);

hPrintf("You will be automatically redirected to the gateway page for this hub's default database "
    "(<A HREF=\"../cgi-bin/hgGateway?%s\">%s</A>) in %d seconds.<BR><BR>",
	  cartSidUrlString(cart),trackHubSkipHubName(database),redirDelay);

struct trackHub *tHub = hub->trackHub;
if (tHub->email != NULL)
    {
    hPrintf("<B>This hub is provided courtesy of <A HREF=\"mailto:%s\">%s</A>.</B> Please contact them with any questions.", tHub->email, tHub->email);
    }

hPrintf("<BR><BR>");
hPrintf("Hub: %s<BR><BR>", tHub->longLabel);
hPrintf("Hub Genomes: ");
struct trackHubGenome *genomeList = tHub->genomeList;

bool firstTime = TRUE;
for(; genomeList; genomeList = genomeList->next)
    {
    if (!firstTime)
	hPrintf(",");
    firstTime = FALSE;
    hPrintf("<A href=\"../cgi-bin/hgTracks?db=%s&%s\">%s</A>",genomeList->name, 
	cartSidUrlString(cart),trackHubSkipHubName(genomeList->name));
    }
hPrintf("<BR><BR>");
return 1;
}

static void doResetHub(struct cart *theCart)
{
char *url = cartOptionalString(cart, hgHubCheckUrl);

if (url != NULL)
    {
    udcSetCacheTimeout(1);
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

int hubConnectStatusCmp(const void *va, const void *vb)
/* Compare to sort based on shortLabel */
{
const struct hubConnectStatus *a = *((struct hubConnectStatus **)va);
const struct hubConnectStatus *b = *((struct hubConnectStatus **)vb);
struct trackHub *ta = a->trackHub;
struct trackHub *tb = b->trackHub;

if ((ta == NULL) || (tb == NULL))
    return 0;

return strcasecmp(tb->shortLabel, ta->shortLabel);
}

void doMiddle(struct cart *theCart)
/* Write header and body of html page. */
{
cart = theCart;

if(cgiIsOnWeb())
    checkForGeoMirrorRedirect(cart);

if (cartVarExists(cart, hgHubDoClear))
    {
    doClearHub(cart);
    cartWebEnd();
    return;
    }

if (cartVarExists(cart, hgHubCheckUrl))
    {
    doResetHub(cart);
    }

if (cartVarExists(cart, hgHubDoRedirect))
    {
    if (doRedirect(cart))
	{
	cartWebEnd();
	return;
	}
    }

cartWebStart(cart, NULL, "%s", pageTitle);

printf(
"<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.7/themes/default/style.min.css\" />\n"
"<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jquery/1.12.1/jquery.min.js\"></script>\n"
"<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.7/jstree.min.js\"></script>\n"
"<style>.jstree-default .jstree-anchor { height: initial; } </style>\n"
);
jsIncludeFile("utils.js", NULL);
jsIncludeFile("jquery-ui.js", NULL);
webIncludeResourceFile("jquery-ui.css");
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("hgHubConnect.js", NULL);
webIncludeResourceFile("hgHubConnect.css");
jsIncludeFile("jquery.cookie.js", NULL);
jsIncludeFile("spectrum.min.js", NULL);

boolean doHubDevMode = cfgOptionBooleanDefault("hgHubConnect.validateHub", FALSE);
printf("<div id=\"hgHubConnectUI\"> <div id=\"description\"> \n");
printf(
    "<P>Track data hubs are collections of external tracks that can be imported into the UCSC Genome Browser. "
    "Hub tracks show up under the hub's own blue label bar on the main browser page, "
    "as well as on the configure page. For more information, including where to "
    "<A HREF=\"../goldenPath/help/hgTrackHubHelp.html#Hosting\">host</A> your track hub, see the "
    "<A HREF=\"../goldenPath/help/hgTrackHubHelp.html\" TARGET=_blank>"
    "User's Guide</A>."
    "To import a public hub click its \"Connect\" button below.</P>"
    "<P><B>NOTE: Because Track Hubs are created and maintained by external sources,"
    " UCSC is not responsible for their content.</B></P>"
   );
printf("</div>\n");
if (doHubDevMode)
    {
    printf("<div id=\"developerModeDescription\" style=\"display:none\"> \n");
    printf("<p>Check the configuration settings for a hub, including the settings "
        "in the hub.txt, genomes.txt, and trackDb.txt files. If there are errors in the hub "
        "setup, a heirarchal tree of tracks is presented with the errors in red text. A hub "
        "with no errors still has the tree present but needs to be clicked to be expanded to "
        " explore the track heirarchy.</p>\n"
        "<p>Also present are buttons to load the hub with track timing measuring turned on "
        "in order to profile slow loading tracks, and an option to disable the track caching "
        "mechanism so the Genome Browser picks up changes to the configuration files or "
        "track data files. For more information on making track hubs, please see the following links: \n "
        "<ul>\n"
        "<li><a href=\"../goldenPath/help/trackDb/trackDbHub.html\">Track Hub Settings</a></li> \n"
        "<li><a href=\"../goldenPath/help/hgTrackHubHelp#Hosting\">Where to host your track hub</a></li> \n"
        "</ul> \n"
        "</div>\n "
        );
    }

// this variable is used by hub search and hub validate, initialize here so we don't
// overwrite it unintentionally depending on which path the CGI takes
jsInline("trackData = [];\n");

getDbAndGenome(cart, &database, &organism, oldVars);

char *survey = cfgOptionEnv("HGDB_HUB_SURVEY", "hubSurvey");
char *surveyLabel = cfgOptionEnv("HGDB_HUB_SURVEY_LABEL", "hubSurveyLabel");

if (survey && differentWord(survey, "off"))
    hPrintf("<span style='background-color:yellow;'><A HREF='%s' TARGET=_BLANK><EM><B>%s</EM></B></A></span>\n", survey, surveyLabel ? surveyLabel : "Take survey");
hPutc('\n');

// grab all the hubs that are listed in the cart
struct hubConnectStatus *hubList =  hubConnectStatusListFromCartAll(cart);

checkTrackDbs(hubList);

slSort(&hubList, hubConnectStatusCmp);

// here's a little form for the add new hub button
printf("<FORM ACTION=\"%s\" NAME=\"addHubForm\">\n",  "../cgi-bin/hgHubConnect");
cgiMakeHiddenVar("hubUrl", "");
cgiMakeHiddenVar( hgHubDoFirstDb, "on");
cgiMakeHiddenVar( hgHubDoRedirect, "on");
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
cartSaveSession(cart);
puts("</FORM>");

// this is the form for the connect hub button
printf("<FORM ACTION=\"%s\" NAME=\"connectHubForm\">\n",  "../cgi-bin/hgHubConnect");
cgiMakeHiddenVar("hubUrl", "");
cgiMakeHiddenVar("db", "");
cgiMakeHiddenVar( hgHubDoRedirect, "on");
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
cartSaveSession(cart);
puts("</FORM>");

// this is the form for the disconnect hub button
printf("<FORM ACTION=\"%s\" NAME=\"disconnectHubForm\">\n",  "../cgi-bin/hgHubConnect");
cgiMakeHiddenVar("hubId", "");
cgiMakeHiddenVar(hgHubDoDisconnect, "on");
cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");
cartSaveSession(cart);
puts("</FORM>");

// this is the form for the reset hub button
printf("<FORM ACTION=\"%s\" NAME=\"resetHubForm\">\n",  "../cgi-bin/hgHubConnect");
cgiMakeHiddenVar(hgHubCheckUrl, "");
cartSaveSession(cart);
puts("</FORM>");

// this is the form for the search hub button
printf("<FORM ACTION=\"%s\" NAME=\"searchHubForm\">\n",  "../cgi-bin/hgHubConnect");
cgiMakeHiddenVar(hgHubSearchTerms, "");
cgiMakeHiddenVar(hgHubDoSearch, "on");
cgiMakeHiddenVar(hgHubDbFilter, "");
cartSaveSession(cart);
puts("</FORM>");

// this is the form for the validate button
if (doHubDevMode)
    {
    printf("<FORM ACTION=\"%s\" NAME=\"validateHubForm\">\n",  "../cgi-bin/hgHubConnect");
    cgiMakeHiddenVar("validateHubUrl", "");
    cgiMakeHiddenVar("measureTiming", "");
    cgiMakeHiddenVar("udcTimeout", "");
    // allows saving the old url in the search bar but not run hubCheck on it right away
    // mostly for when you come back to hgHubConnect after just looking at hgTracks
    cgiMakeHiddenVar(hgHubDoHubCheck, "off");
    cartSaveSession(cart);
    puts("</FORM>");
    }

// ... and now the main form
printf("<FORM ACTION=\"%s\" METHOD=\"POST\" NAME=\"mainForm\">\n", "../cgi-bin/hgGateway");
cartSaveSession(cart);

// we have two tabs for the public and unlisted hubs
printf("<div id=\"tabs\">"
       "<ul> <li><a class=\"defaultDesc\" href=\"#publicHubs\">Public Hubs</a></li>"
       "<li><a class=\"defaultDesc\" href=\"#unlistedHubs\">My Hubs</a></li> ");
if (doHubDevMode) // put up the validate tab if hg.conf statement present
    printf("<li><a class=\"hubDeveloperDesc\" href=\"#hubDeveloper\">Hub Development</a></li>");
printf("</ul> ");

struct hash *publicHash = hgHubConnectPublic();
hgHubConnectUnlisted(hubList, publicHash);
if (doHubDevMode)
    hgHubConnectDeveloperMode();
// add the event listener for toggling the right description text
jsInline(
    "var oldDesc = document.getElementById('description');\n"
    "var newDesc = document.getElementById('developerModeDescription');\n"
    "if (location.hash === \"#hubDeveloper\") {\n"
    "   toggleTwoDivs(newDesc, oldDesc);\n"
    "}\n"
    "window.addEventListener('hashchange', function() {\n"
    "   if (location.hash === \"#hubDeveloper\") {"
    "       toggleTwoDivs(newDesc, oldDesc);\n"
    "   } else {\n"
    "       toggleTwoDivs(oldDesc, newDesc);\n"
    "   }\n"
    "});\n"
    "var plusButton = document.getElementById('advancedSettingsButton');\n"
    "plusButton.addEventListener('click', function() {\n"
    "   var advancedSettingsDiv = document.getElementById('extraSettingsContainer');\n"
    "   var extraSettingsList = document.getElementById('extraSettingsList');\n"
    "   if (extraSettingsList.style.display != 'none') {\n"
    "       extraSettingsList.style.display = 'none';\n"
    "       imgTag = advancedSettingsDiv.getElementsByTagName('img')[0];\n"
    "       imgTag.src = \"../images/add_sm.gif\";\n"
    "   } else {\n"
    "       extraSettingsList.style.display = 'block';\n"
    "       imgTag = advancedSettingsDiv.getElementsByTagName('img')[0];\n"
    "       imgTag.src = \"../images/remove_sm.gif\";\n"
    "   }\n"
    "});"
    );
printf("</div>");

printf("<div class=\"tabFooter\">");

printf("<span class=\"small\">"
    "Contact <a href=\"../contacts.html\">us</A> to add a public hub."
    "</span>\n");
printf("</div>");

cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");

puts("</FORM>");
printf("</div>\n");

cartWebEnd();
}

char *excludeVars[] = {"Submit", "submit", "hc_one_url", hgHubDoHubCheck,
    hgHubCheckUrl, hgHubDoClear, hgHubDoDisconnect,hgHubDoRedirect, hgHubDataText, 
    hgHubConnectRemakeTrackHub, NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
oldVars = hashNew(10);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgHubConnect", enteredMainTime);
return 0;
}

