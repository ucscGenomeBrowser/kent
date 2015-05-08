/* hgIntegrator - bootstrapper / back end for the Data Integrator user interface
 * This CGI has three modes of operation:
 *  - HTML output for minimal main page with a <div> container to be filled in by javascript
 *    (default, in the absence of special CGI params)
 *  - JSON responses to ajax requests from javascript (using hg/lib/cartJson.c)
 *    (if CGI param CARTJSON_COMMAND exists)
 *  - text output for annoGrator queries on track data
 *    (if CGI param DO_QUERY exists)
 * The UI view top level is in ../js/react/hgIntegrator/hgIntegrator.jsx
 * The UI model top level is in ../js/model/hgIntegrator/hgIntegratorModel.js
 */
#include "common.h"
#include "cart.h"
#include "cartJson.h"
#include "cartTrackDb.h"
#include "cheapcgi.h"
#include "hAnno.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgColors.h"
#include "hui.h"
#include "jsHelper.h"
#include "jsonParse.h"
#include "textOut.h"
#include "trackHub.h"
#include "userRegions.h"
#include "web.h"
#include "annoFormatTab.h"
#include "annoGratorQuery.h"

/* Global Variables */
struct cart *cart = NULL;             /* CGI and other variables */

#define QUERY_SPEC "hgi_querySpec"
#define DO_QUERY "hgi_doQuery"

#define hgiRegionType "hgi_range"
#define hgiRegionTypeDefault "position"

static void makeTrackLabel(struct trackDb *tdb, char *table, char *label, size_t labelSize)
/* Write tdb->shortLabel into label if table is the same as tdb->track; otherwise, write shortLabel
 * followed by table name in parens. */
{
if (sameString(table, tdb->track))
    safecpy(label, labelSize, tdb->shortLabel);
else
    safef(label, labelSize, "%s (%s)", tdb->shortLabel, table);
}

static void getFields(struct cartJson *cj, struct hash *paramHash)
/* Print out the fields of the tables in comma-sep tables param. */
{
char *tableStr = cartJsonRequiredParam(paramHash, "tables", cj->jw, "getFields");
if (! tableStr)
    return;

char *db = cartString(cart, "db");
struct slName *table, *tables = slNameListFromComma(tableStr);
jsonWriteObjectStart(cj->jw, "tableFields");
struct trackDb *fullTrackList = NULL;
struct grp *fullGroupList = NULL;
cartTrackDbInit(cj->cart, &fullTrackList, &fullGroupList, /* useAccessControl= */TRUE);
for (table = tables;  table != NULL;  table = table->next)
    {
    char *tableName = table->name;
    if (startsWith("all_", tableName))
        tableName += strlen("all_");
    struct trackDb *tdb = tdbForTrack(NULL, tableName, &fullTrackList);
    if (tdb)
        {
        struct asObject *asObj = hAnnoGetAutoSqlForTdb(db, hDefaultChrom(db), tdb);
        if (asObj)
            {
            jsonWriteObjectStart(cj->jw, table->name);
            char label[strlen(tdb->shortLabel) + strlen(table->name) + PATH_LEN];
            makeTrackLabel(tdb, table->name, label, sizeof(label));
            jsonWriteString(cj->jw, "label", label);
            jsonWriteListStart(cj->jw, "fields");
            struct asColumn *col;
            for (col = asObj->columnList;  col != NULL;  col = col->next)
                {
                jsonWriteObjectStart(cj->jw, NULL);
                jsonWriteString(cj->jw, "name", col->name);
                jsonWriteString(cj->jw, "desc", col->comment);
                jsonWriteObjectEnd(cj->jw);
                }
            jsonWriteListEnd(cj->jw);
            jsonWriteObjectEnd(cj->jw);
            }
        }
    else
        warn("No tdb for %s", table->name);
    }
jsonWriteObjectEnd(cj->jw);
slFreeList(&tables);
}

