/* gencodeTracks - ENCODE GENCODE Genes tracks for both pilot and production ENCODE.
 * although these are used fundamentally different approaches to display */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "gencodeTracksCommon.h"
#include "hgTracks.h"
#include "hdb.h"
#include "gencodeIntron.h"
#include "genePredReader.h"
#include "genePred.h"
#include "encode/wgEncodeGencodeAttrs.h"

/* item label symbolic names and constants.  This must be
 * in sync with lib/hui.c:gencodeLabelControls() */
enum
/* bit set of item labels */
{
    ITEM_LABEL_GENE_NAME = 0x01,
    ITEM_LABEL_GENE_ID = 0x02,
    ITEM_LABEL_TRANSCRIPT_ID = 0x04
};

static struct
/* label names to constant */
{
    char *name;
    unsigned flag;
} itemLabelCartVarNamesMap[] = {
    {"geneName", ITEM_LABEL_GENE_NAME},
    {"geneId", ITEM_LABEL_GENE_ID},
    {"transcriptId", ITEM_LABEL_TRANSCRIPT_ID},
    {NULL, 0}
};


/* function type that returns filter or highlight types */
typedef filterBy_t* (*filterBySetGetFuncType)(struct trackDb *tdb, struct cart *cart, char *name);

struct gencodeQuery
/* Structure used to store information about the query being assembled.
 * Joins are for the purpose of filtering results, and in some cases, returning
 * additional information. */
{
    struct dyString *fields;                // select fields
    struct dyString *from;                  // from clause
    struct dyString *where;                 // where clause
    int genePredNumColumns;                 // number of genePred columns returned
    int attrsNumColumns;                    // number of attr columns returned
    boolean isGenePredX;                    // does this have the extended fields?
    boolean isFiltered;                     // are there filters on the query?
    boolean joinAttrs;                      // join the wgEncodeGencodeAttrs table
    boolean joinSupportLevel;               // join the wgEncodeGencodeTranscriptionSupportLevel table
    boolean joinTranscriptSource;           // join the wgEncodeGencodeTranscriptSource table
    boolean joinTag;                        // join the wgEncodeGencodeTag table (one to many, so filtering only)
};

static struct gencodeQuery *gencodeQueryNew(void)
/* construct a new gencodeQuery object */
{
struct gencodeQuery *gencodeQuery;
AllocVar(gencodeQuery);
gencodeQuery->fields = dyStringNew(0);
gencodeQuery->from = dyStringNew(0);
gencodeQuery->where = dyStringNew(0);
return gencodeQuery;
}

static void gencodeQueryFree(struct gencodeQuery **gencodeQueryPtr)
/* construct a new gencodeQuery object */
{
struct gencodeQuery *gencodeQuery = *gencodeQueryPtr;
if (gencodeQuery != NULL)
    {
    dyStringFree(&gencodeQuery->fields);
    dyStringFree(&gencodeQuery->from);
    dyStringFree(&gencodeQuery->where);
    freeMem(gencodeQuery);
    *gencodeQueryPtr = NULL;
    }
}

static void gencodeQueryBeginSubWhere(struct gencodeQuery *gencodeQuery)
/* begin adding new where sub-clause */
{
if (dyStringLen(gencodeQuery->where) > 0)
    sqlDyStringPrintf(gencodeQuery->where, " and ");
sqlDyStringPrintf(gencodeQuery->where, "(");
}

static void gencodeQueryEndSubWhere(struct gencodeQuery *gencodeQuery)
/* finish adding new where sub-clause */
{
sqlDyStringPrintf(gencodeQuery->where, ")");
}

static struct genePred *gencodeQueryGenePred(struct gencodeQuery *gencodeQuery,
                                             char **row)
/* get genePred from a query results */
{
return genePredExtLoad(row, gencodeQuery->genePredNumColumns);
}

static struct wgEncodeGencodeAttrs *gencodeQueryAttrs(struct gencodeQuery *gencodeQuery,
                                                      char **row)
