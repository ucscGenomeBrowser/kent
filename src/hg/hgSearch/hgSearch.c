/* hgSearch - User interface to explore hgFind search results */
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
#include "udc.h"
#include "knetUdc.h"
#include "hgConfig.h"
#include "jsHelper.h"
#include "errCatch.h"
#include "hgFind.h"
#include "cartJson.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "jsonWrite.h"
#include "hgFind.h"
#include "trix.h"
#include "genbank.h"
#include "cartTrackDb.h"
#include "chromAlias.h"

// name of the searchBar form variable from the HTML
#define SEARCH_TERM_VAR "search"
#define SEARCH_LIMIT 100

/* Standard CGI Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;
boolean measureTiming = FALSE;

/* Caches used by hgFind.c */
extern struct trackDb *hgFindTdbList;
extern struct grp *hgFindGrpList;
extern struct hash *hgFindGroupHash;
extern struct hash *hgFindTrackHash;

struct dbDbHelper
/* A struct dbDb with an extra field for whether the assembly is the default */
{
struct dbDbHelper *next;
struct dbDb *dbDb;
boolean isDefault;
boolean isCurated;;
};

/* Helper functions for cart handlers */
static struct dbDbHelper *getDbDbWithDefault()
/* Grab rows from dbDb along with whether assembly is the default or not */
{
struct dbDbHelper *ret = NULL;
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;
struct dbDb *db;
struct hash *hash = sqlHashOfDatabases();

char query[1024];
sqlSafef(query, sizeof query,  "select dbDb.*,defaultDb.name from %s "
        "join %s on %s.genome=%s.genome where active=1 order by orderKey,dbDb.name desc",
        dbDbTable(), defaultDbTable(), dbDbTable(), defaultDbTable());
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct dbDbHelper *this;
    db = dbDbLoad(row);
    boolean isGenarkHub = sameOk(db->nibPath, "genark");
    boolean isCuratedHub = startsWith("hub:", db->nibPath);
    if (isGenarkHub || isCuratedHub ||  hashLookup(hash, db->name))
        {
        AllocVar(this);
        this->dbDb = db;
        this->isDefault = sameString(row[14],row[0]);
        this->isCurated = isCuratedHub;
        slAddTail(&ret, this);
        }
    else
        dbDbFree(&db);
    }
sqlFreeResult(&sr);
hashFree(&hash);
hDisconnectCentral(&conn);
slReverse(&ret);
return ret;
}

struct hgPositions *doQuery(struct jsonWrite *jw, char *db, struct searchCategory *categories, boolean doRedirect, char *searchTerms, boolean measureTiming)
/* Fire off a query. If the query results in a single position and we came from another CGI,
 * inform the client that we can redirect right to hgTracks, otherwise write the
 * list as JSON. Return the results if we want to handle the redirect server
 * side later. */
{
struct hgPositions *hgp = NULL;
char *chrom;
int retWinStart = 0, retWinEnd = 0;
boolean categorySearch = TRUE;
hgp = genomePosCJ(jw, db, searchTerms, &chrom, &retWinStart, &retWinEnd, cart, categories, categorySearch);
// at this point, if the search term wasn't a singlePos, we have written
// out the JSON already. So now take care of the singlePos case.
if (hgp && hgp->singlePos)
    {
    // if we got an hgvs match to chromInfo (example: chrX:g.31500000_31600000del),
    // or just a plain position range was searched, we have to create the json
    // manually, cause the tdb lookups in hgPositionsJson() won't work
    struct hgPosTable *table = hgp->tableList;
    jsonWriteListStart(jw, "positionMatches");
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "name", table->name);
    jsonWriteString(jw, "description", table->description);
    if (table->searchTime >= 0)
        jsonWriteNumber(jw, "searchTime", table->searchTime);
    jsonWriteListStart(jw, "matches");

    jsonWriteObjectStart(jw, NULL);
    char position[512];
    retWinStart = hgp->singlePos->chromStart;
    retWinEnd = hgp->singlePos->chromEnd;
    if (hgp->singlePos->chromStart > hgp->singlePos->chromEnd)
        {
        retWinStart = retWinEnd;
        retWinEnd = hgp->singlePos->chromStart;
        }
    safef(position, sizeof(position), "%s:%d-%d", hgp->singlePos->chrom, retWinStart+1, retWinEnd);
    jsonWriteString(jw, "position", position);
    jsonWriteString(jw, "posName", hgp->query);
    if (doRedirect)
        {
        // If we are coming from hgTracks or hgGateway, then we can just return right
        // back to hgTracks, the client will handle this:
        jsonWriteBoolean(jw, "doRedirect", doRedirect);
        }
    jsonWriteObjectEnd(jw);

    jsonWriteListEnd(jw); // end matches
    jsonWriteObjectEnd(jw); // end one table
    jsonWriteListEnd(jw); // end positionMatches
    }