// For now at least, use hgTables' CGI var names so regions are shared between hgI & hgTables
//#*** TODO: get own CGI var names or libify these (dup'd from hgTables.h)
#define hgtaEnteredUserRegions "hgta_enteredUserRegions"
#define hgtaUserRegionsFile "hgta_userRegionsFile"
#define hgtaUserRegionsDb "hgta_userRegionsDb"
#define hgtaRegionTypeUserRegions "userRegions"


boolean userRegionsExist()
/* Return true if the trash file for regions exists.  It must be non-empty because
 * if the region list is set to empty we clear region state. */
{
char *trashFileName = cartOptionalString(cart, hgtaUserRegionsFile);
return (isNotEmpty(trashFileName) && fileExists(trashFileName));
}

struct bed4 *userRegionsGetBedList()
/* Read parsed user-defined regions from local trash file and return as bed list. */
// Not libifying at this point because the cart variable names may differ between
// apps -- in that case, libify this but with some kind of param to give cart
// var name prefix.
{
if (! userRegionsExist())
    return NULL;
char *trashFileName = cartOptionalString(cart, hgtaUserRegionsFile);
// Note: I wanted to use basicBed's bedLoadNAll but it chops by whitespace not tabs,
// so it aborts if the name field is empty (that causes it to see 3 words not 4).
char *words[4];
int wordCount;
struct lineFile *lf = lineFileOpen(trashFileName, TRUE);
struct bed4 *bedList = NULL;
while ((wordCount = lineFileChopNext(lf, words, ArraySize(words))) > 0)
    {
    lineFileExpectAtLeast(lf, 3, wordCount);
    char *name = words[3];
    if (wordCount < 4)
        name = "";
    struct bed4 *bed = bed4New(words[0], atoi(words[1]), atoi(words[2]), name);
    slAddHead(&bedList, bed);
    }
lineFileClose(&lf);
slReverse(&bedList);
return bedList;
}

static char *summarizeUserRegions()
/* Return a short summary of user-defined regions. */
// Not libifying at this point because the cart variable names may differ between
// apps -- in that case, libify this but with some kind of param to give cart
// var name prefix.
{
struct bed4 *bedList = userRegionsGetBedList();
if (bedList == NULL)
    return cloneString("no regions have been defined");
struct dyString *dy = dyStringCreate("%s:%d-%d",
                                     bedList->chrom, bedList->chromStart+1, bedList->chromEnd);
if (isNotEmpty(bedList->name))
    dyStringPrintf(dy, " (%s)", bedList->name);
int count = slCount(bedList);
if (count > 1)
    dyStringPrintf(dy, " and %d other%s", count - 1, count > 2 ? "s" : "");
return dyStringCannibalize(&dy);
}

static void getUserRegions(struct cartJson *cj, struct hash *paramHash)
/* If the cart's unparsed user regions are for the current db, return them so we
 * can show the user what they previously entered. */
// Not libifying at this point because the cart variable names may differ between
// apps -- in that case, libify this but with some kind of param to give cart
// var name prefix.
{
char *resultName = cartJsonOptionalParam(paramHash, "resultName");
if (isEmpty(resultName))
    resultName = "userRegions";
struct jsonWrite *jw = cj->jw;
char *db = cartString(cart, "db");
char *regionsDb = cartOptionalString(cart, hgtaUserRegionsDb);
if (sameOk(regionsDb, db) && userRegionsExist())
    {
    char *userRegions = cartUsualString(cart, hgtaEnteredUserRegions, "");
    //#*** TODO: move jsonStringEscape inside jsonWriteString
    char *encoded = jsonStringEscape(userRegions);
    jsonWriteString(jw, resultName, encoded);
    //#*** TODO: move jsonStringEscape inside jsonWriteString
    encoded = jsonStringEscape(summarizeUserRegions());
    jsonWriteString(jw, "userRegionsSummary", encoded);
    }
else
    {
    jsonWriteString(jw, resultName, NULL);
    jsonWriteString(jw, "userRegionsSummary", NULL);
    }
}

