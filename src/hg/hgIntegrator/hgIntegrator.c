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

/* Copyright (C) 2015 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "cart.h"
#include "cartJson.h"
#include "cartTrackDb.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "genbank.h"
#include "hAnno.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgColors.h"
#include "hui.h"
#include "joiner.h"
#include "jsHelper.h"
#include "jsonParse.h"
#include "knetUdc.h"
#include "textOut.h"
#include "trackHub.h"
#include "userRegions.h"
#include "web.h"
#include "annoFormatTab.h"
#include "annoGratorQuery.h"
#include "windowsToAscii.h"
#include "chromAlias.h"

/* Global Variables */
struct cart *cart = NULL;             /* CGI and other variables */
struct hash *oldVars = NULL;          /* Old contents of cart before it was updated by CGI */

#define QUERY_SPEC "hgi_querySpec"
#define UI_CHOICES "hgi_uiChoices"
#define DO_QUERY "hgi_doQuery"

#define hgiRegionType "hgi_range"
#define hgiRegionTypePosition "position"
#define hgiRegionTypeDefault hgiRegionTypePosition

static void writeCartVar(struct cartJson *cj, char *varName)
{
char *val = cartOptionalString(cj->cart, varName);
jsonWriteString(cj->jw, varName, val);
}

static void getQueryState(struct cartJson *cj, struct hash *paramHash)
/* Bundle hgi_querySpec and hgi_uiChoices because they need to be processed together. */
{
jsonWriteObjectStart(cj->jw, "queryState");
writeCartVar(cj, QUERY_SPEC);
writeCartVar(cj, UI_CHOICES);
jsonWriteObjectEnd(cj->jw);
}

static struct trackDb *getFullTrackList(struct cart *cart)
/* It can take a long time to load trackDb, hubs etc, so cache it in case multiple
 * handlers need it.
 * Callers must not modify (e.g. sort) the returned list! */
//#*** Seems like hdb should have something like this if it doesn't already.
{
static struct trackDb *fullTrackList = NULL;
static struct grp *fullGroupList = NULL;
if (fullTrackList == NULL)
    cartTrackDbInit(cart, &fullTrackList, &fullGroupList, /* useAccessControl= */TRUE);
return fullTrackList;
}

static void makeTrackLabel(struct trackDb *tdb, char *table, char *label, size_t labelSize)
/* Write tdb->shortLabel followed by table name in parens. */
{
safef(label, labelSize, "%s (%s)", tdb->shortLabel, table);
}

static struct slPair *fieldsFromAsObj(struct asObject *asObj)
/* Extract name and description from each column in autoSql. */
{
struct slPair *fieldList = NULL;
struct asColumn *col;
for (col = asObj->columnList;  col != NULL;  col = col->next)
    {
    slAddHead(&fieldList, slPairNew(col->name, cloneString(col->comment)));
    }
slReverse(&fieldList);
return fieldList;
}

static struct slPair *fieldsFromSqlFields(struct sqlConnection *conn, char *table)
/* List field names and empty descriptions. */
{
struct slPair *fieldList = NULL;
struct slName *field, *sqlFieldList = sqlListFields(conn, table);
for (field = sqlFieldList;  field != NULL;  field = field->next)
    {
    slAddHead(&fieldList, slPairNew(field->name, cloneString("")));
    }
slReverse(&fieldList);
return fieldList;
}

static void writeTableFields(struct jsonWrite *jw, char *label, char *db, char *dotTable,
                             struct slPair *fields)
/* Add a json object for dotTable containing its label and field descriptions. */
{
char *dot = strchr(dotTable, '.');
char *table = dot ? dot+1 : dotTable;
jsonWriteObjectStart(jw, dotTable);
jsonWriteString(jw, "label", label);
jsonWriteBoolean(jw, "isNoGenome", cartTrackDbIsNoGenome(db, table));
jsonWriteListStart(jw, "fields");
struct slPair *field;
for (field = fields;  field != NULL;  field = field->next)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "name", field->name);
    jsonWriteString(jw, "desc", (char *)field->val);
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
jsonWriteObjectEnd(jw);
}