/* get attributes from a query results, or NULL if not requested */
{
if (gencodeQuery->attrsNumColumns > 0)
    return wgEncodeGencodeAttrsLoad(row + gencodeQuery->genePredNumColumns, gencodeQuery->attrsNumColumns);
else
    return NULL;
}

static boolean anyFilterBy(struct track *tg, filterBySetGetFuncType filterBySetGetFunc)
/* check if any filters were specified of the particular type */
{
filterBy_t *filterBySet = filterBySetGetFunc(tg->tdb, cart, NULL);
filterBy_t *filterBy;
boolean someFilters = FALSE;
for (filterBy = filterBySet; (filterBy != NULL) && !someFilters; filterBy = filterBy->next)
    {
    if (!filterByAllChosen(filterBy))
        someFilters = TRUE;
    }   
filterBySetFree(&filterBySet);
return someFilters;
}

static char *tslSymToNumStr(char *tslSym)
/* convert a transcription support level string (tsl1..tsl5, tslN), to 
 * a numeric string ("1".."5", "-1") */
{
if (sameString(tslSym, "tslNA"))
    return "-1";
else
    return tslSym+3;
}

static void filterByMethodChoiceQuery(char *choice, struct gencodeQuery *gencodeQuery)
/* add SQL expression for GENCODE transcript method choice. */
{
/*
 * example sources and categories:
 *    havana_ig_gene                manual, manual only
 *    ensembl_havana_transcript     manual, automatic & manual and automatic
 *    havana                        manual, manual only
 *    ensembl                       automatic, automatic only
 *    mt_genbank_import             automatic, automatic only
 */
if (sameString(choice, "manual"))
    sqlDyStringPrintf(gencodeQuery->where, "(transSrc.source like \"%%havana%%\")");
else if (sameString(choice, "automatic"))
    sqlDyStringPrintf(gencodeQuery->where, "((transSrc.source like \"%%ensembl%%\") or (transSrc.source not like \"%%havana%%\"))");
else if (sameString(choice, "manual_only"))
    sqlDyStringPrintf(gencodeQuery->where, "((transSrc.source like \"%%havana%%\") and (transSrc.source not like \"%%ensembl%%\"))");
else if (sameString(choice, "automatic_only"))
    sqlDyStringPrintf(gencodeQuery->where, "(transSrc.source not like \"%%havana%%\")");
else if (sameString(choice, "manual_and_automatic"))
    sqlDyStringPrintf(gencodeQuery->where, "((transSrc.source like \"%%havana%%\") and (transSrc.source like \"%%ensembl%%\"))");
else
    errAbort("BUG: filterByMethodChoiceQuery missing choice: \"%s\"", choice);
}

static void filterByMethodChoicesQuery(filterBy_t *filterBy, struct gencodeQuery *gencodeQuery)
/* add transcript source compare clauses */
{
struct slName *choice = NULL;
for (choice = filterBy->slChoices; choice != NULL; choice = choice->next)
    {
    if (choice != filterBy->slChoices)
        sqlDyStringPrintf(gencodeQuery->where, " or ");
    filterByMethodChoiceQuery(choice->name, gencodeQuery);
    }
}

static void filterByMethodQuery(struct track *tg, filterBy_t *filterBy, struct gencodeQuery *gencodeQuery)
/* generate SQL where clause for annotation method filtering */
{
gencodeQueryBeginSubWhere(gencodeQuery);
filterByMethodChoicesQuery(filterBy, gencodeQuery);
gencodeQuery->joinTranscriptSource = TRUE;
gencodeQueryEndSubWhere(gencodeQuery);
}

static void filterBySupportLevelChoiceQuery(char *choice, struct gencodeQuery *gencodeQuery)
/* add SQL expression GENCODE support choice. */
{
/* table is numeric (silly me), and string is tsl1..tsl5 or tslNA */
sqlDyStringPrintf(gencodeQuery->where, "(supLevel.level = %d)", atoi(tslSymToNumStr(choice)));
}