static void clearUserRegions(struct cartJson *cj, struct hash *paramHash)
/* Remove all user-defined region info from cart, and send JSON update. */
// Not libifying at this point because the cart variable names may differ between
// apps -- in that case, libify this but with some kind of param to give cart
// var name prefix.
{
char *resultName = cartJsonOptionalParam(paramHash, "resultName");
if (isEmpty(resultName))
    resultName = "userRegions";
struct jsonWrite *jw = cj->jw;
cartRemove(cart, hgtaUserRegionsDb);
char *trashFileName = cartOptionalString(cart, hgtaUserRegionsFile);
if (trashFileName && fileExists(trashFileName))
    unlink(trashFileName);
cartRemove(cart, hgtaUserRegionsFile);
cartRemove(cart, hgtaEnteredUserRegions);
char *regionType = cartUsualString(cart, hgiRegionType, hgiRegionTypeDefault);
if (regionType && sameString(regionType, hgtaRegionTypeUserRegions))
    {
    regionType = hgiRegionTypeDefault;
    cartSetString(cart, hgiRegionType, regionType);
    }
jsonWriteString(jw, hgiRegionType, regionType);
jsonWriteString(jw, resultName, NULL);
jsonWriteString(jw, "userRegionsSummary", NULL);
}

static void setUserRegions(struct cartJson *cj, struct hash *paramHash)
/* Instead of finding user regions in paramHash, look for them in separate CGI
 * variables and remove them from the cart.  If user regions are small enough to
 * enter in the paste box, send them to the UI model. */
// Not libifying at this point because the cart variable names may differ between
// apps -- in that case, libify this but with some kind of param to give cart
// var name prefix.
{
char *regionText = cartJsonOptionalParam(paramHash, "regions");
char *regionFileVar = cartJsonOptionalParam(paramHash, "regionFileVar");
struct jsonWrite *jw = cj->jw;
if (regionText == NULL && regionFileVar == NULL)
    {
    jsonWriteStringf(jw, "error", "setUserRegions: no param given "
                     "(must give either \"regions\" or \"regionFileVar\"");
    return;
    }
char *db = cartString(cart, "db");
// File upload takes precedence over pasted text:
if (regionFileVar != NULL)
    regionText = cgiOptionalString(regionFileVar);
if (isEmpty(regionText))
    {
    clearUserRegions(cj, paramHash);
    }
else
    {
    int regionCount = 0;
    char *warnText = "";
    char *trashFileName = userRegionsParse(db, regionText, 1000, 10, &regionCount, &warnText);
    if (trashFileName && regionCount > 0)
        {
        cartSetString(cart, hgtaUserRegionsDb, db);
        cartSetString(cart, hgtaUserRegionsFile, trashFileName);
        cartSetString(cart, hgiRegionType, hgtaRegionTypeUserRegions);
        if (strlen(regionText) > 64 * 1024)
            // Unparsed regions are too big to save for editing
            cartRemove(cart, hgtaEnteredUserRegions);
        else
            cartSetString(cart, hgtaEnteredUserRegions, cloneString(regionText));
        char *userRegions = cartOptionalString(cart, hgtaEnteredUserRegions);
        if (isNotEmpty(userRegions))
            {
            // Now that cart is updated, send JSON update
            getUserRegions(cj, paramHash);
            }
        if (warnText != NULL)
            {
            //#*** TODO: move jsonStringEscape inside jsonWriteString
            char *encoded = jsonStringEscape(warnText);
            jsonWriteString(jw, "userRegionsWarn", encoded);
            }
        }
    else
        jsonWriteStringf(jw, "error", "Could not find any regions in input: %s", warnText);
    }
if (regionFileVar)
    cartRemove(cart, regionFileVar);
}