static void writeTableFieldsFromAsObj(struct jsonWrite *jw, char *label, char *db, char *table,
                                      struct asObject *asObj)
/* Add a json object for a table containing its label and field descriptions from autoSql. */
{
struct slPair *fieldList = fieldsFromAsObj(asObj);
writeTableFields(jw, label, db, table, fieldList);
slPairFreeList(&fieldList);
}

static void writeTableFieldsFromDbTable(struct jsonWrite *jw, char *label,
                                        struct sqlConnection *conn, char *db, char *table)
/* Add a json object for a table containing its label and field descriptions from autoSql. */
{
struct slPair *fieldList = fieldsFromSqlFields(conn, table);
writeTableFields(jw, label, db, table, fieldList);
slPairFreeList(&fieldList);
}

static char *getRelatedTableLabel(char *db, char *table, struct trackDb *tdb)
/* Label db.table using tdb->longLabel (if available) or autoSql comment (if available),
 * Caller can free result. */
{
struct dyString *dy = dyStringCreate("%s.%s", db, table);
if (tdb != NULL)
    {
    dyStringPrintf(dy, " (%s)", tdb->longLabel);
    }
else
    {
    struct sqlConnection *conn = hAllocConn(db);
    struct asObject *asObj = asFromTableDescriptions(conn, table);
    if (asObj != NULL)
        {
        dyStringPrintf(dy, " (%s)", asObj->comment);
        asObjectFree(&asObj);
        }
    hFreeConn(&conn);
    }
return dyStringCannibalize(&dy);
}

static void getFields(struct cartJson *cj, struct hash *paramHash)
/* Print out the fields of the tables in tableGroups param.  In order to support related SQL
 * tables, the tableGroups param is ;-separated and may include ,-separated lists containing
 * the main table for a track followed by db.table's for related sql tables. */
{
char *tableGroupStr = cartJsonRequiredParam(paramHash, "tableGroups", cj->jw, "getFields");
if (! tableGroupStr)
    return;
char *jsonName = cartJsonParamDefault(paramHash, "jsonName", "tableFields");
char *cartDb = cartString(cart, "db");
struct trackDb *fullTrackList = getFullTrackList(cj->cart);
jsonWriteObjectStart(cj->jw, jsonName);
struct slName *tableGroup, *tableGroups = slNameListFromString(tableGroupStr, ';');
for (tableGroup = tableGroups;  tableGroup != NULL;  tableGroup = tableGroup->next)
    {
    struct slName *table, *tables = slNameListFromComma(tableGroup->name);
    // When there are multiple tables, the first table in list is the main table for track.
    char *mainTable = tables->name;
//#*** TODO:
//#*** The trackDb setting defaultLinkedTables should be checked too, when determining which tables
//#*** have already been selected.
    struct trackDb *mainTdb = tdbForTrack(NULL, mainTable, &fullTrackList);
    jsonWriteObjectStart(cj->jw, mainTable);
    for (table = tables;  table != NULL;  table = table->next)
        {
        // Note that the given table name can be db.table (for related sql tables); parse out.
        char db[PATH_LEN], tableName[PATH_LEN];
        hParseDbDotTable(cartDb, table->name, db, sizeof(db), tableName, sizeof(tableName));
        if (isEmpty(db))
            safecpy(db, sizeof(db), cartDb);
        struct trackDb *tdb = NULL;
        if (sameString(db, cartDb) && sameString(tableName, mainTable))
            tdb = mainTdb;
        if (tdb)
            {
            struct asObject *asObj = hAnnoGetAutoSqlForTdb(db, hDefaultChrom(db), tdb);
            if (asObj)
                {
                char label[PATH_LEN*2];
                makeTrackLabel(tdb, tableName, label, sizeof(label));
                writeTableFieldsFromAsObj(cj->jw, label, db, table->name, asObj);
                }
            else
                warn("No autoSql for track %s", table->name);
            }
        else if (trackHubDatabase(db))
            {
            warn("No tdb for track hub table %s", table->name);
            }
        else
            {
            // No tdb and not a hub track so it's a sql table, presumably related to mainTable.
            struct sqlConnection *conn = hAllocConn(db);
            char *realTable = NULL;
            struct slName *realTables = NULL;
            if (sqlTableExists(conn, tableName))
                realTable = tableName;
            else
                {
                char likeExpr[PATH_LEN];
                safef(likeExpr, sizeof(likeExpr), "chr%%\\_%s", tableName);
                realTables = sqlListTablesLike(conn, likeExpr);
                if (realTables != NULL)
                    realTable = realTables->name;
                }
            if (realTable != NULL)
                {
                struct asObject *asObj = hAnnoGetAutoSqlForDbTable(db, realTable, tdb, TRUE);
                char *label = getRelatedTableLabel(db, tableName, tdb);
                if (asObj)
                    writeTableFieldsFromAsObj(cj->jw, label, db, table->name, asObj);
                else
                    {
                    warn("No autoSql for table %s", tableName);
                    // Show sql field names, no descriptions available.
                    writeTableFieldsFromDbTable(cj->jw, label, conn, db, realTable);
                    }
                }
            else
                warn("No tdb or table for %s", tableName);
            slFreeList(&realTables);
            hFreeConn(&conn);
            }
        }
    slNameFreeList(&tables);
    jsonWriteObjectEnd(cj->jw); // mainTable ("track")
    }
jsonWriteObjectEnd(cj->jw);
slNameFreeList(&tableGroups);
}