return hgp;
}

static void addCategoryFieldsToHash(struct hash *elementHash, struct searchCategory *category)
{
hashAdd(elementHash, "name", newJsonString(category->name));
hashAdd(elementHash, "id", newJsonString(category->id));
hashAdd(elementHash, "visibility", newJsonNumber((long)category->visibility));
hashAdd(elementHash, "group", newJsonString(category->groupName));
hashAdd(elementHash, "label", newJsonString(category->label));
hashAdd(elementHash, "description", newJsonString(category->description));
hashAdd(elementHash, "parents", newJsonString(slNameListToString(category->parents, ',')));
hashAdd(elementHash, "priority", newJsonDouble(category->priority));
}

static boolean nonTrackCateg(struct searchCategory *categ)
{
if (sameString("publicHubs", categ->id) ||
        sameString("helpDocs", categ->id) ||
        startsWith("trackDb", categ->id))
    return TRUE;
return FALSE;
}

static struct jsonElement *jsonElementFromVisibleCategs(struct searchCategory *categs)
{
struct searchCategory *categ = NULL;
struct jsonElement *ret = newJsonList(NULL);
for (categ = categs; categ != NULL; categ = categ->next)
    {
    if (categ->visibility > 0)
        jsonListAdd(ret, newJsonString(categ->id));
    }
return ret;
}

struct searchCategory *getVisibleCategories(struct searchCategory *categories)
/* Return a list of only the visible categories. Use CloneVar so our original list
 * stays alive */
{
struct searchCategory *ret = NULL, *categ = NULL;
for (categ = categories; categ != NULL; categ = categ->next)
    {
    if (categ->visibility > 0)
        {
        struct searchCategory *temp = (struct searchCategory *)CloneVar(categ);
        slAddHead(&ret, temp);
        }
    }
return ret;
}

struct jsonElement *makeTrackGroupsJson(char *db)
/* Turn the available track groups for a database into JSON. Needed for
 * sorting the categories */
{
hashTracksAndGroups(cart, db);
struct jsonElement *retObj = newJsonObject(hashNew(0));
struct grp *grp;
for (grp = hgFindGrpList; grp != NULL; grp = grp->next)
    {
    if (!sameString(grp->name, "allTracks") && !sameString(grp->name, "allTables"))
        {
        struct jsonElement *grpObj = newJsonObject(hashNew(0));
        jsonObjectAdd(grpObj, "name", newJsonString(grp->name));
        jsonObjectAdd(grpObj, "label", newJsonString(grp->label));
        jsonObjectAdd(grpObj, "priority", newJsonDouble(grp->priority));
        jsonObjectAdd(retObj, grp->name, grpObj);
        }
    }
return retObj;
}

static struct jsonElement *jsonElementFromSearchCategory(struct searchCategory *categories, char *db)
/* Turn struct searchCategory into jsonElement */
{
struct jsonElement *trackList = newJsonList(NULL);
struct jsonElement *nonTrackList = newJsonList(NULL);
struct searchCategory *categ = NULL;
for (categ = categories; categ != NULL; categ = categ->next)
    {
    struct hash *eleHash = hashNew(0);
    struct jsonElement *categJson = newJsonObject(eleHash);
    addCategoryFieldsToHash(eleHash, categ);

    // now add to one of our final lists
    if (nonTrackCateg(categ))
        {
        jsonListAdd(nonTrackList, categJson);
        }
    else
        {
        if (categ->id)
            jsonListAdd(trackList, categJson);
        }
    }

// we need to enclose the track list categories in a parent category
// for the javascript to function:
struct jsonElement *trackContainerEle = newJsonObject(hashNew(0));
jsonObjectAdd(trackContainerEle, "id", newJsonString("trackData"));
char name[2048];
safef(name, sizeof(name), "%s Track Data", db);
jsonObjectAdd(trackContainerEle, "name", newJsonString(name));
jsonObjectAdd(trackContainerEle, "tracks", trackList);
jsonObjectAdd(trackContainerEle, "label", newJsonString(name));
jsonObjectAdd(trackContainerEle, "priority", newJsonDouble(2.0));
jsonObjectAdd(trackContainerEle, "description", newJsonString("Search for track data items"));
jsonListAdd(nonTrackList, trackContainerEle);
return nonTrackList;
}

