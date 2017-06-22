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
    struct dyString *descriptionMatch;
    struct genomeOutputStructure *genomes;
    int genomeCount;
    struct hash *genomeOutHash;
    };

struct genomeOutputStructure
    {
    struct genomeOutputStructure *next;
    struct dyString *shortLabel;
    struct dyString *descriptionMatch;
    struct tdbOutputStructure *tracks;
    struct dyString *assemblyLink;
    char *genomeName;
    int trackCount;
    struct hash *tdbOutHash;
    int hitCount;
    };

struct tdbOutputStructure
    {
    struct tdbOutputStructure *next;
    struct dyString *shortLabel;
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
    char id[256];
    safef(tempHtml, sizeof tempHtml, 
	"<span id=Short%d>[+]&nbsp;%s...</span>"
	"<span id=Full%d style=\"display:none\">[-]<br>%s</span>"
	, row, genomesShort 
	, row, genomesString);

    safef(id, sizeof id, "Short%d", row);
    jsOnEventByIdF("click", id,
	"document.getElementById('Short%d').style.display='none';"
	"document.getElementById('Full%d').style.display='inline';"
	"return false;"
	, row, row);

    safef(id, sizeof id, "Full%d", row);
    jsOnEventByIdF("click", id, 
	"document.getElementById('Full%d').style.display='none';"
	"document.getElementById('Short%d').style.display='inline';"
	"return false;"
	, row, row);
    }
ourPrintCell(tempHtml);
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
    safef(id, sizeof id, "hubDisconnectButton%d", count);
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

    if (!isEmpty(hub->errorMessage))
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
	printGenomes(hub->trackHub, count);
    else
	ourPrintCell("");

    puts("</tr>");
    }

printf("</tbody></TABLE>\n");
printf("</div>");
}


static void addPublicHubsToHubStatus(struct sqlConnection *conn, char *publicTable, char  *statusTable)
/* Add urls in the hubPublic table to the hubStatus table if they aren't there already */
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


struct hubSearchText *getHubSearchResults(struct sqlConnection *conn, char *hubSearchTableName,
        char *hubSearchTerms, bool checkLongText, char *dbFilter, struct hash *hubLookup)
/* Find hubs, genomes, and tracks that match the provided search terms.
 * Return all hits that satisfy the (optional) supplied assembly filter.
 * if checkLongText is FALSE, skip searching within the long description text entries */
{
struct hubSearchText *hubSearchResultsList = NULL;
struct dyString *query = dyStringNew(100);
char *noLongText = NULL;

if (!checkLongText)
    noLongText = cloneString("textLength = 'Short' and");
else
    noLongText = cloneString("");

sqlDyStringPrintf(query, "select * from %s where %s match(text) against ('%s' in natural language mode)",
        hubSearchTableName, noLongText, hubSearchTerms);

struct sqlResult *sr = sqlGetResult(conn, dyStringContents(query));
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hubSearchText *hst = hubSearchTextLoadWithNullGiveContext(row, hubSearchTerms);
    char *hubUrl = hst->hubUrl;
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
            struct hubEntry *hubInfo = hashFindVal(hubLookup, hubUrl);
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
        safef(jsId, sizeof jsId, "hubDisconnectButton%d", count);
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

if (isEmpty(hubInfo->errorMessage))
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

printGenomeList(dbListNames, count); 
printf("</tr>\n");
}