static void addAll_PrefixForJoiner(char *table, size_t sizeofTable)
// Ugly: although we need to trim 'all_' from some table names in order to find their
// trackDb entries, all.joiner uses the all_ table names, so add it back here.
// The names to prefix are from this command:
/*
    grep all_ ~/kent/src/hg/makeDb/schema/all.joiner \
    | perl -wpe 's/^.*all_/all_/; s/\..*$//;' \
    | sort -u
*/
// and cross-checking whether the names are in trackDb (only mrna and est; the bacends,
// fosends and sts tables are auxiliary tables, not track tables).  I expect it to be stable --
// these tracks are ancient and we won't make that mistake again. :)
{
if (sameString(table, "mrna") || sameString(table, "est"))
    {
    int tableLen = strlen(table);
    int prefixLen = strlen("all_");
    if (sizeofTable > tableLen + prefixLen + 1)
        {
        memmove(table+prefixLen, table, tableLen+1);
        strncpy(table, "all_", prefixLen+1);
        }
    }
}

static int joinerDtfCmp(const void *a, const void *b)
/* Compare joinerDtf's alphabetically by database, table and (possiby NULL) field. */
{
struct joinerDtf *dtfA = *((struct joinerDtf **)a);
struct joinerDtf *dtfB = *((struct joinerDtf **)b);
int dif = strcmp(dtfA->database, dtfB->database);
if (dif == 0)
    dif = strcmp(dtfA->table, dtfB->table);
if (dif == 0)
    {
    if (dtfA->field == NULL && dtfB->field != NULL)
        dif = -1;
    else if (dtfA->field != NULL && dtfB->field == NULL)
        dif = 1;
    else
        dif = strcmp(dtfA->field, dtfB->field);
    }
return dif;
}

static struct joinerDtf *findRelatedTables(struct joiner *joiner, char *cartDb,
                                           struct slName *dbTableList)