struct searchCategory *makeCategsFromJson(struct jsonElement *searchCategs, char *db)
/* User has selected some categories, parse the JSON into a struct searchCategory */
{
if (searchCategs == NULL)
    return getAllCategories(cart, db, hgFindGroupHash);
struct searchCategory *ret = NULL;
struct slRef *jsonVal = NULL;
for (jsonVal= searchCategs->val.jeList; jsonVal != NULL; jsonVal = jsonVal->next)
    {
    struct jsonElement *val = jsonVal->val;
    if (val->type != jsonString)
        errAbort("makeCategsFromJson: non-string argument value for 'categs'");
    char *categName = val->val.jeString;
    struct searchCategory *category = NULL;
    if (!sameString(categName, "allTracks"))
        category = makeCategory(cart, categName, NULL, db, hgFindGroupHash);
    else
        {
        struct hashEl *hel, *helList = hashElListHash(hgFindTrackHash);
        for (hel = helList; hel != NULL; hel = hel->next)
            {
            struct trackDb *tdb = hel->val;
            if (!sameString(tdb->track, "knownGene") && !sameString(tdb->table, "knownGene"))
                slAddHead(&category, makeCategory(cart, tdb->track, NULL, db, hgFindGroupHash));
            }
        // the hgFindTrackHash can contain both a composite track (where makeCategory would make categories
        // for each of the subtracks, and a subtrack, where makeCategory just makes a single
        // category, which means our final list can contain duplicate categories, so do a
        // uniqify here so we only have one category for each category
        slUniqify(&category, cmpCategories, searchCategoryFree);
        }
    if (category != NULL)
        {
        if (ret)
            slCat(&ret, category);
        else
            ret = category;
        }
    }
return ret;
}

void jsonElementSaveCategoriesToCart(char *db, struct jsonElement *jel)
/* Write the currently selected categories to the cart so on back button navigation
 * we only search the selected categories. jel must be a jeList of strings */
{
char cartSetting[512];
safef(cartSetting, sizeof(cartSetting), "hgSearch_categs_%s", db);
struct dyString *cartVal = dyStringNew(0);
struct slRef *categ;
for (categ = jel->val.jeList; categ != NULL; categ = categ->next)
    {
    struct jsonElement *val = categ->val;
    if (val->type != jsonString)
        errAbort("saveCategoriesToCart: category is not a string");
    else
        dyStringPrintf(cartVal, "%s,", val->val.jeString);
    }
cartSetString(cart, cartSetting, dyStringCannibalize(&cartVal));
}

static void writeDefaultForDb(struct jsonWrite *jw, char *database)
/* Put up the default view when entering this page for the first time, which basically
 * means return the list of searchable stuff. */
{
struct searchCategory *defaultCategories = getAllCategories(cart, database, hgFindGroupHash);
struct jsonElement *categsJsonElement = jsonElementFromSearchCategory(defaultCategories, database);
struct jsonElement *selectedCategsJsonList = jsonElementFromVisibleCategs(defaultCategories);
jsonElementSaveCategoriesToCart(database,selectedCategsJsonList);
dyStringPrintf(jw->dy, ", \"db\": \"%s\"", database);
dyStringPrintf(jw->dy, ", \"categs\": ");
jsonDyStringPrint(jw->dy, categsJsonElement, NULL, -1);
dyStringPrintf(jw->dy, ", \"trackGroups\": ");
jsonDyStringPrint(jw->dy, makeTrackGroupsJson(database), NULL, -1);
}

void printMainPageIncludes()
{
webIncludeResourceFile("gb.css");
webIncludeResourceFile("gbStatic.css");
webIncludeResourceFile("jquery-ui.css");
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("jquery-ui.js", NULL);
puts("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/themes/default/style.min.css' />");
puts("<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.7/jstree.min.js\"></script>\n");
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("lodash.3.10.0.compat.min.js", NULL);
jsIncludeFile("cart.js", NULL);
jsIncludeFile("autocompleteCat.js", NULL);
jsIncludeFile("hgSearch.js", NULL);

// Write the skeleton HTML, which will get filled out by the javascript
webIncludeFile("inc/hgSearch.html");
webIncludeFile("inc/gbFooter.html");
}