void printSearchOutputForTrack(struct tdbOutputStructure *tdbOut)
/* Write out a <li> entry for a search hit on a track, along with a nested
 * <ul> for any included hits to subtracks */
{
printf("<li configLink='%s' nodeType='track'>\n", dyStringContents(tdbOut->configUrl));
printf("%s", dyStringContents(tdbOut->shortLabel));
if (tdbOut->childCount > 0)
    printf(" (%d subtrack%s)", tdbOut->childCount, tdbOut->childCount==1?"":"s");
printf("<br>\n");
if (isNotEmpty(dyStringContents(tdbOut->descriptionMatch)))
    {
    printf("<span class='descriptionMatch'><em>%s</em></span>\n", dyStringContents(tdbOut->descriptionMatch));
    }
if (tdbOut->children != NULL)
    {
    struct tdbOutputStructure *child = tdbOut->children;
    printf("<ul>\n");
    while (child != NULL)
        {
        printSearchOutputForTrack(child);
        child = child->next;
        }
    printf("</ul>\n");
    }
printf("</li>\n");
}


void printSearchOutputForGenome(struct genomeOutputStructure *genomeOut)
/* Write out a chunk of search results for a genome as a <li>, with a nested ul
 * element for hits to tracks within that genome */
{
printf("<li assemblyLink='%s' nodeType='assembly'>%s",
        dyStringContents(genomeOut->assemblyLink), dyStringContents(genomeOut->shortLabel));
if (genomeOut->trackCount > 0)
    printf(" (%d track%s)", genomeOut->trackCount, genomeOut->trackCount==1?"":"s");

if (isNotEmpty(dyStringContents(genomeOut->descriptionMatch)))
    {
    printf("<br>\n<em>Assembly Description:</em> %s\n", dyStringContents(genomeOut->descriptionMatch));
    }
if (genomeOut->tracks != NULL)
    {
    printf("<ul>\n");
    struct tdbOutputStructure *tdbOut = genomeOut->tracks;
    while (tdbOut != NULL)
        {
        printSearchOutputForTrack(tdbOut);
        tdbOut = tdbOut->next;
        }
    printf("</ul>\n");
    }
printf("</li>\n");
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


struct tdbOutputStructure *addOrUpdateTrackOut(char *track, struct genomeOutputStructure *genomeOut,
        struct hash *tdbHash, struct trackHub *hub)
/* If an output structure already exists for the track within genomeOut, return that.  Otherwise,
 * create one for it and add it to genomeOut.  Any missing parent tracks are also added at
 * the same time.
 * tdbHash takes track names to the struct trackDb * for that track */
{
struct tdbOutputStructure *tdbOut = hashFindVal(genomeOut->tdbOutHash, track);
if (tdbOut == NULL)
    {
    genomeOut->trackCount++;
    AllocVar(tdbOut);
    tdbOut->shortLabel = dyStringNew(0);
    tdbOut->descriptionMatch = dyStringNew(0);
    tdbOut->configUrl = dyStringNew(0);
    struct trackDb *trackInfo = (struct trackDb *) hashFindVal(tdbHash, track);
    if (trackInfo == NULL)
        {
        // Some tracks are prefixed with the hub name; try that
        char withHubName[4096];
        safef(withHubName, sizeof(withHubName), "%s_%s", hub->name, track);
        trackInfo = hashMustFindVal(tdbHash, withHubName);
        }
    if (isNotEmpty(trackInfo->longLabel))
        dyStringPrintf(tdbOut->shortLabel, "%s", trackInfo->longLabel);
    else if (isNotEmpty(trackInfo->shortLabel))
        dyStringPrintf(tdbOut->shortLabel, "%s", trackInfo->shortLabel);
    else
        dyStringPrintf(tdbOut->shortLabel, "%s", trackHubSkipHubName(trackInfo->track));

    dyStringPrintf(tdbOut->configUrl, "../cgi-bin/hgTrackUi?hubUrl=%s&db=%s&g=%s&hgsid=%s", hub->url,
            genomeOut->genomeName, trackInfo->track, cartSessionId(cart));

    if (trackInfo->parent != NULL)
        {
        struct trackDb *parent = trackInfo->parent;
        struct tdbOutputStructure *parentOut = addOrUpdateTrackOut(parent->track, genomeOut, tdbHash, hub);
        slAddTail(&(parentOut->children), tdbOut);
        parentOut->childCount++;
        }
    else
        slAddTail(&(genomeOut->tracks), tdbOut);
    hashAdd(genomeOut->tdbOutHash, track, tdbOut);
    }
return tdbOut;
}


void buildTdbHash(struct hash *tdbHash, struct trackDb *tdbList)
/* Recursively add all tracks from tdbList to the hash (indexed by track),
 * along with all parents and children of those tracks */
{
struct trackDb *tdb = tdbList;
while (tdb != NULL)
    {
    hashAdd(tdbHash, tdb->track, tdb);
    if (tdb->subtracks != NULL)
        buildTdbHash(tdbHash, tdb->subtracks);
    if (tdb->parent != NULL)
        {
        // supertracks might be omitted from tdbList, but are still referred to by parent links
        if (hashFindVal(tdbHash, tdb->parent->track) == NULL)
            hashAdd(tdbHash, tdb->parent->track, tdb->parent);
        }
    tdb = tdb->next;
    }
}


struct hubOutputStructure *buildHubSearchOutputStructure(struct trackHub *hub,
        struct hubSearchText *searchResults)
/* Build a structure that contains the data for writing out the hub search results for this hub */
{
struct hubOutputStructure *hubOut = NULL;
AllocVar(hubOut);
hubOut->descriptionMatch = dyStringNew(0);
hubOut->genomeOutHash = newHash(5);

struct hash *tdbHashHash = newHash(5);  // takes genome names to trackDb hashes

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
        continue;
        }

    char *db = cloneString(hst->db);
    struct trackHubGenome *genome = hashFindVal(hub->genomeHash, db);
    if (genome == NULL)
        {
        // assembly hub genomes are stored with a prefix; try that
        char withHubName[4096];
        safef(withHubName, sizeof(withHubName), "%s_%s", hub->name, db);
        genome = hashMustFindVal(hub->genomeHash, withHubName);
        }
    struct genomeOutputStructure *genomeOut = hashFindVal(hubOut->genomeOutHash, db);
    if (genomeOut == NULL)
        {
        AllocVar(genomeOut);
        genomeOut->tdbOutHash = newHash(5);
        genomeOut->descriptionMatch = dyStringNew(0);
        genomeOut->shortLabel = dyStringNew(0);
        genomeOut->assemblyLink = dyStringNew(0);
        dyStringPrintf(genomeOut->assemblyLink, "../cgi-bin/hgTracks?hubUrl=%s&db=%s&hgsid=%s",
                hub->url, genome->name, cartSessionId(cart));
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

    if (isNotEmpty(hst->track))
        {
        // Time to add a track! (or add info to one, maybe)
        struct hash *tdbHash = (struct hash *) hashFindVal(tdbHashHash, db);
        if (tdbHash == NULL)
            {
            tdbHash = newHash(5);
            hashAdd(tdbHashHash, db, tdbHash);
            struct trackDb *tdbList = trackHubTracksForGenome(hub, genome);
            tdbList = trackDbLinkUpGenerations(tdbList);
            tdbList = trackDbPolishAfterLinkup(tdbList, db);
            trackHubPolishTrackNames(hub, tdbList);
            buildTdbHash(tdbHash, tdbList);
            }
        struct tdbOutputStructure *tdbOut = addOrUpdateTrackOut(hst->track, genomeOut, tdbHash, hub);
        if (hst->textLength == hubSearchTextLong)
            dyStringPrintf(tdbOut->descriptionMatch, "%s", hst->text);
        }
    }
return hubOut;
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
    struct hubOutputStructure *hubOut = buildHubSearchOutputStructure(hub, hubSearchResult);
    if (dyStringIsEmpty(hubOut->descriptionMatch) && (hubOut->genomes == NULL))
        return; // no detailed search results; hit must have been to hub short label or something   

    printf("<div class=\"hubTdbTree\">\n");
    printf("<ul>\n");
    printf("<li>Search details ...\n<ul>\n");
    if (isNotEmpty(dyStringContents(hubOut->descriptionMatch)))
        printf("<li>Hub Description:&nbsp<span class='descriptionMatch'><em>%s</em></span></li>\n", dyStringContents(hubOut->descriptionMatch));

    struct genomeOutputStructure *genomeOut = hubOut->genomes;
    if (genomeOut != NULL)
        {
        printf("<li>%d Matching Assembl%s\n<ul>\n", hubOut->genomeCount, hubOut->genomeCount==1?"y":"ies");
        while (genomeOut != NULL)
            {
            printSearchOutputForGenome(genomeOut);
            genomeOut = genomeOut->next;
            }
        printf("</ul></li>\n");
        }
    printf("</ul></li></ul></div>\n");
    }
}


