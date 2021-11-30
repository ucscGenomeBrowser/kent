/* searchExample - Example search page using sphinx. */
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

// name of the searchBar form variable from the HTML
#define SEARCH_TERM_VAR "searchString"
#define SEARCH_LIMIT 100

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;
static struct hash *hubLabelHash = NULL;
boolean measureTiming = FALSE;

struct trackDb *tdbList = NULL;
struct grp *grpList = NULL;
struct hash *trackHash = NULL;
struct hash *groupHash = NULL;

/* struct hubLabel: a helper struct for making links to hubs in the search result list */
struct hubLabel
    {
    char *shortLabel;
    char *longLabel;
    char *hubId;
    };

/* Helper functions for cart handlers */

static void getLabelsForHubs()
/* Hash up the shortLabels, longLabels and hub_id for a hubUrl */
{
if (hubLabelHash != NULL)
    return;
hubLabelHash = hashNew(0);
struct sqlConnection *conn = hConnectCentral();
char **row;
struct sqlResult *sr;
char query[2048];
sqlSafef(query, sizeof(query), "select hp.hubUrl, hp.shortLabel, hp.longLabel, concat('hub_',id) from hubPublic hp join hubStatus hs on hp.hubUrl=hs.hubUrl");
sr = sqlGetResult(conn, query);
while ( (row = sqlNextRow(sr)) != NULL)
    {
    struct hubLabel *label = NULL;
    AllocVar(label);
    label->shortLabel = cloneString(row[1]);
    label->longLabel = cloneString(row[2]);
    label->hubId = cloneString(row[3]);
    char *hubUrl = cloneString(row[0]);
    hashAdd(hubLabelHash, hubUrl, label);
    }
hDisconnectCentral(&conn);
}

static struct hubLabel *getLabelForHub(char *hubUrl)
/* Look up the shortLabel, longLabel, and hub_id for a hubUrl */
{
if (!hubLabelHash)
    getLabelsForHubs();
return (struct hubLabel *)hashFindVal(hubLabelHash, hubUrl);
}

static struct trackDb *addSupers(struct trackDb *trackList)
/* Insert supertracks into the hierarchy. */
{
struct trackDb *newList = NULL;
struct trackDb *tdb, *nextTdb;
struct hash *superHash = newHash(5);

for(tdb = trackList; tdb;  tdb = nextTdb)
    {
    nextTdb = tdb->next;
    if (tdb->parent)
        {
        // part of a super track
        if (hashLookup(superHash, tdb->parent->track) == NULL)
            {
            hashStore(superHash, tdb->parent->track);

            slAddHead(&newList, tdb->parent);
            }
        slAddTail(&tdb->parent->subtracks, tdb);
        }
    else
        slAddHead(&newList, tdb);
    }
slReverse(&newList);
return newList;
}

void hashTdbNames(struct trackDb *tdb, struct hash *trackHash)
/* Store the track names for lookup, except for knownGene which gets its own special
 * category in the UI */
{
struct trackDb *tmp;
for (tmp = tdb; tmp != NULL; tmp = tmp->next)
    {
    if (tmp->subtracks)
        hashTdbNames(tmp->subtracks, trackHash);
    if (!sameString(tmp->table, tmp->track))
        hashAdd(trackHash, tmp->track, tmp);
    hashAdd(trackHash, tmp->table, tmp);
    }
}

void hashTracksAndGroups(struct cart *cart, char *db)
/* get the list of tracks available for this assembly, along with their group names
 * and visibility-ness. Note that this implicitly makes connected hubs show up
 * in the trackList struct, which means we get item search for connected
 * hubs for free */
{
if (tdbList != NULL && grpList != NULL)
    return;

if (!trackHash)
    trackHash = hashNew(0);
if (!groupHash)
    groupHash = hashNew(0);
cartTrackDbInit(cart, &tdbList, &grpList, FALSE);
if (!tdbList)
    errAbort("Error getting tracks for %s", db);
if (!grpList)
    errAbort("Error getting groups for %s", db);
struct trackDb *superList = addSupers(tdbList);
tdbList = superList;
hashTdbNames(superList, trackHash);
struct grp *g;
for (g = grpList; g != NULL; g = g->next)
    if (!sameString(g->name, "allTracks") && !sameString(g->name, "allTables"))
        hashStore(groupHash, g->name);
}

