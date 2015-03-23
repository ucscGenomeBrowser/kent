/* hgIntegrator - bootstrapper / back end for the Annotation Integrator user interface
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
#include "web.h"
#include "annoFormatTab.h"
#include "annoGratorQuery.h"

/* Global Variables */
struct cart *cart = NULL;             /* CGI and other variables */

#define QUERY_SPEC "hgi_querySpec"
#define DO_QUERY "hgi_doQuery"

//#*** duplicated from hgVai... put in some anno*.h?
#define NO_MAXROWS 0

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
                jsonWriteString(cj->jw, NULL, col->name);
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

void doCartJson()
/* Perform UI commands to update the cart and/or retrieve cart vars & metadata. */
{
// When cart is brand new, we need to set db in the cart because several cartJson functions
// require it to be there.
if (! cartOptionalString(cart, "db"))
    cartSetString(cart, "db", hDefaultDb());
struct cartJson *cj = cartJsonNew(cart);
cartJsonRegisterHandler(cj, "getFields", getFields);
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


void doQuery()
/* Execute a query that has been built up by the UI. */
{
// Make sure we have either genome-wide search or a valid position
char *db = cartString(cart, "db");
char *chrom = NULL;
uint start = 0, end = 0;
char *regionType = cartUsualString(cart, "hgi_range", "position");
if (sameString(regionType, "position"))
    {
    char *position = cartUsualString(cart, "position", hDefaultPos(db));
    if (! parsePosition(position, &chrom, &start, &end))
        errAbort("doQuery: Expected position to be chrom:start-end but got '%s'", position);
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
// Build annoGrator query.
struct slRef *dataSources = jsonListVal(jsonFindNamedField(queryObj, "queryObj", "dataSources"),
                                        "dataSources");
struct grp *fullGroupList = NULL;
struct trackDb *fullTrackList = NULL;
cartTrackDbInit(cart, &fullTrackList, &fullGroupList, TRUE);
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
        primary = hAnnoStreamerFromTrackDb(assembly, table, tdb, chrom, NO_MAXROWS);
        annoStreamerSetName(primary, table);
        }
    else
        {
        struct annoGrator *grator = hAnnoGratorFromTrackDb(assembly, table, tdb, chrom,
                                                           NO_MAXROWS, NULL, agoNoConstraint);
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

// Make an annoFormatter to print output.
// For now, tab-separated output is it.
struct annoFormatter *formatter = makeTabFormatter(queryObj);

// Set up and execute query.
struct annoGratorQuery *query = annoGratorQueryNew(assembly, primary, gratorList, formatter);
if (chrom != NULL)
    annoGratorQuerySetRegion(query, chrom, start, end);
annoGratorQueryExecute(query);
annoGratorQueryFree(&query);

textOutClose(&textOutPipe, &savedStdout);
}


void doMainPage()
/* Send HTML with javascript to bootstrap the user interface. */
{
char *db = cartUsualString(cart, "db", hDefaultDb());
webStartWrapperDetailedNoArgs(cart, trackHubSkipHubName(db),
                              "", "Annotation Integrator",
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