/* End handler helper functions */

/* Handlers for returning JSON to client */
static void getSearchResults(struct cartJson *cj, struct hash *paramHash)
/* User has entered a term to search, search this term against the selected categories */
{
char *db = cartJsonRequiredParam(paramHash, "db", cj->jw, "getSearchResults");
cartSetString(cj->cart, "db", db);
initGenbankTableNames(db);
hashTracksAndGroups(cj->cart, db);
chromAliasSetup(db);
char *searchTerms = cartJsonRequiredParam(paramHash, SEARCH_TERM_VAR, cj->jw, "getSearchResults");
measureTiming = cartUsualBoolean(cj->cart, "measureTiming", FALSE);
struct jsonElement *searchCategs = hashFindVal(paramHash, "categs");
struct searchCategory *searchCategoryList = makeCategsFromJson(searchCategs, db);
boolean doRedirect = FALSE;
(void)doQuery(cj->jw, db, searchCategoryList, doRedirect, searchTerms, measureTiming);
fprintf(stderr, "performed query on %s\n", searchTerms);
}

static void getUiState(struct cartJson *cj, struct hash *paramHash)
/* We haven't seen this database before, return list of all searchable stuff */
{
char *db = cartJsonRequiredParam(paramHash, "db", cj->jw, "getUiState");
cartSetString(cj->cart, "db", db);
initGenbankTableNames(db);
hashTracksAndGroups(cj->cart, db);
chromAliasSetup(db);
writeDefaultForDb(cj->jw, db);
}

static struct jsonElement *getGenomes()
/* Return a string that the javascript can use to put up a species and db select. */
{
struct jsonElement *genomesObj = newJsonObject(hashNew(0));
struct dbDbHelper *localDbs = getDbDbWithDefault();
struct dbDbHelper *temp;
struct hash *curatedHubs = hashNew(0);
for (temp = localDbs; temp != NULL; temp = temp->next)
    {
    // fill out the dbDb fields into a struct
    struct jsonElement *genomeObj = newJsonObject(hashNew(0));
    jsonObjectAdd(genomeObj, "organism", newJsonString(temp->dbDb->organism));
    jsonObjectAdd(genomeObj, "name", newJsonString(temp->dbDb->name));
    jsonObjectAdd(genomeObj, "genome", newJsonString(temp->dbDb->genome));
    jsonObjectAdd(genomeObj, "description", newJsonString(temp->dbDb->description));
    jsonObjectAdd(genomeObj, "orderKey", newJsonNumber(temp->dbDb->orderKey));
    jsonObjectAdd(genomeObj, "isDefault", newJsonBoolean(temp->isDefault));
    jsonObjectAdd(genomeObj, "isCurated", newJsonBoolean(temp->isCurated));

    if (temp->isCurated)
        hashStore(curatedHubs, temp->dbDb->name);

    // finally add the dbDb object to the hash on species, either create a new
    // list if this is the first time seeing this species or append to the end
    // of the list to keep things in the right order
    struct jsonElement *genomeList = NULL;
    if ( (genomeList = jsonFindNamedField(genomesObj, temp->dbDb->genome, temp->dbDb->genome)) != NULL)
        jsonListAdd(genomeList, genomeObj);
    else
        {
        genomeList = newJsonList(NULL);
        jsonListAdd(genomeList, genomeObj);
        jsonObjectAdd(genomesObj, temp->dbDb->genome, genomeList);
        }
    }

struct dbDb *trackHubDbs = trackHubGetDbDbs(NULL);
struct dbDb *dbDb;
for (dbDb = trackHubDbs; dbDb != NULL; dbDb = dbDb->next)
    {
    // if this is a curated hub, don't treat it like other track hubs
    if (hashLookup(curatedHubs, trackHubSkipHubName(dbDb->name)) != NULL)
        continue;

    // fill out the dbDb fields into a struct
    struct jsonElement *genomeObj = newJsonObject(hashNew(0));
    jsonObjectAdd(genomeObj, "organism", newJsonString(dbDb->organism));
    jsonObjectAdd(genomeObj, "name", newJsonString(dbDb->name));
    jsonObjectAdd(genomeObj, "genome", newJsonString(dbDb->genome));
    jsonObjectAdd(genomeObj, "description", newJsonString(dbDb->description));

    // finally add the dbDb object to the hash on species, either create a new
    // list if this is the first time seeing this species or append to the end
    // of the list to keep things in the right order
    struct jsonElement *genomeList = NULL;
    if ( (genomeList = jsonFindNamedField(genomesObj, dbDb->genome, dbDb->genome)) != NULL)
        jsonListAdd(genomeList, genomeObj);
    else
        {
        genomeList = newJsonList(NULL);
        jsonListAdd(genomeList, genomeObj);
        jsonObjectAdd(genomesObj, dbDb->genome, genomeList);
        }
    }
return genomesObj;
}