boolean doTrixQuery(struct searchCategory *category, char *searchTerm, struct hgPositions *hgp, char *database, boolean measureTiming)
/* Get a trix search result and snippets for a trix index.
 * TODO: return an error message if there is a problem with the trix index or snippet index */
{
long startTime = clock1000();
boolean ret = FALSE;
char *lowered = cloneString(searchTerm);
char *keyWords[16];
int keyCount;
tolowers(lowered);
keyCount = chopLine(lowered, keyWords);
// TODO: let the user control this:
int maxReturn = SEARCH_LIMIT;
struct trixSearchResult *tsrList = NULL;
if (category->trix)
    tsrList = trixSearch(category->trix, keyCount, keyWords, tsmExpand);
struct trixSearchResult *dbTsrList = NULL;
if (sameString(category->name, "publicHubs"))
    {
    int len = 0;
    struct trixSearchResult *tsr, *next;
    for (tsr = tsrList; tsr != NULL; tsr = next)
        {
        next = tsr->next;
        char *itemId[5];
        int numItems = chopString(cloneString(tsr->itemId), ":", itemId, ArraySize(itemId));
        if (numItems <= 2 || isEmpty(itemId[2]) || !sameString(itemId[2], database))
            continue;
        else
            {
            slAddHead(&dbTsrList, tsr);
            len++;
            }
        if (len > SEARCH_LIMIT)
            break;
        }
    }
else
    dbTsrList = tsrList;

if (slCount(dbTsrList) > SEARCH_LIMIT)
    {
    struct trixSearchResult *tsr = slElementFromIx(dbTsrList, maxReturn-1);
    tsr->next = NULL;
    }
if (dbTsrList != NULL)
    {
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        addSnippetsToSearchResults(dbTsrList, category->trix);
    errCatchEnd(errCatch);
    // silently return if there was a problem
    if (errCatch->gotError || errCatch->gotWarning)
        return FALSE;
    errCatchFree(&errCatch);
    struct trixSearchResult *tsr = NULL;
    struct hgPosTable *table = NULL;
    AllocVar(table);
    table->name = category->name;
    table->description = category->description;
    slReverse(&hgp->tableList);
    slAddHead(&hgp->tableList, table);
    slReverse(&hgp->tableList);
    for (tsr = dbTsrList; tsr != NULL; tsr = tsr->next)
        {
        struct hgPos *this = NULL;
        AllocVar(this);
        this->name = tsr->itemId;
        this->description = tsr->snippet;
        if (startsWith(category->name, "trackDb"))
            {
            struct trackDb *tdb = (struct trackDb *)hashFindVal(trackHash, this->name);
            struct dyString *tdbLabels = dyStringNew(0);
            dyStringPrintf(tdbLabels, "%s:%s:%s", tsr->itemId, tdb->shortLabel, tdb->longLabel);
            this->name = dyStringCannibalize(&tdbLabels);
            }
        if (sameString(category->name, "publicHubs"))
            {
            char *itemId[5];
            int numItems = chopString(tsr->itemId, ":", itemId, ArraySize(itemId));
            struct dyString *hubLabel = dyStringNew(0);
            char hubUrl[PATH_LEN];
            safef(hubUrl, sizeof(hubUrl), "%s:%s", itemId[0], itemId[1]);
            struct hubLabel *label = getLabelForHub(hubUrl);
            if (!label)
                continue;
            char *db = "";
            struct dyString *track = dyStringNew(0);
            if (numItems > 2)
                db = itemId[2] != NULL ? itemId[2] : "";
            if (numItems > 3)
                dyStringPrintf(track, "%s_%s", label->hubId, itemId[3]);
            dyStringPrintf(hubLabel, "%s:%s:%s:%s:%s", hubUrl, db, dyStringCannibalize(&track), label->shortLabel, label->longLabel);
            this->name = dyStringCannibalize(&hubLabel);
            }
        slAddHead(&table->posList, this);
        }
    slReverse(&table->posList);
    if (measureTiming)
        table->searchTime = clock1000() - startTime;
    ret = TRUE;
    }
return ret;
}

boolean addHubsAndDocsToHgp(struct hgPositions *hgp, struct searchCategory *categoryList, char *searchTerm, char *database, boolean measureTiming)
/* Go out and get any results from the public hubs search or help docs search
 * and make a fake hgp for them */
{
struct searchCategory *category= NULL;
boolean found = FALSE;
getLabelsForHubs();
for (category = categoryList; category != NULL; category = category->next)
    found |= doTrixQuery(category, searchTerm, hgp, database, measureTiming);
return found;
}

