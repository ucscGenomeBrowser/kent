/* gencodeTracks - ENCODE GENCODE Genes tracks for both pilot and production ENCODE.
 * although these are used fundamentally different approaches to display */
#include "common.h"
#include "hgTracks.h"
#include "hdb.h"
#include "gencodeIntron.h"
#include "genePredReader.h"
#include "genePred.h"

struct gencodeQuery
/* structure used to store information about the query being assembled.
 * this allows a join to bring in additional data without having to
 * do repeated queries. */
{
    struct dyString *fields;                // select fields
    struct dyString *from;                  // from clause
    struct dyString *where;                 // where clause
    boolean isGenePredX;                    // does this have the extended fields?
    int nextFieldCol;                       // next available column in result row
    int supportLevelCol;                    // support level column if joined for highlighting, or -1 
    int transcriptTypeCol;                  // transcript type column if joined for highlighting, or -1 
    int transcriptSourceCol;                // transcript source column if joined for method highlighting, or -1 
    filterBy_t *supportLevelHighlight;      // choices for support level highlighting if not NULL
    filterBy_t *transcriptTypeHighlight;    // choices for transcript type highlighting if not NULL
    filterBy_t *transcriptMethodHighlight;  // choices for transcript method highlighting if not NULL
    boolean joinAttrs;                      // join the wgEncodeGencodeAttrs table
    boolean joinTransSrc;                   // join the wgEncodeGencodeTranscriptSource table
    boolean joinSupportLevel;               // join the wgEncodeGencodeTranscriptionSupportLevel table
    boolean joinTranscriptSource;            // join the wgEncodeGencodeTranscriptSource table
};