void doCartJson()
/* Perform UI commands to update the cart and/or retrieve cart vars & metadata. */
{
// When cart is brand new, we need to set db in the cart because several cartJson functions
// require it to be there.
if (! cartOptionalString(cart, "db"))
    cartSetString(cart, "db", hDefaultDb());
struct cartJson *cj = cartJsonNew(cart);
cartJsonRegisterHandler(cj, "getFields", getFields);
cartJsonRegisterHandler(cj, "setUserRegions", setUserRegions);
cartJsonRegisterHandler(cj, "getUserRegions", getUserRegions);
cartJsonExecute(cj);
}

static struct pipeline *configTextOut(struct jsonElement *queryObj, int *pSavedStdout)
// Set up a textOut pipeline according to output file options in queryObj.
{
char *fileName = "";
char *compressType = textOutCompressNone;
struct jsonElement *outFileOptions = jsonFindNamedField(queryObj, QUERY_SPEC, "outFileOptions");
if (outFileOptions)
    {
    boolean doFile = jsonOptionalBooleanField(outFileOptions, "doFile", FALSE);
    if (doFile)
        {
        fileName = jsonOptionalStringField(outFileOptions, "fileName", "hgIntegratorResults");
        boolean doGzip = jsonOptionalBooleanField(outFileOptions, "doGzip", FALSE);
        if (doGzip)
            compressType = textOutCompressGzip;
        }
    }
return textOutInit(fileName, compressType, pSavedStdout);
}

static struct annoFormatter *makeTabFormatter(struct jsonElement *queryObj)
// Create and configure an annoFormatter subclass as specified by queryObj.
{
struct annoFormatter *tabOut = annoFormatTabNew("stdout");
// Look for fields that have been deselected by the user
struct jsonElement *outFileOptions = jsonFindNamedField(queryObj, QUERY_SPEC, "outFileOptions");
if (outFileOptions)
    {
    struct jsonElement *tableFieldsObj = jsonFindNamedField(outFileOptions, "outFileOptions",
                                                            "tableFields");
    if (tableFieldsObj)
        {
        struct hash *tableFields = jsonObjectVal(tableFieldsObj, "tableFields");
        // Iterate over names which are tables which had better end up being annoStreamer names...
        //#*** Hmmm, annoStreamer uses complete file path for big and we don't have that info
        //#*** here.  Better find a way to pass in names to streamers for consistency!
        struct hashEl *hel;
        struct hashCookie cookie = hashFirst(tableFields);
        while ((hel = hashNext(&cookie)) != NULL)
            {
            char *sourceName = hel->name;
            struct jsonElement *tableObj = hel->val;
            struct hash *fieldVals = jsonObjectVal(tableObj, sourceName);
            // Now iterate over field/column names to see which ones are explicitly deselected:
            struct hashEl *innerHel;
            struct hashCookie innerCookie = hashFirst(fieldVals);
            while ((innerHel = hashNext(&innerCookie)) != NULL)
                {
                char *colName = innerHel->name;
                struct jsonElement *enabledEl = innerHel->val;
                boolean enabled = jsonBooleanVal(enabledEl, colName);
                if (! enabled)
                    annoFormatTabSetColumnVis(tabOut, sourceName, colName, enabled);
                }
            }
        }
    }
return tabOut;
}


static void regionQuery(struct annoAssembly *assembly, struct bed4 *region,
                        struct slRef *dataSources, struct trackDb *fullTrackList,
                        struct annoFormatter *formatter)