void doQuery(struct jsonWrite *jw, char *db, struct searchCategory *categories, char *searchTerms, boolean measureTiming)
/* Fire off a query. If the query results in a single position and we came from another CGI,
 * redirect right back to that CGI (except if we came from hgGateway redirect to hgTracks),
 * otherwise return the position list as JSON */
{
struct hgPositions *hgp = NULL;
struct searchCategory *category, *trackCategories = NULL, *nonTrackCategories = NULL;
struct searchCategory *next = NULL;
for (category = categories; category != NULL; category = next)
    {
    next = category->next;
    if (!startsWith("trackDb", category->id) && !sameString(category->id, "helpDocs") && !sameString(category->id, "publicHubs"))
        slAddHead(&trackCategories, category);
    else
        slAddHead(&nonTrackCategories, category);
    }
if (trackCategories)
    hgp = hgPositionsFind(db, searchTerms, "", "searchExample", cart, FALSE, measureTiming, trackCategories);
if (nonTrackCategories)
    {
    if (!hgp)
        AllocVar(hgp);
    (void)addHubsAndDocsToHgp(hgp, nonTrackCategories, searchTerms, db, measureTiming);
    }
if (hgp)
    {
    // check if user entered a plain position range, in which case we have to write the
    // json ourselves
    if (hgp->singlePos && hgp->posCount == 1 && hgp->tableList != NULL &&
            sameString(hgp->tableList->name, "chromInfo"))
        {
        jsonWriteListStart(jw, "positionMatches");
        jsonWriteObjectStart(jw, NULL);
        jsonWriteString(jw, "name", "chromInfo");
        jsonWriteString(jw, "chrom", hgp->singlePos->chrom);
        jsonWriteNumber(jw, "chromStart", hgp->singlePos->chromStart);
        jsonWriteNumber(jw, "chromEnd", hgp->singlePos->chromEnd);
        jsonWriteObjectEnd(jw);
        jsonWriteListEnd(jw);
        }
    else
        hgPositionsJson(jw, db, hgp, cart);
    }
}

//static struct searchCategory *getCategsFromResults(struct jsonElement *results, char *db)
/* From the list of search results, figure out what categories were
 * included, and make them default selected */
/*
{
// first parse the results for all the trackNames:
struct hash *searchedCategs = hashNew(0);
struct hash *wrapperObj = results->val.jeHash;
struct slRef *result, *resultList = ((struct jsonElement *)hashFindVal(wrapperObj, "positionMatches"))->val.jeList;
for (result = resultList; result != NULL; result = result->next)
    {
    struct jsonElement *resultObject = (struct jsonElement *)result->val;
    char *categName = jsonStringField(resultObject, "trackName");
    if (categName)
        hashStoreName(searchedCategs, categName);
    }

// now change the visibility-ness in the final categ list from what
// we already searched:
struct searchCategory *categ, *ret = getCategsForDatabase(cart, db, trackHash, groupHash);
for (categ = ret; categ != NULL; categ = categ->next)
    {
    if (hashLookup(searchedCategs, categ->id) != NULL)
        categ->visibility = 1;
    }
struct searchCategory *next, *nonTrackCategs = getCategsForNonDb(cart, db, trackHash, groupHash);
for (categ = nonTrackCategs; categ != NULL; categ = next)
    {
    next = categ->next;
    categ->next = NULL;
    if (hashLookup(searchedCategs, categ->id) != NULL)
        categ->visibility = 1;
    else
        categ->visibility = 0;
    slAddHead(&ret, categ);
    }
return ret;
}
*/

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
if (startsWith("publicHubs", categ->id) ||
        startsWith("helpDocs", categ->id) ||
        startsWith("trackDb", categ->id) ||
        startsWith("knownGene", categ->id))
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
for (grp = grpList; grp != NULL; grp = grp->next)
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
jsonObjectAdd(trackContainerEle, "priority", newJsonDouble(5.0));
jsonObjectAdd(trackContainerEle, "description", newJsonString("Search for track data items"));
jsonListAdd(nonTrackList, trackContainerEle);
return nonTrackList;
}

int cmpCategories(const void *a, const void *b)
/* Compare two categories for uniquifying */
{
struct searchCategory *categA = *(struct searchCategory **)a;
struct searchCategory *categB = *(struct searchCategory **)b;
return strcmp(categA->id, categB->id);
}

struct searchCategory *makeCategsFromJson(struct jsonElement *searchCategs, char *db)
/* User has selected some categories, parse the JSON into a struct searchCategory */
{
if (searchCategs == NULL)
    return NULL;
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
        category = makeCategory(cart, categName, NULL, db, trackHash, groupHash);
    else
        {
        struct hashEl *hel, *helList = hashElListHash(trackHash);
        for (hel = helList; hel != NULL; hel = hel->next)
            {
            struct trackDb *tdb = hel->val;
            if (!sameString(tdb->track, "knownGene") && !sameString(tdb->table, "knownGene"))
                slAddHead(&category, makeCategory(cart, tdb->track, NULL, db, trackHash, groupHash));
            }
        }
    if (category != NULL)
        {
        if (ret)
            slCat(&ret, category);
        else
            ret = category;
        }
    }