static void filterBySupportLevelChoicesQuery(filterBy_t *filterBy, struct gencodeQuery *gencodeQuery)
/* add support level compare clauses */
{
struct slName *choice = NULL;
for (choice = filterBy->slChoices; choice != NULL; choice = choice->next)
    {
    if (choice != filterBy->slChoices)
        sqlDyStringPrintf(gencodeQuery->where, " or ");
    filterBySupportLevelChoiceQuery(choice->name, gencodeQuery);
    }
}

static void filterBySupportLevelQuery(struct track *tg, filterBy_t *filterBy, struct gencodeQuery *gencodeQuery)
/* generate SQL where clause for annotation support level filtering */
{
gencodeQueryBeginSubWhere(gencodeQuery);
filterBySupportLevelChoicesQuery(filterBy, gencodeQuery);
gencodeQuery->joinSupportLevel = TRUE;
gencodeQueryEndSubWhere(gencodeQuery);
}

static void filterByTagChoiceQuery(char *choice, struct gencodeQuery *gencodeQuery)
/* add SQL expression GENCODE tag choice. */
{
sqlDyStringPrintf(gencodeQuery->where, "(tag.tag = \"%s\")", choice);
}

static void filterByTagChoicesQuery(filterBy_t *filterBy, struct gencodeQuery *gencodeQuery)
/* add tag compare clauses */
{
struct slName *choice = NULL;
for (choice = filterBy->slChoices; choice != NULL; choice = choice->next)
    {
    if (choice != filterBy->slChoices)
        sqlDyStringPrintf(gencodeQuery->where, " or ");
    filterByTagChoiceQuery(choice->name, gencodeQuery);
    }
}

static void filterByTagQuery(struct track *tg, filterBy_t *filterBy, struct gencodeQuery *gencodeQuery)
/* generate SQL where clause for annotation tag filtering */
{
gencodeQueryBeginSubWhere(gencodeQuery);
filterByTagChoicesQuery(filterBy, gencodeQuery);
gencodeQuery->joinTag = TRUE;
gencodeQueryEndSubWhere(gencodeQuery);
}

static void filterByAttrsQuery(struct track *tg, filterBy_t *filterBy, struct gencodeQuery *gencodeQuery)
/* handle adding on filterBy clause for attributes table */
{
char *clause = filterByClause(filterBy);
if (clause != NULL)
    {
    gencodeQueryBeginSubWhere(gencodeQuery);
    sqlDyStringPrintf(gencodeQuery->where, "%-s", clause);
    gencodeQuery->joinAttrs = TRUE;
    gencodeQueryEndSubWhere(gencodeQuery);
    freeMem(clause);
    }
}

static void gencodeFilterByQuery(struct track *tg, filterBy_t *filterBy, struct gencodeQuery *gencodeQuery)
/* handle adding on filterBy clause for gencode */
{
if (sameString(filterBy->column, "transcriptMethod"))
    filterByMethodQuery(tg, filterBy, gencodeQuery);
else if (sameString(filterBy->column, "supportLevel"))
    filterBySupportLevelQuery(tg, filterBy, gencodeQuery);
else if (sameString(filterBy->column, "tag"))
    filterByTagQuery(tg, filterBy, gencodeQuery);
else if (startsWith("attrs.", filterBy->column))
    filterByAttrsQuery(tg, filterBy, gencodeQuery);
else
    errAbort("gencodeFilterByQuery: don't know how to filter on column \"%s\"", filterBy->column);
gencodeQuery->isFiltered = TRUE;
}

