/* gencodeTracks - ENCODE GENCODE Genes tracks for both pilot and production ENCODE.
 * although these are used fundamentally different approaches to display */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hgTracks.h"
#include "hdb.h"
#include "gencodeIntron.h"
#include "genePredReader.h"
#include "genePred.h"

typedef filterBy_t* (*filterBySetGetFuncType)(struct trackDb *tdb, struct cart *cart, char *name);

struct gencodeQuery
/* structure used to store information about the query being assembled.
 * this allows a join to bring in additional data without having to
 * do repeated queries. */
{
    struct dyString *fields;                // select fields
    struct dyString *from;                  // from clause
    struct dyString *where;                 // where clause
    boolean isGenePredX;                    // does this have the extended fields?
    boolean isFiltered;                     // are there filters on the query?
    boolean joinAttrs;                      // join the wgEncodeGencodeAttrs table
    boolean joinSupportLevel;               // join the wgEncodeGencodeTranscriptionSupportLevel table
    boolean joinTranscriptSource;           // join the wgEncodeGencodeTranscriptSource table
    boolean joinTag;                        // join the wgEncodeGencodeTag table
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
    dyStringAppend(gencodeQuery->where, " and ");
dyStringAppend(gencodeQuery->where, "(");
}

static void gencodeQueryEndSubWhere(struct gencodeQuery *gencodeQuery)
/* finish adding new where sub-clause */
{
dyStringAppend(gencodeQuery->where, ")");
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
/* add SQL expression GENCODE transcript method choice. */
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
    dyStringAppend(gencodeQuery->where, "(transSrc.source like \"%havana%\")");
else if (sameString(choice, "automatic"))
    dyStringAppend(gencodeQuery->where, "((transSrc.source like \"%ensembl%\") or (transSrc.source not like \"%havana%\"))");
else if (sameString(choice, "manual_only"))
    dyStringAppend(gencodeQuery->where, "((transSrc.source like \"%havana%\") and (transSrc.source not like \"%ensembl%\"))");
else if (sameString(choice, "automatic_only"))
    dyStringAppend(gencodeQuery->where, "(transSrc.source not like \"%havana%\")");
else if (sameString(choice, "manual_and_automatic"))
    dyStringAppend(gencodeQuery->where, "((transSrc.source like \"%havana%\") and (transSrc.source like \"%ensembl%\"))");
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
        dyStringAppend(gencodeQuery->where, " or ");
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
dyStringPrintf(gencodeQuery->where, "(supLevel.level = %s)", tslSymToNumStr(choice));
}