/* Get streamers, grators & do query for region.  Wasteful but necessary until
 * streamers internally handle split tables/files (i.e. are chrom-agnostic when
 * opening). */
{
char *db = assembly->name;
struct annoStreamer *primary = NULL;
struct annoGrator *gratorList = NULL;
struct slRef *dsRef;
int i;
for (i = 0, dsRef = dataSources;  dsRef != NULL;  i++, dsRef = dsRef->next)
    {
    struct jsonElement *dsObj = dsRef->val;
    struct slRef *trackPath = jsonListVal(jsonMustFindNamedField(dsObj, "dataSource", "trackPath"),
                                          "trackPath");
    // The first item in trackPath is group.  The second is track (or composite):
    struct jsonElement *trackEl = (struct jsonElement *)(trackPath->next->val);
    // and the last item in trackPath is track or leaf subtrack.
    struct slRef *leafRef = slLastEl(trackPath);
    struct jsonElement *leafEl = (struct jsonElement *)(leafRef->val);
    char *leafTrack = jsonStringVal(leafEl, "leaf");
    char *track = jsonStringVal(trackEl, "track");
    struct trackDb *tdb = tdbForTrack(db, leafTrack, &fullTrackList);
    if (!tdb)
        tdb = tdbForTrack(db, track, &fullTrackList);
    if (!tdb)
        errAbort("doQuery: no tdb for track %s, leaf %s", track, leafTrack);
    char *table = tdb->table;
    if (i == 0)
        {
        primary = hAnnoStreamerFromTrackDb(assembly, table, tdb, region->chrom, ANNO_NO_LIMIT);
        annoStreamerSetName(primary, table);
        }
    else
        {
        struct annoGrator *grator = hAnnoGratorFromTrackDb(assembly, table, tdb, region->chrom,
                                                           ANNO_NO_LIMIT, NULL, agoNoConstraint);
        if (grator)
            {
            annoStreamerSetName((struct annoStreamer *)grator, table);
            slAddHead(&gratorList, grator);
            }
        else
            errAbort("doQuery: no grator for track %s, table %s", track, table);
        }
    }
slReverse(&gratorList);

// Set up and execute query.
struct annoGratorQuery *query = annoGratorQueryNew(assembly, primary, gratorList, formatter);
if (region->chrom != NULL)
    annoGratorQuerySetRegion(query, region->chrom, region->chromStart, region->chromEnd);
annoGratorQueryExecute(query);

//#*** SKIP THIS FOR NOW: annoGratorQueryFree(&query);
//#*** annoGratorQueryFree closes streamers, grators and formatters. In this case we
//#*** want the formatter to stay live.  Pushing handling of split tables/files down into
//#*** annoStreamers will make this unnecessary -- this won't happen in a loop, there will
//#*** be only one call to free after looping on regions.
primary->close(&primary);
struct annoStreamer *grator = (struct annoStreamer *)gratorList, *nextGrator;
for (;  grator != NULL;  grator = nextGrator)
    {
    nextGrator = grator->next;
    grator->close(&grator);
    }
}