static void gencodeFilterBySetQuery(struct track *tg, filterBySetGetFuncType filterBySetGetFunc, struct gencodeQuery *gencodeQuery)
/* build where sql clauses for filters or highlights. */
{
filterBy_t *filterBySet = filterBySetGetFunc(tg->tdb, cart, NULL);
filterBy_t *filterBy;
for (filterBy = filterBySet; filterBy != NULL; filterBy = filterBy->next)
    {
    if (!filterByAllChosen(filterBy))
        gencodeFilterByQuery(tg, filterBy, gencodeQuery);
    }
filterBySetFree(&filterBySet);
}

static void addFilterMaxTranscripsByRange(struct sqlConnection *conn, struct track *tg, struct gencodeQuery *gencodeQuery)
/* Add query for the maximum number of transcripts to display if requested and
 * if transcriptRank is available in attrs */
{
char varName[64];
safef(varName, sizeof(varName), "%s.maxTrans", tg->track);
int maxTrans = cartCgiUsualInt(cart, varName, 0);
if (maxTrans <= 0)
    return;  // zero disables
// do we have transcriptRank column?
if (!sqlColumnExists(conn, gencodeGetTableName(tg->tdb, "wgEncodeGencodeAttrs"), "transcriptRank"))
    return ;

// rank starts at 1, so anything less than or equal to max will be included
gencodeQueryBeginSubWhere(gencodeQuery);
sqlDyStringPrintf(gencodeQuery->where, "attrs.transcriptRank <= %d", maxTrans);
gencodeQuery->joinAttrs = TRUE;
gencodeQueryEndSubWhere(gencodeQuery);
gencodeQuery->isFiltered = TRUE;
}

static void addQueryTables(struct track *tg, struct gencodeQuery *gencodeQuery)
/* add required from tables and joins */
{
sqlDyStringPrintf(gencodeQuery->from, "%s g", tg->table);
if (gencodeQuery->joinAttrs)
    {
    sqlDyStringPrintf(gencodeQuery->from, ", %s attrs", gencodeGetTableName(tg->tdb, "wgEncodeGencodeAttrs"));
    sqlDyStringPrintf(gencodeQuery->where, " and (attrs.transcriptId = g.name)");
    }
if (gencodeQuery->joinTranscriptSource)
    {
    sqlDyStringPrintf(gencodeQuery->from, ", %s transSrc", gencodeGetTableName(tg->tdb, "wgEncodeGencodeTranscriptSource"));
    sqlDyStringPrintf(gencodeQuery->where, " and (transSrc.transcriptId = g.name)");
    }
if (gencodeQuery->joinSupportLevel)
    {
    sqlDyStringPrintf(gencodeQuery->from, ", %s supLevel", gencodeGetTableName(tg->tdb, "wgEncodeGencodeTranscriptionSupportLevel"));
    sqlDyStringPrintf(gencodeQuery->where, " and (supLevel.transcriptId = g.name)");
    }
if (gencodeQuery->joinTag)
    {
    sqlDyStringPrintf(gencodeQuery->from, ", %s tag", gencodeGetTableName(tg->tdb, "wgEncodeGencodeTag"));
    sqlDyStringPrintf(gencodeQuery->where, " and (tag.transcriptId = g.name)");
    }
}

static void addQueryCommon(struct sqlConnection *conn, struct track *tg, filterBySetGetFuncType filterBySetGetFunc, struct gencodeQuery *gencodeQuery)
/* Add tables and joins for both gene and highlight queries */
{
// bin range overlap part
hAddBinToQuery(winStart, winEnd, gencodeQuery->where);
sqlDyStringPrintf(gencodeQuery->where, "(g.chrom = \"%s\") and (g.txStart < %u) and (g.txEnd > %u)", chromName, winEnd, winStart);

gencodeFilterBySetQuery(tg, filterBySetGetFunc, gencodeQuery);
addFilterMaxTranscripsByRange(conn, tg, gencodeQuery);
addQueryTables(tg, gencodeQuery);
}

static struct sqlResult *executeQuery(struct sqlConnection *conn, struct gencodeQuery *gencodeQuery)
/* execute the actual SQL query */
{
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "select %-s from %-s where %-s", 
    dyStringContents(gencodeQuery->fields), dyStringContents(gencodeQuery->from), dyStringContents(gencodeQuery->where));