void printHubList(struct slName *hubsToPrint, struct hash *hubLookup, struct hash *searchResultHash)
/* Print out a list of hubs, possibly along with search hits to those hubs.
 * hubLookup takes hub URLs to struct hubEntry
 * searchResultHash takes hub URLs to struct hubSearchText * (list of hits on that hub)
 */
{
int count = 0;
int udcTimeoutVal = udcCacheTimeout();
char *udcOldDir = cloneString(udcDefaultDir());
char *searchUdcDir = cfgOptionDefault("hgHubConnect.cacheDir", udcOldDir);
udcSetDefaultDir(searchUdcDir);
udcSetCacheTimeout(1<<30);
if (hubsToPrint != NULL)
    {
    printHubListHeader();

    if (searchResultHash == NULL) // if not displaying search results, join the hub <tr>s into one table
        printf("<table class='hubList'><tbody>\n");
    struct slName *thisHubName = NULL;
    for (thisHubName = hubsToPrint; thisHubName != NULL; thisHubName = thisHubName->next)
        {
        struct hubEntry *hubInfo = (struct hubEntry *) hashFindVal(hubLookup, thisHubName->name);
        if (hubInfo == NULL)
            {
            /* This shouldn't happen often, but maybe the search hits list was built from an outdated
             * search text file that includes hubs for which no info is available.
             * Skip this hub. */
            continue;
            }
        struct hubSearchText *searchResult = NULL;
        if (searchResultHash != NULL)
            {
            searchResult = (struct hubSearchText *) hashMustFindVal(searchResultHash, thisHubName->name);
            }
        printOutputForHub(hubInfo, searchResult, count);
        count++;
        }
    if (searchResultHash == NULL)
        printf("</tbody></table>\n");
    }
udcSetCacheTimeout(udcTimeoutVal);
udcSetDefaultDir(udcOldDir);
if (hubsToPrint != NULL)
    {
    /* Write out the list of hubs in a single table inside a div that will be hidden by
     * javascript.  This table is used (before being hidden) to set common column widths for
     * the individual hub tables when they're split by detailed search results. */
    printf("<div id='hideThisDiv'>\n");
    printf("<table class='hubList' id='hideThisTable'><tbody>\n");
    struct slName *thisHubName = NULL;
    for (thisHubName = hubsToPrint; thisHubName != NULL; thisHubName = thisHubName->next)
        {
        struct hubEntry *hubInfo = (struct hubEntry *) hashFindVal(hubLookup, thisHubName->name);
        if (hubInfo == NULL)
            {
            continue;
            }
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
}


static bool outputPublicTable(struct sqlConnection *conn, char *publicTable, char *statusTable,
        struct hash **pHash)
/* Put up the list of public hubs and other controls for the page. */
{
char *hubSearchTerms = cartOptionalString(cart, hgHubSearchTerms);
char *cleanSearchTerms = cloneString(hubSearchTerms); // only cleaned by tolowers() at the moment
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
    if (isNotEmpty(cleanSearchTerms))
        tolowers(cleanSearchTerms);
    // Forcing checkDescriptions to TRUE right now, but we might want to add this as a
    // checkbox option for users in the near future.
    bool checkDescriptions = TRUE;
    struct hubSearchText *hubSearchResults = getHubSearchResults(conn, hubSearchTableName,
            cleanSearchTerms, checkDescriptions, lcDbFilter, hubLookup);
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
            slAddTail(&(hubHashEnt->val), hst);
        hst = nextHst;
        }
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
"<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.4/themes/default/style.min.css\" />\n"
"<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jquery/1.12.1/jquery.min.js\"></script>\n"
"<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.4/jstree.min.js\"></script>\n"
"<style>.jstree-default .jstree-anchor { height: initial; } </style>\n"
);
jsIncludeFile("utils.js", NULL);
jsIncludeFile("jquery-ui.js", NULL);
webIncludeResourceFile("jquery-ui.css");
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("hgHubConnect.js", NULL);
webIncludeResourceFile("hgHubConnect.css");
jsIncludeFile("jquery.cookie.js", NULL);

printf("<div id=\"hgHubConnectUI\"> <div id=\"description\"> \n");
printf(
    "<P>Track data hubs are collections of external tracks that can be imported into the UCSC Genome Browser. "
    "Hub tracks show up under the hub's own blue label bar on the main browser page, "
    "as well as on the configure page. For more information, see the "
    "<A HREF=\"../goldenPath/help/hgTrackHubHelp.html\" TARGET=_blank>"
    "User's Guide</A>."
    "To import a public hub click its \"Connect\" button below.</P>"
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

printf("<span class=\"small\">"
    "Contact <a href=\"../contacts.html\">us</A> to add a public hub."
    "</span>\n");
printf("</div>");

cgiMakeHiddenVar(hgHubConnectRemakeTrackHub, "on");

puts("</FORM>");
printf("</div>\n");

jsInline(
"var hubSearchTree = (function() {\n"
"    // Effectively global vars set by init\n"
"    var treeDiv;        // Points to div we live in\n"
"\n"
"    function hubSearchTreeContextMenuHandler (node, callback) {\n"
"        var nodeType = node.li_attr.nodetype;\n"
"        if (nodeType == 'track') {\n"
"            callback({\n"
"               'openConfig': {\n"
"                   'label' : 'Configure this track',\n"
"                   'action' : function () {window.open(node.li_attr.configlink, '_blank'); }\n"
"               }\n"
"            });\n"
"        }\n"
"        else if (nodeType == 'assembly') {\n"
"            callback({\n"
"               'openConfig': {\n"
"                   'label' : 'Open this assembly',\n"
"                   'action' : function () {window.open(node.li_attr.assemblylink, '_blank'); }\n"
"               }\n"
"            });\n"
"        }\n"
"    }\n"
"    function toggleExpansion(node, event) {\n"
"       var ident = '#' + node.id;\n"
"       if (event.type != 'contextmenu')\n"
"           $(ident).jstree(true).toggle_node(node);\n"
"       return false;\n"
"    }\n" 
"    function init() {\n"
"       $.jstree.defaults.core.themes.icons = false;\n"
"       $.jstree.defaults.core.themes.dots = true;\n"
"       $.jstree.defaults.contextmenu.show_at_node = false;\n"
"       $.jstree.defaults.contextmenu.items = hubSearchTreeContextMenuHandler\n"
"       treeDiv=$('.hubTdbTree');\n"
"       treeDiv.jstree({\n"
"               'conditionalselect' : function (node, event) { toggleExpansion(node, event); },\n"
"               'plugins' : ['conditionalselect', 'contextmenu'],\n"
"               'core' : { dblclick_toggle: false }\n"
"               });\n"
"    }\n\n"
"    return { init: init};\n\n"
"}());\n"
"\n"
"$(function () {\n"
"    hubSearchTree.init();\n"
"});\n"
);

cartWebEnd();
}

char *excludeVars[] = {"Submit", "submit", "hc_one_url", 
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