static void getChromName(struct cartJson *cj, struct hash *paramHash)
/* Check if search term is a valid chromosome name, if so return the
 * UCSC approved chromsome name for easy redirect to hgTracks/CGI. If
 * no chromosome name return chromName: null and let the user handle it */
{
// we need to connect to database explicitly to handle hub chromosomes (hs1):
char *database = NULL, *genome = NULL;
getDbAndGenome(cj->cart, &database, &genome, oldVars);
char *term = cartJsonRequiredParam(paramHash, "searchTerm", cj->jw, "getChromName");
chromAliasSetup(database);
char *chromName = NULL;

// wrap this lookup in an errCatch so we can silently return
// a null chromosome name if there was a problem. This is meant
// for quick look ups so if a term is not a chrom name it's not
// a big deal
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    chromName = hgOfficialChromName(database, term);
errCatchEnd(errCatch);

// whoever consumes this json can deal with a NULL chromName
jsonWriteString(cj->jw, "chromName", chromName);
}

/* End Handlers */

/* Start do commands, functions that dispatch */
void doSaveCategoriesToCart(struct cartJson *cj, struct hash *paramHash)
/* Save any selected categories to the cart so if the user clicks the back button
 * from hgTracks/hgTrackUi/etc we know what they were last looking at */
{
char *db = cartUsualString(cj->cart, "db", hDefaultDb());
struct jsonElement *jel = hashFindVal(paramHash, "categs");
if (jel == NULL)
    {
    jsonWriteStringf(cj->jw, "error",
		     "saveCategoriesToCart: required param 'categs' is missing");
    return;
    }
jsonElementSaveCategoriesToCart(db,jel);
}

void doCartJson()
/* Register functions that return JSON to client */
{
struct cartJson *cj = cartJsonNew(cart);
cartJsonRegisterHandler(cj, "getSearchResults", getSearchResults);
cartJsonRegisterHandler(cj, "getUiState", getUiState);
cartJsonRegisterHandler(cj, "saveCategoriesToCart", doSaveCategoriesToCart);
cartJsonRegisterHandler(cj, "getChromName", getChromName);
cartJsonExecute(cj);
}

void doMainPage()
/* Print the basic HTML page and include any necessary Javascript. AJAX calls
 * will fill out the page later */
{
char *database = NULL;
char *genome = NULL;
getDbAndGenome(cart, &database, &genome, oldVars);
webStartGbNoBanner(cart, database, "Search Disambiguation");
printMainPageIncludes();
jsInlineF("var hgsid='%s';\n", cartSessionId(cart));
struct jsonElement *cartJson = newJsonObject(hashNew(0));
jsonObjectAdd(cartJson, "db", newJsonString(database));
jsonObjectAdd(cartJson, "genomes", getGenomes());
struct dyString *cartJsonDy = dyStringNew(0);
jsonDyStringPrint(cartJsonDy, cartJson, "cartJson", -1);
jsInlineF("%s;\n", dyStringCannibalize(&cartJsonDy));
// Call our init function to fill out the page
jsInline("hgSearch.init();\n");
webEndGb();
}