struct sqlResult *sr = sqlGetResult(conn, dyStringContents(query));
dyStringFree(&query);
return sr;
}

static boolean annotIsGenePredExt(struct track *tg)
/* determine if a table has genePred extended fields.  two-way consensus
 * pseudo doesn't have them. */
{
char *db = database;
char *liftDb = cloneString(trackDbSetting(tg->tdb, "quickLiftDb"));
if (liftDb)
    db = liftDb;
struct sqlConnection *conn = hAllocConn(db);
struct slName *fields = sqlFieldNames(conn, tg->table);
hFreeConn(&conn);
boolean isGenePredX = slNameInList(fields, "score");
slFreeList(&fields);
return isGenePredX;
}

static void geneQueryAddGenePredCols(struct track *tg,
                                     struct gencodeQuery *gencodeQuery)
/* add genePred columns to query */
{
gencodeQuery->isGenePredX = annotIsGenePredExt(tg);
sqlDyStringPrintf(gencodeQuery->fields, "g.name, g.chrom, g.strand, g.txStart, g.txEnd, g.cdsStart, g.cdsEnd, g.exonCount, g.exonStarts, g.exonEnds");
gencodeQuery->genePredNumColumns = GENEPRED_NUM_COLS;

if (gencodeQuery->isGenePredX)
    {
    sqlDyStringPrintf(gencodeQuery->fields, ", g.score, g.name2, g.cdsStartStat, g.cdsEndStat, g.exonFrames");
    gencodeQuery->genePredNumColumns = GENEPREDX_NUM_COLS;
    }
}

static void geneQueryAddAttrsCols(struct track *tg, struct sqlConnection *conn,
                                  struct gencodeQuery *gencodeQuery)
/* add attributes columns to query */
{
struct slName *fields = sqlFieldNames(conn, gencodeGetTableName(tg->tdb, "wgEncodeGencodeAttrs"));

sqlDyStringPrintf(gencodeQuery->fields, ", ");
sqlDyStringPrintf(gencodeQuery->fields, "attrs.geneId, attrs.geneName, attrs.geneType, attrs.geneStatus, attrs.transcriptId, attrs.transcriptName, attrs.transcriptType, attrs.transcriptStatus, attrs.havanaGeneId, attrs.havanaTranscriptId, attrs.ccdsId, attrs.level, attrs.transcriptClass");
gencodeQuery->attrsNumColumns = WGENCODEGENCODEATTRS_MIM_NUM_COLS;
if (slNameInList(fields, "proteinId"))
    {
    sqlDyStringPrintf(gencodeQuery->fields, ", attrs.proteinId");
    gencodeQuery->attrsNumColumns++;
    }
if (slNameInList(fields, "transcriptRank"))
    {
    sqlDyStringPrintf(gencodeQuery->fields, ", attrs.transcriptRank");
    gencodeQuery->attrsNumColumns = WGENCODEGENCODEATTRS_NUM_COLS;
    }
gencodeQuery->joinAttrs = TRUE;
}

static struct gencodeQuery *geneQueryConstruct(struct sqlConnection *conn,
                                               struct track *tg,
                                               boolean includeAttrs)
/* construct the query for a GENCODE records, which includes filters. */
{
struct gencodeQuery *gencodeQuery = gencodeQueryNew();
geneQueryAddGenePredCols(tg, gencodeQuery);
if (includeAttrs)
    geneQueryAddAttrsCols(tg, conn, gencodeQuery);
addQueryCommon(conn, tg, filterBySetGet, gencodeQuery);
return gencodeQuery;
}

static struct gencodeQuery *highlightQueryConstruct(struct track *tg, struct sqlConnection *conn)
/* construct the query for GENCODE ids which should be highlighted.
 * this essentially redoes the genePred query, only using the filter functions
 * and only getting ids */
{
struct gencodeQuery *gencodeQuery = gencodeQueryNew();
sqlDyStringPrintf(gencodeQuery->fields, "g.name");

addQueryCommon(conn, tg, highlightBySetGet, gencodeQuery);
return gencodeQuery;
}