/* Return a (usually NULL) list of tables that can be joined to something in dbTableList.
 * First item in dbTableList is the main track table. */
{
struct joinerDtf *outList = NULL;
struct hash *uniqHash = newHash(0);
char *mainTable = dbTableList->name;
struct slName *dbTable;
for (dbTable = dbTableList;  dbTable != NULL;  dbTable = dbTable->next)
    {
    char db[PATH_LEN], table[PATH_LEN];
    hParseDbDotTable(cartDb, dbTable->name, db, sizeof(db), table, sizeof(table));
    if (isEmpty(db))
        safecpy(db, sizeof(db), cartDb);
    addAll_PrefixForJoiner(table, sizeof(table));
    struct joinerPair *jp, *jpList = joinerRelate(joiner, db, table, cartDb);
    for (jp = jpList; jp != NULL; jp = jp->next)
        {
        // omit the main table from the list if some related table links back to the main table:
        boolean isMainTable = (sameString(jp->b->database, cartDb) &&
                               sameString(jp->b->table, mainTable));
        char dbDotTable[PATH_LEN];
        safef(dbDotTable, sizeof(dbDotTable), "%s.%s", jp->b->database, jp->b->table);
        if (! isMainTable && !hashLookup(uniqHash, dbDotTable) &&
            !cartTrackDbIsAccessDenied(jp->b->database, jp->b->table))
            {
            hashAdd(uniqHash, dbDotTable, NULL);
            slAddHead(&outList, joinerDtfClone(jp->b));
            }
        }
    joinerPairFreeList(&jpList);
    }
slSort(&outList, joinerDtfCmp);
hashFree(&uniqHash);
return outList;
}

static void getRelatedTables(struct cartJson *cj, struct hash *paramHash)
/* Print out related tables (if any) for each track in the tableGroups param.
 * The tableGroups param is ;-separated and may include ,-separated lists containing
 * the main table for a track followed by db.table's for related sql tables. */
{
char *tableGroupStr = cartJsonRequiredParam(paramHash, "tableGroups", cj->jw, "getRelatedTables");
if (! tableGroupStr)
    return;
char *jsonName = cartJsonParamDefault(paramHash, "jsonName", "relatedTables");
char *cartDb = cartString(cart, "db");
struct joiner *joiner = joinerRead("all.joiner");
// Even if we didn't require the track list, it would still be necessary to call cartTrackDbInit
// so that cartTrackDbIsAccessDenied can use the local static forbiddenTrackList, ugh...
// need to make that more explicit.
struct trackDb *fullTrackList = getFullTrackList(cj->cart);
jsonWriteObjectStart(cj->jw, jsonName);
struct slName *tableGroup, *tableGroups = slNameListFromString(tableGroupStr, ';');
for (tableGroup = tableGroups;  tableGroup != NULL;  tableGroup = tableGroup->next)
    {
    struct slName *dbTableList = slNameListFromComma(tableGroup->name);
    // When there are multiple tables, the first table in list is the main table for track.
    char *mainTable = dbTableList->name;
    struct joinerDtf *relatedTables = findRelatedTables(joiner, cartDb, dbTableList);
    if (relatedTables != NULL)
        {
        // Write a list of [table, description, isNoGenome] tuples
        jsonWriteListStart(cj->jw, mainTable);
        struct joinerDtf *dtf;
        for (dtf = relatedTables;  dtf != NULL;  dtf = dtf->next)
            {
            // Write one [table, description, isNoGenome] tuple
            jsonWriteListStart(cj->jw, NULL);
            // If related table is in same database as main table, make its name begin with
            // just "." so saved settings don't pull in related tables from the old database
            // when we switch to a new database.
            char relTableName[PATH_LEN];
            if (sameString(dtf->database, cartDb))
                safef(relTableName, sizeof(relTableName), ".%s", dtf->table);
            else
                safef(relTableName, sizeof(relTableName), "%s.%s", dtf->database, dtf->table);
            jsonWriteString(cj->jw, NULL, relTableName);
            struct trackDb *tdb = NULL;
            if (sameString(dtf->database, cartDb))
                tdb = tdbForTrack(cartDb, dtf->table, &fullTrackList);
            char *description = getRelatedTableLabel(dtf->database, dtf->table, tdb);
            jsonWriteString(cj->jw, NULL, description);
            jsonWriteBoolean(cj->jw, NULL, cartTrackDbIsNoGenome(dtf->database, dtf->table));
            jsonWriteListEnd(cj->jw);
            }
        jsonWriteListEnd(cj->jw);
        }
    slNameFreeList(&dbTableList);
    slNameFreeList(&relatedTables);
    }
jsonWriteObjectEnd(cj->jw);
slNameFreeList(&tableGroups);
joinerFree(&joiner);
}