// the trackHash can contain both a composite track (where makeCategory would make categories
// for each of the subtracks, and a subtrack, where makeCategory just makes a single
// category, which means our final list can contain duplicate categories, so do a
// uniqify here so we only have one category for each category
slUniqify(&ret, cmpCategories, searchCategoryFree);
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
struct searchCategory *defaultCategories = getAllCategories(cart, database, trackHash, groupHash);
struct jsonElement *categsJsonElement = jsonElementFromSearchCategory(defaultCategories, database);
struct jsonElement *selectedCategsJsonList = jsonElementFromVisibleCategs(defaultCategories);
jsonElementSaveCategoriesToCart(database,selectedCategsJsonList);
dyStringPrintf(jw->dy, ", \"categs\": ");
jsonDyStringPrint(jw->dy, categsJsonElement, NULL, -1);
dyStringPrintf(jw->dy, ", \"trackGroups\": ");
jsonDyStringPrint(jw->dy, makeTrackGroupsJson(database), NULL, -1);
}

void printMainPageIncludes()
{
webIncludeResourceFile("gb.css");
webIncludeResourceFile("gbStatic.css");
webIncludeResourceFile("spectrum.min.css");
webIncludeResourceFile("hgGtexTrackSettings.css");
puts("<link rel='stylesheet' href='https://code.jquery.com/ui/1.10.3/themes/smoothness/jquery-ui.css'>");
puts("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/themes/default/style.min.css' />");
puts("<script src='https://cdnjs.cloudflare.com/ajax/libs/jquery/1.12.1/jquery.min.js'></script>");
puts("<script src=\"//code.jquery.com/ui/1.10.3/jquery-ui.min.js\"></script>");
puts("<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.7/jstree.min.js\"></script>\n");
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("lodash.3.10.0.compat.min.js", NULL);
jsIncludeFile("cart.js", NULL);
jsIncludeFile("searchExample.js", NULL);

// Write the skeleton HTML, which will get filled out by the javascript
webIncludeFile("inc/searchExample.html");
}

/* End handler helper functions */

/* Handlers for returning JSON to client */
static void getSearchResults(struct cartJson *cj, struct hash *paramHash)
/* User has entered a term to search, search this term against the selected categories */
{
char *db = cartUsualString(cj->cart, "db", hDefaultDb());
initGenbankTableNames(db);
hashTracksAndGroups(cj->cart, db);
char *searchTerms = cartJsonRequiredParam(paramHash, SEARCH_TERM_VAR, cj->jw, "getSearchResults");
measureTiming = cartUsualBoolean(cj->cart, "measureTiming", FALSE);
struct jsonElement *searchCategs = hashFindVal(paramHash, "categs");
struct searchCategory *searchCategoryList = makeCategsFromJson(searchCategs, db);
doQuery(cj->jw, db, searchCategoryList, searchTerms, measureTiming);
fprintf(stderr, "performed query on %s\n", searchTerms);
}

static void getUiState(struct cartJson *cj, struct hash *paramHash)
/* We haven't seen this database before, return list of all searchable stuff */
{
char *db = cartUsualString(cj->cart, "db", hDefaultDb());
initGenbankTableNames(db);
hashTracksAndGroups(cj->cart, db);
writeDefaultForDb(cj->jw, db);
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
// Call our init function
jsInlineF("var hgsid='%s';\n", cartSessionId(cart));
jsInline("searchExample.init();\n");
webEndGb();
}

void doSearchOnly()
/* Send back search results specified in the CGI arguments */
{
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
struct searchCategory *allCategories = getAllCategories(cart, db, trackHash, groupHash);
struct searchCategory *defaultCategories = getVisibleCategories(allCategories);
struct jsonElement *categsJsonElement = jsonElementFromSearchCategory(allCategories, db);
struct jsonElement *selectedCategsJsonList = jsonElementFromVisibleCategs(defaultCategories);

struct cartJson *cj = cartJsonNew(cart);
jsonElementSaveCategoriesToCart(db,selectedCategsJsonList);
dyStringPrintf(cj->jw->dy, "\"categs\": ");
jsonDyStringPrint(cj->jw->dy, categsJsonElement, NULL,-1);
dyStringPrintf(cj->jw->dy, ", \"trackGroups\": ");
jsonDyStringPrint(cj->jw->dy, makeTrackGroupsJson(db), NULL, -1);
dyStringPrintf(cj->jw->dy, ", ");

measureTiming = cartUsualBoolean(cart, "measureTiming", FALSE);
doQuery(cj->jw, db, defaultCategories, userSearch, measureTiming);
// Now we need to actually spit out the page + json
webStartGbNoBanner(cart, db, "Search Disambiguation");
printMainPageIncludes();
jsInlineF("var hgsid='%s';\n", cartSessionId(cart));
jsInline("var cartJson = {");
jsInline(cj->jw->dy->string);
jsInline("};\n");
jsInline("searchExample.init();\n");
webEndGb();
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
    // included in the returned HTML
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