static unsigned getHighlightColor(struct track *tg)
/* get the highlightColor from trackDb, or a default if not found */
{
unsigned char red = 255, green = 165, blue = 0; // Orange default
char *colorStr = trackDbSetting(tg->tdb, "highlightColor");
if (colorStr != NULL)
    parseColor(colorStr, &red, &green, &blue);
return MAKECOLOR_32(red, green, blue);
}

static void highlightByGetColor(struct genePred *gp, struct hash *highlightIds, unsigned highlightColor,
                                struct linkedFeatures *lf)
/* compute the highlight color based on a extra fields returned in a row, setting
 * the linkedFeatures field */
{
if (hashLookup(highlightIds, gp->name) != NULL)
    {
    lf->highlightColor = highlightColor;
    lf->highlightMode = highlightBackground;
    }
}

static struct hash* loadHighlightIds(struct sqlConnection *conn, struct track *tg)
/* Load ids (genePred names) in window for annotations to be highlighted. */
{
struct hash *highlightIds = hashNew(0);
struct gencodeQuery *gencodeQuery = highlightQueryConstruct(tg, conn);
struct sqlResult *sr = executeQuery(conn, gencodeQuery);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    hashAddInt(highlightIds, row[0], 1);
sqlFreeResult(&sr);
return highlightIds;
}

static boolean getLabelCartVar(struct track *tg, char *labelName, boolean *anyExistsP)
/* get the cart label value for a label type. Sort TRUE in anyExistsP if the variable exists. */
{
char varSuffix[64];
safef(varSuffix, sizeof(varSuffix), "label.%s", labelName);
char *value = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, varSuffix, NULL);
if (value != NULL)
    *anyExistsP = TRUE;
return ((value != NULL) && !sameString(value, "0"));
}

static void setLabelCartVar(struct track *tg, char *labelName, boolean value)
/* set one of the label type cart variables. */
{
char varName[256];
safef(varName, sizeof(varName), "%s.label.%s", tg->track, labelName);
cartSetBoolean(cart, varName, value);
}

static unsigned setFromOldLabelsVarsInCart(struct track *tg)
/* If the old gencode label variable are set for this track, migrate to the new
 * variable.  This prevents labels from disappearing with the new old and an old cart.
 */
{
// logic from simpleTracks.c:genePredAssignConfiguredName()
unsigned enabledLabels = 0;
char *geneLabel = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, "label", "gene");
if (sameString(geneLabel, "gene") || sameString(geneLabel, "name") || sameString(geneLabel, "both"))
    {
    setLabelCartVar(tg, "geneName", TRUE);
    enabledLabels |= ITEM_LABEL_GENE_NAME;
    }
if (sameString(geneLabel, "accession") || sameString(geneLabel, "both"))
    {
    setLabelCartVar(tg, "transcriptId", TRUE);
    enabledLabels |= ITEM_LABEL_TRANSCRIPT_ID;
    }
cartRemoveVariableClosestToHome(cart, tg->tdb, FALSE, "label");
return enabledLabels;
}

static unsigned getEnabledLabels(struct track *tg)
/* Look up each of the label type names in the cart to see if they are enabled,
 * Return bit set */
{
unsigned enabledLabels = 0;
boolean anyExists = FALSE;
int i;
for (i = 0; itemLabelCartVarNamesMap[i].name != NULL; i++)
    {
    if (getLabelCartVar(tg, itemLabelCartVarNamesMap[i].name, &anyExists))
        enabledLabels |= itemLabelCartVarNamesMap[i].flag;
    }

// if it looks like new track settings have never been configured, set from old.
if ((enabledLabels == 0) && !anyExists)
    enabledLabels = setFromOldLabelsVarsInCart(tg);
return enabledLabels;
}