// For now at least, use hgTables' CGI var names so regions are shared between hgI & hgTables
//#*** TODO: get own CGI var names or libify these (dup'd from hgTables.h)
#define hgtaEnteredUserRegions "hgta_enteredUserRegions"
#define hgtaUserRegionsFile "hgta_userRegionsFile"
#define hgtaUserRegionsDb "hgta_userRegionsDb"
#define hgtaRegionTypeUserRegions "userRegions"
#define hgtaRegionTypeGenome "genome"


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
    jsonWriteString(jw, resultName, userRegions);
    jsonWriteString(jw, "userRegionsSummary", summarizeUserRegions());
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
            jsonWriteString(jw, "userRegionsWarn", warnText);
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
char *db = cartOptionalString(cart, "db");
if (! db)
    {
    db = hDefaultDb();
    cartSetString(cart, "db", db);
    }
initGenbankTableNames(db);
struct cartJson *cj = cartJsonNew(cart);
cartJsonRegisterHandler(cj, "getQueryState", getQueryState);
cartJsonRegisterHandler(cj, "getFields", getFields);
cartJsonRegisterHandler(cj, "getRelatedTables", getRelatedTables);
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
        fileName = textOutSanitizeHttpFileName(fileName);
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
// In case the autoSql includes the bin column, turn it off.  The user can explicitly enable it
// in the UI, and in that case we'll turn it back on below.
struct slRef *dataSources = jsonListVal(jsonFindNamedField(queryObj, "queryObj", "dataSources"),
                                        "dataSources");
struct slRef *dsRef;
for (dsRef = dataSources;  dsRef != NULL;  dsRef = dsRef->next)
    {
    struct jsonElement *dsObj = dsRef->val;
    struct slRef *trackPath = jsonListVal(jsonMustFindNamedField(dsObj, "dataSource", "trackPath"),
                                          "trackPath");
    struct slRef *leafRef = slLastEl(trackPath);
    struct jsonElement *leafEl = (struct jsonElement *)(leafRef->val);
    char *sourceName = jsonStringVal(leafEl, "trackPath leaf");
    // If source's asObject doesn't have a bin column then this won't have any effect.
    annoFormatTabSetColumnVis(tabOut, sourceName, "bin", FALSE);
    }
// Look for fields that have been deselected by the user
struct jsonElement *outFileOptions = jsonFindNamedField(queryObj, QUERY_SPEC, "outFileOptions");
if (outFileOptions)
    {
    struct jsonElement *tableFieldsObj = jsonFindNamedField(outFileOptions, "outFileOptions",
                                                            "tableFields");
    if (tableFieldsObj)
        {
        struct hash *tableFields = jsonObjectVal(tableFieldsObj, "tableFields");
        // Iterate over hash keys (= tables); the same names must be passed into annoStreamers.
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
                if (!enabled || sameString(colName, "bin"))
                    annoFormatTabSetColumnVis(tabOut, sourceName, colName, enabled);
                }
            }
        }
    }
return tabOut;
}

static void filterNoGenome(char *db, boolean regionIsGenome, struct jsonElement *configEl)
/* If we are doing a genome-wide query and configEl specifies related tables, then remove
 * any related tables that appear in a 'tableBrowser noGenome' setting in trackDb. */
{
if (configEl && regionIsGenome)
    {
    struct jsonElement *relatedTablesEl = jsonFindNamedField(configEl, "config", "relatedTables");
    if (relatedTablesEl)
        {
        // relatedTables is a list of objects like { table: <[db.]table name>,
        //                                           fields: [ <field1>, <field2>, ...] }
        struct slRef *relatedTables = jsonListVal(relatedTablesEl, "relatedTables");
        struct slRef *tfRef, *tfRefNext, *newRefList = NULL;
        for (tfRef = relatedTables;  tfRef != NULL;  tfRef = tfRefNext)
            {
            tfRefNext = tfRef->next;
            struct jsonElement *tfEl = tfRef->val;
            char *dbTable = jsonStringField(tfEl, "table");
            char tfDb[PATH_LEN], tfTable[PATH_LEN];
            hParseDbDotTable(db, dbTable, tfDb, sizeof(tfDb), tfTable, sizeof(tfTable));
            if (isEmpty(tfDb))
                safecpy(tfDb, sizeof(tfDb), db);
            if (! cartTrackDbIsNoGenome(tfDb, tfTable))
                slAddHead(&newRefList, tfRef);
            }
        slReverse(&newRefList);
        struct jsonElement *newListEl = newJsonList(newRefList);
        jsonObjectAdd(configEl, "relatedTables", newListEl);
        }
    }
}