void doQuery()
/* Execute a query that has been built up by the UI. */
{
// Make sure we have either genome-wide search or a valid position

//#*** TODO: user-defined regions
//#*** For starters, just loop the whole damn thing on regions just like the TB.
//#*** It would be better for performance to push the details of per-chrom files
//#*** or split tables down into streamers, which could then open a different
//#*** file or db table when a new minChrom is passed in or when streamer->setRegion
//#*** is called.

char *db = cartString(cart, "db");
char *regionType = cartUsualString(cart, hgiRegionType, hgiRegionTypeDefault);
struct bed4 *regionList = NULL;
if (sameString(regionType, "position"))
    {
    char *chrom = NULL;
    uint start = 0, end = 0;
    char *position = cartUsualString(cart, "position", hDefaultPos(db));
    if (! parsePosition(position, &chrom, &start, &end))
        errAbort("doQuery: Expected position to be chrom:start-end but got '%s'", position);
    AllocVar(regionList);
    regionList->chrom = cloneString(chrom);
    regionList->chromStart = start;
    regionList->chromEnd = end;
    }
else if (sameString(regionType, hgtaRegionTypeUserRegions))
    {
    regionList = userRegionsGetBedList();
    slSort(&regionList, bedCmp);
    }
else
    {
    // genome-wide query: chrom=NULL, start=end=0
    AllocVar(regionList);
    }
struct annoAssembly *assembly = hAnnoGetAssembly(db);
// Decode and parse CGI-encoded querySpec.
char *querySpec = cartString(cart, QUERY_SPEC);
int len = strlen(querySpec);
char querySpecDecoded[len+1];
cgiDecodeFull(querySpec, querySpecDecoded, len);
struct jsonElement *queryObj = jsonParse(querySpecDecoded);
// Set up output.
int savedStdout = -1;
struct pipeline *textOutPipe = configTextOut(queryObj, &savedStdout);
webStartText();
// Make an annoFormatter to print output.
// For now, tab-separated output is it.
struct annoFormatter *formatter = makeTabFormatter(queryObj);

// Unpack the query spec.
struct slRef *dataSources = jsonListVal(jsonFindNamedField(queryObj, "queryObj", "dataSources"),
                                        "dataSources");
// Get trackDb.
struct grp *fullGroupList = NULL;
struct trackDb *fullTrackList = NULL;
cartTrackDbInit(cart, &fullTrackList, &fullGroupList, TRUE);

// For now, do a complete annoGrator query for each region, rebuilding each data source
// since annoStreamers don't yet handle split tables/files internally.  For decent
// performance, it will be necessary to push split-source handling inside the streamers,
// and then all we'll need to do is make one set of streamers and then loop only on calls
// to annoGratorQuerySetRegion and annoGratorQueryExecute.
struct bed4 *region;
for (region = regionList;  region != NULL;  region = region->next)
    {
    regionQuery(assembly, region, dataSources, fullTrackList, formatter);
    }

textOutClose(&textOutPipe, &savedStdout);
}


void doMainPage()
/* Send HTML with javascript to bootstrap the user interface. */
{
char *db = cartUsualString(cart, "db", hDefaultDb());
webStartWrapperDetailedNoArgs(cart, trackHubSkipHubName(db),
                              "", "Data Integrator",
                              TRUE, FALSE, TRUE, TRUE);

// Ideally these would go in the <HEAD>
puts("<link rel=\"stylesheet\" href=\"//code.jquery.com/ui/1.10.3/themes/smoothness/jquery-ui.css\">");
puts("<link rel=\"stylesheet\" href=\"//netdna.bootstrapcdn.com/font-awesome/4.0.3/css/font-awesome.css\">");

puts("<div id=\"appContainer\">Loading...</div>");

// Set a global JS variable hgsid.
// Plain old "var ..." doesn't work (other scripts can't see it), it has to belong to window.
printf("<script>window.%s='%s';</script>\n", cartSessionVarName(), cartSessionId(cart));

jsIncludeReactLibs();
puts("<script src=\"../js/reactHgIntegrator.js\"></script>");
puts("<script src=\"../js/hgIntegratorModel.js\"></script>");

// Invisible form for submitting a query
printf("\n<form action=\"%s\" method=%s id='queryForm'>\n",
       hgIntegratorName(), cartUsualString(cart, "formMethod", "GET"));
cartSaveSession(cart);
cgiMakeHiddenVar(QUERY_SPEC, cartUsualString(cart, QUERY_SPEC, ""));
cgiMakeHiddenVar(DO_QUERY, "go");
puts("</form>");

// Invisible form for jumping to another CGI
printf("\n<form method=%s id='jumpForm'>\n", cartUsualString(cart, "formMethod", "GET"));
cartSaveSession(cart);
puts("</form>");

webEnd();
}

void doMiddle(struct cart *theCart)
/* Depending on invocation, either perform a query and print out results,
 * serve up JSON for the UI, or display the main page. */
{
cart = theCart;
if (cgiOptionalString(CARTJSON_COMMAND))
    doCartJson();
else if (cgiOptionalString(DO_QUERY))
    doQuery();
else
    doMainPage();
}

int main(int argc, char *argv[])
/* Process CGI / command line. */
{
/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {DO_QUERY, CARTJSON_COMMAND, NULL,};
struct hash *oldVars = NULL;
cgiSpoof(&argc, argv);
setUdcCacheDir();
cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