static void concatItemName(char *name, int nameSize, char *value)
/* add a name to the name buffer */
{
if (strlen(name) > 0)
    safecat(name, nameSize, "/");
safecat(name, nameSize, value);
}

static char* getTranscriptLabel(unsigned enabledLabels, struct genePred *gp,
                                struct wgEncodeGencodeAttrs *attrs)
/* one type of label for a item in track */
{
char name[256];
name[0] = '\0';
if (enabledLabels & ITEM_LABEL_GENE_NAME)
    concatItemName(name, sizeof(name), gp->name2);
if (enabledLabels & ITEM_LABEL_GENE_ID)
    concatItemName(name, sizeof(name), attrs->geneId);
if (enabledLabels & ITEM_LABEL_TRANSCRIPT_ID)
    concatItemName(name, sizeof(name), gp->name);
return cloneString(name);
}

static struct linkedFeatures *loadGencodeTranscript(struct track *tg, struct gencodeQuery *gencodeQuery, char **row,
                                                    unsigned enabledLabels, struct hash *highlightIds, unsigned highlightColor)
/* load one genePred record into a linkedFeatures object */
{
struct genePred *gp = gencodeQueryGenePred(gencodeQuery, row);
struct wgEncodeGencodeAttrs *attrs = gencodeQueryAttrs(gencodeQuery, row);  // maybe NULL
struct linkedFeatures *lf = linkedFeaturesFromGenePred(tg, gp, TRUE);
if (highlightIds != NULL)
    highlightByGetColor(gp, highlightIds, highlightColor, lf);
if (gencodeQuery->isGenePredX)
    lf->extra = getTranscriptLabel(enabledLabels, gp, attrs);
else
    lf->extra = cloneString(gp->name);
wgEncodeGencodeAttrsFree(&attrs);
return lf;
}

static void loadGencodeTrack(struct track *tg)
/* Load genePreds in window info linked feature, with filtering, etc. */
{
char *db = database;
char *liftDb = cloneString(trackDbSetting(tg->tdb, "quickLiftDb"));
if (liftDb)
    db = liftDb;
struct sqlConnection *conn = hAllocConn(db);
unsigned enabledLabels = getEnabledLabels(tg);
boolean needAttrs = (enabledLabels & ITEM_LABEL_GENE_ID) != 0;  // only for certain labels
struct hash *highlightIds = NULL;
if (anyFilterBy(tg, highlightBySetGet))
    highlightIds = loadHighlightIds(conn, tg);
struct gencodeQuery *gencodeQuery = geneQueryConstruct(conn, tg, needAttrs);
struct sqlResult *sr = executeQuery(conn, gencodeQuery);
struct linkedFeatures *lfList = NULL;
unsigned highlightColor = getHighlightColor(tg);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    slAddHead(&lfList, loadGencodeTranscript(tg, gencodeQuery, row, enabledLabels, highlightIds, highlightColor));
sqlFreeResult(&sr);
hFreeConn(&conn);

if (tg->visibility != tvDense)
    slSort(&lfList, linkedFeaturesCmpStart);
else
    slReverse(&lfList);
tg->items = lfList;
if (gencodeQuery->isFiltered)
    labelTrackAsFiltered(tg);
gencodeQueryFree(&gencodeQuery);
hashFree(&highlightIds);
}

static char *gencodeGeneName(struct track *tg, void *item)
/* Get name to use for Gencode gene item. */
{
struct linkedFeatures *lf = item;
if (lf->extra != NULL)
    return lf->extra;
else
    return lf->name;
}

static void gencodeGeneMethods(struct track *tg)
/* Load up custom methods for ENCODE Gencode gene track */
{
char *liftDb = cloneString(trackDbSetting(tg->tdb, "quickLiftDb"));
extern void loadGenePredWithName2(struct track *tg);
if (liftDb)
    tg->loadItems = loadGenePredWithName2;
else
    tg->loadItems = loadGencodeTrack;
tg->itemName = gencodeGeneName;
}