static struct trackDb *tdbForDataSource(struct jsonElement *dsObj, char *db,
                                        struct trackDb *fullTrackList)
/* Use dsObj's trackPath to find its trackDb record in fullTrackList.  abort if not found. */
{
struct slRef *trackPath = jsonListVal(jsonMustFindNamedField(dsObj, "dataSource", "trackPath"),
                                      "trackPath");
// The first item in trackPath is group.  The second is track (or composite):
struct jsonElement *trackEl = (struct jsonElement *)(trackPath->next->val);
// and the last item in trackPath is track or leaf subtrack.
struct slRef *leafRef = slLastEl(trackPath);
struct jsonElement *leafEl = (struct jsonElement *)(leafRef->val);
char *leafTrack = jsonStringVal(leafEl, "leaf");
char *topTrack = jsonStringVal(trackEl, "track");
struct trackDb *tdb = tdbForTrack(db, leafTrack, &fullTrackList);
if (!tdb)
    tdb = tdbForTrack(db, topTrack, &fullTrackList);
if (!tdb)
    errAbort("doQuery: no tdb for track %s, leaf %s", topTrack, leafTrack);
return tdb;
}

static void regionQuery(struct annoAssembly *assembly, struct bed4 *region,
                        struct slRef *dataSources, struct trackDb *fullTrackList,
                        struct annoFormatter *formatter)