static struct gencodeQuery *gencodeQueryNew(void)
/* construct a new gencodeQuery object */
{
struct gencodeQuery *gencodeQuery;
AllocVar(gencodeQuery);
gencodeQuery->fields = dyStringNew(0);
gencodeQuery->from = dyStringNew(0);
gencodeQuery->where = dyStringNew(0);
gencodeQuery->supportLevelCol = -1;
gencodeQuery->transcriptTypeCol = -1;
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
    filterBySetFree(&gencodeQuery->supportLevelHighlight);
    filterBySetFree(&gencodeQuery->transcriptTypeHighlight);
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

static char *tslNumStrToSym(char *tslNum)
/* convert a transcription support level numeric string ("1".."5", "-1:) 
 * to a symbol (tsl1..tsl5, tslN).  WARNING: static return */
{
static char buf[8];
if (sameString(tslNum, "-1"))
    return "tslNA";
else
    {
    safef(buf, sizeof(buf), "tsl%s", tslNum);
    return buf;
    }
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
gencodeQuery->joinTransSrc = TRUE;
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
else
    filterByAttrsQuery(tg, filterBy, gencodeQuery);
}

static void gencodeFilterBySetQuery(struct track *tg, struct gencodeQuery *gencodeQuery)
/* build where clause based on filters. */
{
filterBy_t *filterBySet = filterBySetGet(tg->tdb, cart, NULL);
filterBy_t *filterBy;
for (filterBy = filterBySet; filterBy != NULL; filterBy = filterBy->next)
    {
    if (!filterByAllChosen(filterBy))
        gencodeFilterByQuery(tg, filterBy, gencodeQuery);
    }
filterBySetFree(&filterBySet);
}

static void highlightBySupportLevelQuery(struct track *tg, filterBy_t *highlightBy, struct gencodeQuery *gencodeQuery)
/* generate SQL where clause for obtaining support level for highlighting */
{
dyStringAppend(gencodeQuery->fields, ", supLevel.level");
gencodeQuery->supportLevelCol = gencodeQuery->nextFieldCol++;
gencodeQuery->supportLevelHighlight = highlightBy;
gencodeQuery->joinSupportLevel = TRUE;
}

static void highlightByTranscriptTypeQuery(struct track *tg, filterBy_t *highlightBy, struct gencodeQuery *gencodeQuery)
/* generate SQL where clause for obtaining transcript type for highlighting */
{
dyStringAppend(gencodeQuery->fields, ", attrs.transcriptType");
gencodeQuery->transcriptTypeCol = gencodeQuery->nextFieldCol++;
gencodeQuery->transcriptTypeHighlight = highlightBy;
gencodeQuery->joinAttrs = TRUE;
}


static void highlightByTranscriptMethodQuery(struct track *tg, filterBy_t *highlightBy, struct gencodeQuery *gencodeQuery)
/* generate SQL where clause for obtaining transcript type for highlighting */
{
gencodeQuery->joinTransSrc = TRUE;
dyStringAppend(gencodeQuery->fields, ", transSrc.source");
gencodeQuery->transcriptSourceCol = gencodeQuery->nextFieldCol++;
gencodeQuery->transcriptMethodHighlight = highlightBy;
gencodeQuery->joinAttrs = TRUE;
}

static void gencodeHighlightByQuery(struct track *tg, filterBy_t *highlightBy, struct gencodeQuery *gencodeQuery)
/* Handle a highlight by category.  highlightBy object will be saved in gencodeQuery */
{
if (sameString(highlightBy->column, "supportLevel"))
    highlightBySupportLevelQuery(tg, highlightBy, gencodeQuery);
else if (sameString(highlightBy->column, "attrs.transcriptType"))
    highlightByTranscriptTypeQuery(tg, highlightBy, gencodeQuery);
else if (sameString(highlightBy->column, "transcriptMethod"))
    highlightByTranscriptMethodQuery(tg, highlightBy, gencodeQuery);
else
    errAbort("BUG: gencodeHighlightByQuery unknown highlight column: \"%s\"", highlightBy->column);
}

static void gencodeHighlightBySetQuery(struct track *tg, struct gencodeQuery *gencodeQuery)
/* build add join and fields to include to provide data for highlighting.  This results
 * in extra columns being returned. */
{
filterBy_t *highlightBySet = highlightBySetGet(tg->tdb, cart, NULL);
filterBy_t *highlightBy;
while ((highlightBy = slPopHead(&highlightBySet)) != NULL)
    {
    if (!filterByAllChosen(highlightBy))
        gencodeHighlightByQuery(tg, highlightBy, gencodeQuery);
    else
        filterBySetFree(&highlightBy);
    }
}

static bool highlightBySupportLevelSelected(char **row, struct gencodeQuery *gencodeQuery)
/* is the support level associated with this transcript selected? */
{
if (gencodeQuery->supportLevelHighlight == NULL)
    return FALSE;  // no highlighting by support level
else
    return slNameInList(gencodeQuery->supportLevelHighlight->slChoices, tslNumStrToSym(row[gencodeQuery->supportLevelCol]));
}

static bool highlightByTranscriptTypeSelected(char **row, struct gencodeQuery *gencodeQuery)
/* is the transcript type associated with this transcript selected? */
{
if (gencodeQuery->transcriptTypeHighlight == NULL)
    return FALSE;  // no highlighting by transcript type
else
    return slNameInList(gencodeQuery->transcriptTypeHighlight->slChoices, row[gencodeQuery->transcriptTypeCol]);
}

static bool highlightByTranscriptMethodMatch(char *choice, char *transSource)
/* check if the transcript source matches the specified choice */
{
if (sameString(choice, "manual"))
    {
    if (containsStringNoCase(transSource, "havana"))
        return TRUE;
    }
else if (sameString(choice, "automatic"))
    {
    if (containsStringNoCase(transSource, "ensembl") || !containsStringNoCase(transSource, "havana"))
        return TRUE;
    }
else if (sameString(choice, "manual_only"))
    {
    if (containsStringNoCase(transSource, "havana") && !containsStringNoCase(transSource, "ensembl"))
        return TRUE;
    }
else if (sameString(choice, "automatic_only"))
    {
    if (!containsStringNoCase(transSource, "havana"))
        return TRUE;
    }
else if (sameString(choice, "manual_and_automatic"))
    {
    if (containsStringNoCase(transSource, "havana") && containsStringNoCase(transSource, "ensembl"))
        return TRUE;
    }
else
    errAbort("BUG: highlightByTranscriptMethodMatch missing choice: \"%s\"", choice);
return FALSE;
}

static bool highlightByTranscriptMethodSelected(char **row, struct gencodeQuery *gencodeQuery)
/* is the transcript type associated with this transcript selected? */
{
if (gencodeQuery->transcriptMethodHighlight != NULL)
    {
    struct slName *choice;
    for (choice = gencodeQuery->transcriptMethodHighlight->slChoices; choice != NULL; choice = choice->next)
        {
        if (highlightByTranscriptMethodMatch(choice->name, row[gencodeQuery->transcriptSourceCol]))
            return TRUE;
        }
    }
return FALSE;
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

static void highlightByGetColor(char **row, struct gencodeQuery *gencodeQuery, unsigned highlightColor,
                                struct linkedFeatures *lf)
/* compute the highlight color based on a extra fields returned in a row, setting
 * the linkedFeatures field */
{
if (highlightBySupportLevelSelected(row, gencodeQuery) || highlightByTranscriptTypeSelected(row, gencodeQuery)
    || highlightByTranscriptMethodSelected(row, gencodeQuery))
    {
    lf->highlightColor = highlightColor;
    lf->highlightMode = highlightBackground;
    }
}

static void addQueryTables(struct track *tg, struct gencodeQuery *gencodeQuery)
/* add required from tables and joins */
{
dyStringPrintf(gencodeQuery->from, "%s g", tg->table);
if (gencodeQuery->joinAttrs)
    {
    dyStringPrintf(gencodeQuery->from, ", %s attrs", trackDbRequiredSetting(tg->tdb, "wgEncodeGencodeAttrs"));
    dyStringAppend(gencodeQuery->where, " and (attrs.transcriptId = g.name)");
    }
if (gencodeQuery->joinTransSrc)
    {
    dyStringPrintf(gencodeQuery->from, ", %s transSrc", trackDbRequiredSetting(tg->tdb, "wgEncodeGencodeTranscriptSource"));
    dyStringAppend(gencodeQuery->where, " and (transSrc.transcriptId = g.name)");
    }
if (gencodeQuery->joinSupportLevel)
    {
    dyStringPrintf(gencodeQuery->from, ", %s supLevel", trackDbRequiredSetting(tg->tdb, "wgEncodeGencodeTranscriptionSupportLevel"));
    dyStringAppend(gencodeQuery->where, " and (supLevel.transcriptId = g.name)");
    }
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

static struct gencodeQuery *gencodeQueryConstruct(struct track *tg)
/* construct the query for a GENCODE genePred track, which includes filters. */
{
static char *genePredXFields = "g.name, g.chrom, g.strand, g.txStart, g.txEnd, g.cdsStart, g.cdsEnd, g.exonCount, g.exonStarts, g.exonEnds, g.score, g.name2, g.cdsStartStat, g.cdsEndStat, g.exonFrames";
static char *genePredFields = "g.name, g.chrom, g.strand, g.txStart, g.txEnd, g.cdsStart, g.cdsEnd, g.exonCount, g.exonStarts, g.exonEnds";

struct gencodeQuery *gencodeQuery = gencodeQueryNew();
gencodeQuery->isGenePredX = tableIsGenePredX(tg);
gencodeQuery->nextFieldCol = (gencodeQuery->isGenePredX ? GENEPREDX_NUM_COLS : GENEPRED_NUM_COLS);
dyStringAppend(gencodeQuery->fields, (gencodeQuery->isGenePredX ? genePredXFields : genePredFields));

// bin range overlap part
hAddBinToQuery(winStart, winEnd, gencodeQuery->where);
dyStringPrintf(gencodeQuery->where, "(g.chrom = \"%s\") and (g.txStart < %u) and (g.txEnd > %u)", chromName, winEnd, winStart);

gencodeFilterBySetQuery(tg, gencodeQuery);
gencodeHighlightBySetQuery(tg, gencodeQuery);
addQueryTables(tg, gencodeQuery);
return gencodeQuery;
}

static struct sqlResult *gencodeMakeQuery(struct sqlConnection *conn, struct gencodeQuery *gencodeQuery)
/* make the actual SQL query */
{
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "select %s from %s where %s", dyStringContents(gencodeQuery->fields), dyStringContents(gencodeQuery->from), dyStringContents(gencodeQuery->where));
struct sqlResult *sr = sqlGetResult(conn, dyStringContents(query));
dyStringFree(&query);
return sr;
}

static struct linkedFeatures *loadGencodeGenePred(struct track *tg, struct gencodeQuery *gencodeQuery, char **row, unsigned highlightColor)
/* load one genePred record into a linkedFeatures object */
{
struct genePred *gp = genePredExtLoad(row, (gencodeQuery->isGenePredX ? GENEPREDX_NUM_COLS:  GENEPRED_NUM_COLS));
struct linkedFeatures *lf = linkedFeaturesFromGenePred(tg, gp, TRUE);
highlightByGetColor(row, gencodeQuery, highlightColor, lf);
return lf;
}

static void loadGencodeGenePreds(struct track *tg)
/* Load genePreds in window info linked feature, with filtering, etc. */
{
struct gencodeQuery *gencodeQuery = gencodeQueryConstruct(tg);
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = gencodeMakeQuery(conn, gencodeQuery);
struct linkedFeatures *lfList = NULL;
unsigned highlightColor = getHighlightColor(tg);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    slAddHead(&lfList, loadGencodeGenePred(tg, gencodeQuery, row, highlightColor));
sqlFreeResult(&sr);
hFreeConn(&conn);
gencodeQueryFree(&gencodeQuery);

if (tg->visibility != tvDense)
    slSort(&lfList, linkedFeaturesCmpStart);
else
    slReverse(&lfList);
tg->items = lfList;
genePredAssignConfiguredName(tg);
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
// pre-versioning
registerTrackHandler("wgEncodeGencode", gencodeGeneMethods);
registerTrackHandler("wgEncodeSangerGencode", gencodeGeneMethods);

// one per gencode version. Add a bunch in the future so
// this doesn't have to be changed on every release.
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