void doSearchOnly()
/* Send back search results along with whatever we need to make the UI */
{
cartJsonPushErrHandlers();
char *db = NULL;
char *genome = NULL;
getDbAndGenome(cart, &db, &genome, oldVars);
char *userSearch = cartCgiUsualString(cart, "search", NULL);
if (userSearch == NULL || isEmpty(userSearch))
    {
    doMainPage();
    return;
    }
initGenbankTableNames(db);
hashTracksAndGroups(cart, db);
chromAliasSetup(db);
struct searchCategory *allCategories = getAllCategories(cart, db, hgFindGroupHash);
struct jsonElement *categsJsonElement = jsonElementFromSearchCategory(allCategories, db);

struct cartJson *cj = cartJsonNew(cart);
// since we are coming directly from hgTracks/hgGateway (or a URL manipulation),
// if the search would normally be a singlePos, like a chromosome name or HGVS search,
// we can just go directly there. But if we aren't a singlePos, we need to show
// the results page
boolean doRedirect = TRUE;
measureTiming = cartUsualBoolean(cart, "measureTiming", FALSE);
struct hgPositions *hgp = doQuery(cj->jw, db, allCategories, doRedirect, userSearch, measureTiming);
// Since we are coming from another CGI or a URL manip, go directly to hgTracks
// if we resolve to a single position
if (cartJsonIsNoWarns() && hgp && hgp->singlePos)
    {
    char newPosBuf[128];
    safef(newPosBuf, sizeof(newPosBuf), "%s:%d-%d", hgp->singlePos->chrom, hgp->singlePos->chromStart+1, hgp->singlePos->chromEnd);
    cartSetString(cj->cart, "position", newPosBuf);
    if (hgp->singlePos->highlight)
        cartSetString(cj->cart, "addHighlight", hgp->singlePos->highlight);
    char *trackName = cloneString(hgp->tableList->name);
    struct trackDb *track = NULL;
    if (!sameString(trackName, "chromInfo"))
        {
        track = tdbForTrack(db, trackName, &hgFindTdbList);
        if (!track && startsWith("all_", trackName))
            track = tdbForTrack(db, trackName+strlen("all_"), &hgFindTdbList);
        if (!track)
            errAbort("no track for table \"%s\" found via a findSpec", trackName);
        }
    if (track)
        {
        trackName = cloneString(track->track);
        }
    trackHubFixName(trackName);
    puts("Content-type:text/html\n");
    puts("<HTML>\n<HEAD>\n");
    printf("<script type='text/javascript' src='../js/utils.js'></script>\n");
    printf("<script>\n");
    // we are about to redirect back to hgTracks, save the search term onto the
    // history stack so it will appear in the dropdown of auto-suggestions before
    // redirecting
    printf("addRecentSearch(\"%s\", \"%s\", {\"label\": \"%s\", \"value\": \"%s\", \"id\": \"%s\"});\n",
            db, userSearch, userSearch, userSearch, newPosBuf);
    printf("window.location.href=\"../cgi-bin/hgTracks?");
    printf("db=%s", db);
    printf("&position=%s", newPosBuf);
    if (!sameString(trackName, "chromInfo"))
        printf("&%s=pack", trackName);
    printf("&hgFind.matches=%s", hgp->singlePos->name);
    if (track && track->parent)
        {
        if (tdbIsSuperTrackChild(track))
            printf("&%s=show", track->parent->track);
        else
            {
            // tdb is a subtrack of a composite or a view
            printf("&%s_sel=1&%s_sel=1", trackName, track->parent->track);
            }
        }
    printf("\"</script>\n");
    puts("</HEAD>\n</HTML>");
    }
else
    {
    dyStringPrintf(cj->jw->dy, ", \"db\": '%s'", db);
    dyStringPrintf(cj->jw->dy, ", \"categs\": ");
    jsonDyStringPrint(cj->jw->dy, categsJsonElement, NULL,-1);
    dyStringPrintf(cj->jw->dy, ", \"trackGroups\": ");
    jsonDyStringPrint(cj->jw->dy, makeTrackGroupsJson(db), NULL, -1);
    dyStringPrintf(cj->jw->dy, ", \"genomes\": ");
    jsonDyStringPrint(cj->jw->dy, getGenomes(), NULL, -1);

    // Now we need to actually spit out the page + json
    webStartGbNoBanner(cart, db, "Search Disambiguation");
    printMainPageIncludes();
    cartJsonPrintWarnings(cj->jw);
    jsInlineF("var hgsid='%s';\n", cartSessionId(cart));
    jsInline("var cartJson = {");
    jsInline(cj->jw->dy->string);
    jsInline("};\n");
    jsInline("hgSearch.init();\n");
    webEndGb();
    }
cartJsonPopErrHandlers();
}
/* End do commands */

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;

if (cgiOptionalString(CARTJSON_COMMAND))
    doCartJson();
else if (cgiOptionalString("search"))
    // we got here from:
    //   1. changing the URL arguments
    //   2. a back button reload
    // regardless, we can just put up the whole page with search results already
    // included in the returned json
    doSearchOnly();
else
    doMainPage();
}


/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