/* Get streamers, grators & do query for region.  Wasteful but necessary until
 * streamers internally handle split files (i.e. are chrom-agnostic when
 * opening; we need an hg-level streamer for db table of per-chrom BAM or VCF files
 * like 1000 Genomes Variants). */
{
char *db = assembly->name;
struct annoStreamer *primary = NULL;
struct annoGrator *gratorList = NULL;
boolean regionIsGenome = isEmpty(region->chrom);
struct slRef *dsRef;
int i;
for (i = 0, dsRef = dataSources;  dsRef != NULL;  i++, dsRef = dsRef->next)
    {
    struct jsonElement *dsObj = dsRef->val;
    struct jsonElement *configEl = jsonFindNamedField(dsObj, "dataSource", "config");
    filterNoGenome(db, regionIsGenome, configEl);
    struct trackDb *tdb = tdbForDataSource(dsObj, db, fullTrackList);
    char *table = tdb->table;
    if (i == 0)
        {
        primary = hAnnoStreamerFromTrackDb(assembly, table, tdb, region->chrom, ANNO_NO_LIMIT,
                                           configEl);
        annoStreamerSetName(primary, tdb->track);
        }
    else
        {
        struct annoGrator *grator = hAnnoGratorFromTrackDb(assembly, table, tdb, region->chrom,
                                                           ANNO_NO_LIMIT, NULL, agoNoConstraint,
                                                           configEl);
        if (grator)
            {
            annoStreamerSetName((struct annoStreamer *)grator, tdb->track);
            slAddHead(&gratorList, grator);
            }
        else
            errAbort("doQuery: no grator for track %s, table %s", tdb->track, table);
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

static struct bed4 *positionToBed4(char *position)
/* Expect position to be chr:start-end; parse that and return a new BED4 with chrom, chromStart,
 * chromEnd but no name. */
{
struct bed4 *bed = NULL;
char *chrom = NULL;
uint start = 0, end = 0;
if (! parsePosition(position, &chrom, &start, &end))
    errAbort("doQuery: Expected position to be chrom:start-end but got '%s'", position);
AllocVar(bed);
bed->chrom = cloneString(chrom);
bed->chromStart = start;
bed->chromEnd = end;
return bed;
}

static boolean hasTBNoGenome(struct slRef *dataSources, char *db, struct trackDb *fullTrackList)
/* Return TRUE if any dataSource has the 'tableBrowser noGenome' trackDb setting. */
{
boolean foundNoGenome = FALSE;
struct slRef *dsRef;
for (dsRef = dataSources;  dsRef != NULL;  dsRef = dsRef->next)
    {
    struct jsonElement *dsObj = dsRef->val;
    struct trackDb *tdb = tdbForDataSource(dsObj, db, fullTrackList);
    char *setting = tdb ? trackDbSetting(tdb, "tableBrowser") : NULL;
    if (setting && startsWithWord("noGenome", setting))
        {
        foundNoGenome = TRUE;
        break;
        }
    }
return foundNoGenome;
}

static struct bed4 *getRegionList(char *db, struct slRef *dataSources,
                                  struct trackDb *fullTrackList,
                                  char *retRegionDesc, size_t retRegionDescSize)
/* Return a bed or list of bed: one bed for position range, possible multiple beds for
 * user-defined regions, and for genome-wide search, a bed with chrom=NULL, start=end=0.
 * If genome-wide search is specified but one of the dataSources has tdb setting
 * 'tableBrowser noGenome', force region to position range.  Put a human-readable
 * description of the region(s) in retRegionDesc. */
{
struct bed4 *regionList = NULL;
char *regionType = cartUsualString(cart, hgiRegionType, hgiRegionTypeDefault);
if (sameString(regionType, hgiRegionTypePosition) ||
    (sameString(regionType, hgtaRegionTypeGenome) && hasTBNoGenome(dataSources, db, fullTrackList)))
    {
    char *position = windowsToAscii(cartUsualString(cart, "position", hDefaultPos(db)));
    regionList = positionToBed4(position);
    safef(retRegionDesc, retRegionDescSize, "%s:%d-%d",
          regionList->chrom, regionList->chromStart+1, regionList->chromEnd);
    }
else if (sameString(regionType, hgtaRegionTypeUserRegions))
    {
    regionList = userRegionsGetBedList();
    slSort(&regionList, bedCmp);
    safecpy(retRegionDesc, retRegionDescSize, "defined-regions");
    }
else if (sameString(regionType, hgtaRegionTypeGenome))
    {
    // genome-wide query: chrom=NULL, start=end=0
    AllocVar(regionList);
    safecpy(retRegionDesc, retRegionDescSize, "genome");
    }
else
    errAbort("Unrecognized region type '%s'", regionType);
return regionList;
}

void doQuery()
/* Execute a query that has been built up by the UI. */
{
// Make sure we have either genome-wide search or a valid position

//#*** TODO: improve user-defined regions:
//#*** For starters, just loop the whole damn thing on regions just like the TB.
//#*** It would be better for performance to push the details of per-chrom files
//#*** or split tables down into streamers, which could then open a different
//#*** file or db table when a new minChrom is passed in or when streamer->setRegion
//#*** is called.

// Decode and parse CGI-encoded querySpec.
char *querySpec = cartString(cart, QUERY_SPEC);
int len = strlen(querySpec);
char querySpecDecoded[len+1];
cgiDecodeFull(querySpec, querySpecDecoded, len);
struct jsonElement *queryObj = jsonParse(querySpecDecoded);
struct slRef *dataSources = jsonListVal(jsonFindNamedField(queryObj, "queryObj", "dataSources"),
                                        "dataSources");
// Set up output.
int savedStdout = -1;
struct pipeline *textOutPipe = configTextOut(queryObj, &savedStdout);
webStartText();

// Get trackDb, assembly and regionList.
struct trackDb *fullTrackList = getFullTrackList(cart);
char *db = cartString(cart, "db");
initGenbankTableNames(db);
struct annoAssembly *assembly = hAnnoGetAssembly(db);
char regionDesc[PATH_LEN];
struct bed4 *regionList = getRegionList(db, dataSources, fullTrackList,
                                        regionDesc, sizeof(regionDesc));
// Print a simple output header
time_t now = time(NULL);
printf("# hgIntegrator: database=%s region=%s %s", db, regionDesc, ctime(&now));
// Make an annoFormatter to print output.
// For now, tab-separated output is it.
struct annoFormatter *formatter = makeTabFormatter(queryObj);

// For now, do a complete annoGrator query for each region, rebuilding each data source
// since annoStreamers don't yet handle split tables/files internally.  For decent
// performance, it will be necessary to push split-source handling inside the streamers,
// and then all we'll need to do is make one set of streamers and then loop only on calls
// to annoGratorQuerySetRegion and annoGratorQueryExecute.
boolean userDefinedRegions = sameString(hgtaRegionTypeUserRegions,
                                        cartUsualString(cart, hgiRegionType, hgiRegionTypeDefault));
struct bed4 *region;
for (region = regionList;  region != NULL;  region = region->next)
    {
    if (userDefinedRegions)
        printf("# region=%s:%d-%d\n",
               region->chrom, region->chromStart+1, region->chromEnd);
    regionQuery(assembly, region, dataSources, fullTrackList, formatter);
    }

textOutClose(&textOutPipe, &savedStdout);
}


void doMainPage()
/* Send HTML with javascript to bootstrap the user interface. */
{
char *db = NULL, *genome = NULL, *clade = NULL;
getDbGenomeClade(cart, &db, &genome, &clade, oldVars);
chromAliasSetup(db);
char *position = windowsToAscii(cartUsualString(cart, "position", hDefaultPos(db)));
cartSetLastPosition(cart, position, oldVars);
initGenbankTableNames(db);
webStartWrapperDetailedNoArgs(cart, trackHubSkipHubName(db),
                              "", "Data Integrator",
                              TRUE, FALSE, TRUE, TRUE);

// Ideally these would go in the <HEAD>
puts("<link rel=\"stylesheet\" href=\"//code.jquery.com/ui/1.10.3/themes/smoothness/jquery-ui.css\">");
puts("<link rel=\"stylesheet\" href=\"//netdna.bootstrapcdn.com/font-awesome/4.0.3/css/font-awesome.css\">");

puts("<div id=\"appContainer\">Loading...</div>");

// Set a global JS variable hgsid.
// Plain old "var ..." doesn't work (other scripts can't see it), it has to belong to window.
char javascript[1024];
safef(javascript, sizeof javascript,
    "window.%s='%s';\n", cartSessionVarName(), cartSessionId(cart));
// jsInline(javascript);  // GALT TODO would prefer inline, but lack of global early causes issues.
printf("<script type='text/javascript' nonce='%s'>\n%s</script>\n", getNonce(), javascript);

jsIncludeReactLibs();
jsIncludeFile("reactHgIntegrator.js", NULL);
jsIncludeFile("hgIntegratorModel.js", NULL);
jsIncludeFile("autocompleteCat.js", NULL);

// Invisible form for submitting a query
printf("\n<form action=\"%s\" method=%s id='queryForm'>\n",
       hgIntegratorName(), cartUsualString(cart, "formMethod", "POST"));
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

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

// Try to deal with virt chrom position used by hgTracks.
if (startsWith(    MULTI_REGION_CHROM, cartUsualString(cart, "position", ""))
 || startsWith(OLD_MULTI_REGION_CHROM, cartUsualString(cart, "position", "")))
    cartSetString(cart, "position", cartUsualString(cart, "nonVirtPosition", ""));

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
long enteredMainTime = clock1000();
/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {DO_QUERY, CARTJSON_COMMAND, NULL,};
cgiSpoof(&argc, argv);
oldVars = hashNew(10);
cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgIntegrator", enteredMainTime);
return 0;
}