static void registerProductionTrackHandlers()
/* register track handlers for production GENCODE tracks */
{
// ENCODE 1 legacy
registerTrackHandler("wgEncodeSangerGencode", gencodeGeneMethods);

// uses trackHandler attribute
registerTrackHandler("wgEncodeGencode", gencodeGeneMethods);

}

static char *gencodePilotGeneName(struct track *tg, void *item)
/* Get name to use for GENCODE pilot gene item. */
{
struct linkedFeatures *lf = item;
if (lf->extra != NULL)
    return lf->extra;
else
    return lf->name;
}

static void gencodePilotGeneMethods(struct track *tg)
/* Load up custom methods for ENCODE Gencode gene track */
{
tg->loadItems = loadGenePredWithConfiguredName;
tg->itemName = gencodePilotGeneName;
}

static Color gencodeIntronPilotColorItem(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of ENCODE gencode intron track item.  For ENCODE pilot tracks only.
 * Use recommended color palette pantone colors (level 4) for red, green, blue.*/
{
struct gencodeIntron *intron = (struct gencodeIntron *)item;

if (sameString(intron->status, "not_tested"))
    return hvGfxFindColorIx(hvg, 214,214,216);       /* light grey */
if (sameString(intron->status, "RT_negative"))
    return hvGfxFindColorIx(hvg, 145,51,56);       /* red */
if (sameString(intron->status, "RT_positive") ||
        sameString(intron->status, "RACE_validated"))
    return hvGfxFindColorIx(hvg, 61,142,51);       /* green */
if (sameString(intron->status, "RT_wrong_junction"))
    return getOrangeColor(hvg);                 /* orange */
if (sameString(intron->status, "RT_submitted"))
    return hvGfxFindColorIx(hvg, 102,109,112);       /* grey */
return hvGfxFindColorIx(hvg, 214,214,216);       /* light grey */
}

static void gencodeIntronPilotLoadItems(struct track *tg)
/* Load up track items.  For ENCODE pilot tracks only. */
{
bedLoadItem(tg, tg->table, (ItemLoader)gencodeIntronLoad);
}

static void gencodeIntronPilotMethods(struct track *tg)
/* Load up custom methods for ENCODE Gencode intron validation track.
 * For ENCODE pilot tracks only. */
{
tg->loadItems = gencodeIntronPilotLoadItems;
tg->itemColor = gencodeIntronPilotColorItem;
}

static void gencodePilotRaceFragsMethods(struct track *tg)
/* Load up custom methods for ENCODE Gencode RACEfrags track.  For ENCODE
 * pilot tracks only. */
{
tg->loadItems = loadGenePred;
tg->subType = lfNoIntronLines;
}

static void registerPilotTrackHandlers()
/* register track handlers for pilot GENCODE tracks */
{
// hg16 only
registerTrackHandler("encodeGencodeGene", gencodePilotGeneMethods);
registerTrackHandler("encodeGencodeIntron", gencodeIntronPilotMethods);

// hg17 only
registerTrackHandler("encodeGencodeGeneJun05", gencodePilotGeneMethods);
registerTrackHandler("encodeGencodeIntronJun05", gencodeIntronPilotMethods);
registerTrackHandler("encodeGencodeGeneOct05", gencodePilotGeneMethods);
registerTrackHandler("encodeGencodeIntronOct05", gencodeIntronPilotMethods);
registerTrackHandler("encodeGencodeGeneMar07", gencodePilotGeneMethods);
registerTrackHandler("encodeGencodeGenePolyAMar07", bed9Methods);
registerTrackHandler("encodeGencodeRaceFrags", gencodePilotRaceFragsMethods);
}


void gencodeRegisterTrackHandlers()
/* register track handlers for GENCODE tracks */
{
registerProductionTrackHandlers();
registerPilotTrackHandlers();
}