static void filterBySupportLevelChoicesQuery(filterBy_t *filterBy, struct gencodeQuery *gencodeQuery)
/* add support level compare clauses */
{
struct slName *choice = NULL;
for (choice = filterBy->slChoices; choice != NULL; choice = choice->next)
    {
    if (choice != filterBy->slChoices)
        dyStringAppend(gencodeQuery->where, " or ");
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
dyStringPrintf(gencodeQuery->where, "(tag.tag = \"%s\")", choice);
}

static void filterByTagChoicesQuery(filterBy_t *filterBy, struct gencodeQuery *gencodeQuery)
/* add tag compare clauses */
{
struct slName *choice = NULL;
for (choice = filterBy->slChoices; choice != NULL; choice = choice->next)
    {
    if (choice != filterBy->slChoices)
        dyStringAppend(gencodeQuery->where, " or ");
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
    dyStringPrintf(gencodeQuery->where, "%s", clause);
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
/* build where clause based for filters or highlights filters. */
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

static void addQueryTables(struct track *tg, struct gencodeQuery *gencodeQuery)
/* add required from tables and joins */
{
sqlDyStringPrintfFrag(gencodeQuery->from, "%s g", tg->table);
if (gencodeQuery->joinAttrs)
    {
    sqlDyStringPrintf(gencodeQuery->from, ", %s attrs", trackDbRequiredSetting(tg->tdb, "wgEncodeGencodeAttrs"));
    dyStringAppend(gencodeQuery->where, " and (attrs.transcriptId = g.name)");
    }
if (gencodeQuery->joinTranscriptSource)
    {
    sqlDyStringPrintf(gencodeQuery->from, ", %s transSrc", trackDbRequiredSetting(tg->tdb, "wgEncodeGencodeTranscriptSource"));
    dyStringAppend(gencodeQuery->where, " and (transSrc.transcriptId = g.name)");
    }
if (gencodeQuery->joinSupportLevel)
    {
    sqlDyStringPrintf(gencodeQuery->from, ", %s supLevel", trackDbRequiredSetting(tg->tdb, "wgEncodeGencodeTranscriptionSupportLevel"));
    dyStringAppend(gencodeQuery->where, " and (supLevel.transcriptId = g.name)");
    }
if (gencodeQuery->joinTag)
    {
    sqlDyStringPrintf(gencodeQuery->from, ", %s tag", trackDbRequiredSetting(tg->tdb, "wgEncodeGencodeTag"));
    dyStringAppend(gencodeQuery->where, " and (tag.transcriptId = g.name)");
    }
}

static void addQueryCommon(struct track *tg, filterBySetGetFuncType filterBySetGetFunc, struct gencodeQuery *gencodeQuery)
/* Add tables and joins for both gene and highlight queries */
{
// bin range overlap part
hAddBinToQuery(winStart, winEnd, gencodeQuery->where);
sqlDyStringPrintf(gencodeQuery->where, "(g.chrom = \"%s\") and (g.txStart < %u) and (g.txEnd > %u)", chromName, winEnd, winStart);

gencodeFilterBySetQuery(tg, filterBySetGetFunc, gencodeQuery);
addQueryTables(tg, gencodeQuery);
}

static struct sqlResult *executeQuery(struct sqlConnection *conn, struct gencodeQuery *gencodeQuery)
/* execute the actual SQL query */
{
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "select %-s from %-s where %-s", 
    sqlCkIl(dyStringContents(gencodeQuery->fields)), dyStringContents(gencodeQuery->from), dyStringContents(gencodeQuery->where));
struct sqlResult *sr = sqlGetResult(conn, dyStringContents(query));
dyStringFree(&query);
return sr;
}

static boolean tableIsGenePredX(struct track *tg)
/* determine if a table has genePred extended fields.  two-way consensus
 * pseudo doesn't have them. */
{
struct sqlConnection *conn = hAllocConn(database);
struct slName *fields = sqlFieldNames(conn, tg->table);
hFreeConn(&conn);
boolean isGenePredX = slNameInList(fields, "score");
slFreeList(&fields);
return isGenePredX;
}

static struct gencodeQuery *geneQueryConstruct(struct track *tg)
/* construct the query for a GENCODE genePred records, which includes filters. */
{
static char *genePredXFields = "g.name, g.chrom, g.strand, g.txStart, g.txEnd, g.cdsStart, g.cdsEnd, g.exonCount, g.exonStarts, g.exonEnds, g.score, g.name2, g.cdsStartStat, g.cdsEndStat, g.exonFrames";
static char *genePredFields = "g.name, g.chrom, g.strand, g.txStart, g.txEnd, g.cdsStart, g.cdsEnd, g.exonCount, g.exonStarts, g.exonEnds";

struct gencodeQuery *gencodeQuery = gencodeQueryNew();
gencodeQuery->isGenePredX = tableIsGenePredX(tg);
dyStringAppend(gencodeQuery->fields, (gencodeQuery->isGenePredX ? genePredXFields : genePredFields));

addQueryCommon(tg, filterBySetGet, gencodeQuery);
return gencodeQuery;
}

static struct gencodeQuery *highlightQueryConstruct(struct track *tg)
/* construct the query for GENCODE ids which should be highlighted.
 * this essentially redoes the genePred query, only using the filter functions
 * and only getting ids */
{
struct gencodeQuery *gencodeQuery = gencodeQueryNew();
dyStringAppend(gencodeQuery->fields, "g.name");

addQueryCommon(tg, highlightBySetGet, gencodeQuery);
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
struct gencodeQuery *gencodeQuery = highlightQueryConstruct(tg);
struct sqlResult *sr = executeQuery(conn, gencodeQuery);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    hashAddInt(highlightIds, row[0], 1);
sqlFreeResult(&sr);
return highlightIds;
}


static struct linkedFeatures *loadGencodeGenePred(struct track *tg, struct gencodeQuery *gencodeQuery, char **row,
                                                  struct hash *highlightIds, unsigned highlightColor)
/* load one genePred record into a linkedFeatures object */
{
struct genePred *gp = genePredExtLoad(row, (gencodeQuery->isGenePredX ? GENEPREDX_NUM_COLS:  GENEPRED_NUM_COLS));
struct linkedFeatures *lf = linkedFeaturesFromGenePred(tg, gp, TRUE);
highlightByGetColor(gp, highlightIds, highlightColor, lf);
return lf;
}

static void loadGencodeGenePreds(struct track *tg)
/* Load genePreds in window info linked feature, with filtering, etc. */
{
struct sqlConnection *conn = hAllocConn(database);
struct hash *highlightIds = loadHighlightIds(conn, tg);
struct gencodeQuery *gencodeQuery = geneQueryConstruct(tg);
struct sqlResult *sr = executeQuery(conn, gencodeQuery);
struct linkedFeatures *lfList = NULL;
unsigned highlightColor = getHighlightColor(tg);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    slAddHead(&lfList, loadGencodeGenePred(tg, gencodeQuery, row, highlightIds, highlightColor));
sqlFreeResult(&sr);
hFreeConn(&conn);

if (tg->visibility != tvDense)
    slSort(&lfList, linkedFeaturesCmpStart);
else
    slReverse(&lfList);
tg->items = lfList;
genePredAssignConfiguredName(tg);

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
tg->loadItems = loadGencodeGenePreds;
tg->itemName = gencodeGeneName;
}

static void registerProductionTrackHandlers()
/* register track handlers for production GENCODE tracks */
{
// ENCODE 1 legacy
registerTrackHandler("wgEncodeSangerGencode", gencodeGeneMethods);

// uses trackHandler attribute
registerTrackHandler("wgEncodeGencode", gencodeGeneMethods);

// one per gencode version. Add a bunch in the future so
// this doesn't have to be changed on every release.
// FIXME: delete these CGI and tracks are all user trackHandler
registerTrackHandler("wgEncodeGencodeV3", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV4", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV7", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV8", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV9", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV10", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV11", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV12", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV13", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV14", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV15", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV16", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV17", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV18", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV19", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV20", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeV21", gencodeGeneMethods);

registerTrackHandler("wgEncodeGencodeVM2", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeVM3", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeVM4", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeVM5", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeVM6", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeVM7", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeVM8", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeVM9", gencodeGeneMethods);
registerTrackHandler("wgEncodeGencodeVM10", gencodeGeneMethods);

}

static void gencodePilotGeneMethods(struct track *tg)
/* Load up custom methods for ENCODE Gencode gene track */
{
tg->loadItems = loadGenePredWithConfiguredName;
tg->itemName = gencodeGeneName;
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
registerTrackHandler("encodeGencodeGenePolyAMar07",bed9Methods);
registerTrackHandler("encodeGencodeRaceFrags", gencodePilotRaceFragsMethods);
}


void gencodeRegisterTrackHandlers()
/* register track handlers for GENCODE tracks */
{
registerProductionTrackHandlers();
registerPilotTrackHandlers();
}
