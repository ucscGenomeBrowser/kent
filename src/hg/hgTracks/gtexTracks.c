/* GTEx (Genotype Tissue Expression) tracks  */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hgTracks.h"
#include "hvGfx.h"
#include "rainbow.h"
#include "gtexInfo.h"
#include "gtexGeneBed.h"
#include "gtexTissue.h"
#include "gtexTissueData.h"
#include "gtexUi.h"

// NOTE: Sections to change for multi-region (vertical slice) display 
//       are marked with #ifdef MULTI_REGION

struct gtexGeneExtras 
/* Track info */
    {
    double maxMedian;           /* Maximum median rpkm for all tissues */
    boolean isComparison;       /* Comparison of two sample sets (e.g. male/female).
                                       Displayed as two graphs, one oriented downward */
    char *graphType;            /* Additional info about graph (e.g. type of comparison graph */
    struct rgbColor *colors;    /* Color palette for tissues */
    };

struct gtexGeneInfo
/* GTEx gene model, names, and expression medians */
    {
    struct gtexGeneInfo *next;  /* Next in singly linked list */
    struct gtexGeneBed *geneBed;/* Gene name, id, canonical transcript, exp count and medians 
                                        from BED table */
    struct genePred *geneModel; /* Gene structure from model table */
    double *medians1;            /* Computed medians */
    double *medians2;            /* Computed medians for comparison (inverse) graph */
    };

/***********************************************/
/* Color gene models using GENCODE conventions */

// TODO: reuse GENCODE code for some/all of this ??
// MAKE_COLOR_32 ?
struct rgbColor codingColor = {12, 12, 120}; // #0C0C78
struct rgbColor noncodingColor = {0, 100, 0}; // #006400
struct rgbColor problemColor = {254, 0, 0}; // #FE0000
struct rgbColor unknownColor = {1, 1, 1};

static struct statusColors
/* Color values for gene models */
    {
    Color coding;
    Color noncoding;
    Color problem;
    Color unknown;
    } statusColors = {0,0,0,0};

static void initGeneColors(struct hvGfx *hvg)
/* Get and cache indexes for color values */
{
if (statusColors.coding != 0)
    return;
statusColors.coding = hvGfxFindColorIx(hvg, codingColor.r, codingColor.g, codingColor.b);
statusColors.noncoding = hvGfxFindColorIx(hvg, noncodingColor.r, noncodingColor.g, noncodingColor.b);
statusColors.problem = hvGfxFindColorIx(hvg, problemColor.r, problemColor.g, problemColor.b);
statusColors.unknown = hvGfxFindColorIx(hvg, unknownColor.r, unknownColor.g, unknownColor.b);
}

static Color getTranscriptStatusColor(struct hvGfx *hvg, struct gtexGeneBed *geneBed)
/* Find GENCODE color for transcriptClass  of canonical transcript */
{
initGeneColors(hvg);
if (geneBed->transcriptClass == NULL)
    return statusColors.unknown;
if (sameString(geneBed->transcriptClass, "coding"))
    return statusColors.coding;
if (sameString(geneBed->transcriptClass, "nonCoding"))
    return statusColors.noncoding;
if (sameString(geneBed->transcriptClass, "problem"))
    return statusColors.problem;
return statusColors.unknown;
}

/***********************************************/
/* Cache tissue info */

struct gtexTissue *getTissues()
/* Get and cache tissue metadata from database */
{
static struct gtexTissue *gtexTissues = NULL;

if (!gtexTissues)
    gtexTissues = gtexGetTissues();
return gtexTissues;
}

int getTissueCount()
/* Get and cache the number of tissues in GTEx tissue table */
{
static int tissueCount = 0;

if (!tissueCount)
    tissueCount = slCount(getTissues());
return tissueCount;
}

char *getTissueName(int id)
/* Get tissue name from id, cacheing */
{
static char **tissueNames = NULL;

struct gtexTissue *tissue;
int count = getTissueCount();
if (!tissueNames)
    {
    struct gtexTissue *tissues = getTissues();
    AllocArray(tissueNames, count);
    for (tissue = tissues; tissue != NULL; tissue = tissue->next)
        tissueNames[tissue->id] = cloneString(tissue->name);
    }
if (id >= count)
    errAbort("GTEx tissue table problem: can't find id %d\n", id);
return tissueNames[id];
}

struct rgbColor *getGtexTissueColors()
/* Get RGB colors from tissue table */
{
struct gtexTissue *tissues = getTissues();
struct gtexTissue *tissue = NULL;
int count = slCount(tissues);
struct rgbColor *colors;
AllocArray(colors, count);
int i = 0;
for (tissue = tissues; tissue != NULL; tissue = tissue->next)
    {
    // TODO: reconcile 
    colors[i] = (struct rgbColor){.r=COLOR_32_BLUE(tissue->color), .g=COLOR_32_GREEN(tissue->color), .b=COLOR_32_RED(tissue->color)};
    //colors[i] = mgColorIxToRgb(NULL, tissue->color);
    i++;
    }
return colors;
}

/*****************************************************************/
/* Load sample data, gene info, and anything else needed to draw */

static struct hash *loadGeneModels(char *table)
/* Load gene models from table */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
sr = hRangeQuery(conn, table, chromName, winStart, winEnd, NULL, &rowOffset);

struct hash *modelHash = newHash(0);
struct genePred *model = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    model = genePredLoad(row+rowOffset);
    hashAdd(modelHash, model->name, model);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return modelHash;
}

static void loadComputedMedians(struct gtexGeneInfo *geneInfo, struct gtexGeneExtras *extras)
/* Compute medians based on graph type.  Returns a list of 2 for comparison graph types */
{
struct gtexGeneBed *geneBed = geneInfo->geneBed;
int expCount = geneBed->expCount;
if (extras->isComparison)
    {
    // create two score hashes, one for each sample subset
    struct hash *scoreHash1 = hashNew(0), *scoreHash2 = hashNew(0);
    struct sqlConnection *conn = hAllocConn("hgFixed");
    char query[1024];
    char **row;
    sqlSafef(query, sizeof(query), "select gtexSampleData.sample, gtexDonor.gender, gtexSampleData.tissue, gtexSampleData.score from gtexSampleData, gtexSample, gtexDonor where gtexSampleData.geneId='%s' and gtexSampleData.sample=gtexSample.sampleId and gtexSample.donor=gtexDonor.name", geneBed->geneId);
    struct sqlResult *sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        char gender = *row[1];
        // TODO: generalize for other comparison graphs (this code just for M/F comparison)
        struct hash *scoreHash = ((gender == 'F') ? scoreHash1 : scoreHash2);
        char *tissue = cloneString(row[2]);
        struct slDouble *score = slDoubleNew(sqlDouble(row[3]));

        // create hash of lists of scores, keyed by tissue name
        double *tissueScores = hashFindVal(scoreHash, tissue);
        if (tissueScores)
            slAddHead(tissueScores, score);
        else
            hashAdd(scoreHash, tissue, score);
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);

    // get tissue medians for each sample subset
    double *medians1;
    double *medians2;
    AllocArray(medians1, expCount);
    AllocArray(medians2, expCount);
    int i;
    for (i=0; i<geneBed->expCount; i++)
        {
        //medians1[i] = -1, medians2[i] = -1;       // mark missing tissues ?
        struct slDouble *scores;
        scores = hashFindVal(scoreHash1, getTissueName(i));
        if (scores)
            medians1[i] = slDoubleMedian(scores);
        scores = hashFindVal(scoreHash2, getTissueName(i));
        if (scores)
            medians2[i] = slDoubleMedian(scores);
        }
    geneInfo->medians1 = medians1;
    geneInfo->medians2 = medians2;
    }
else
    {
    // TODO: compute median for single graph based on filtering of sample set
    }
}

static void gtexGeneLoadItems(struct track *tg)
/* Load method for track items */
{
/* Get track UI info */
struct gtexGeneExtras *extras;
AllocVar(extras);
tg->extraUiData = extras;
char *samples = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, 
                                                GTEX_SAMPLES, GTEX_SAMPLES_DEFAULT);
extras->graphType = cloneString(samples);
if (sameString(samples, GTEX_SAMPLES_COMPARE_SEX))
    extras->isComparison = TRUE;
extras->maxMedian = gtexMaxMedianScore(NULL);

/* Get geneModels in range */
//TODO: version the table name, move to lib
char *modelTable = "gtexGeneModel";
struct hash *modelHash = loadGeneModels(modelTable);

/* Get geneBeds (names and all-sample tissue median scores) in range */
bedLoadItem(tg, tg->table, (ItemLoader)gtexGeneBedLoad);

/* Create geneInfo items with BED and geneModels */
struct gtexGeneInfo *geneInfo = NULL, *list = NULL;
struct gtexGeneBed *geneBed = (struct gtexGeneBed *)tg->items;

/* Load tissue colors: GTEx or rainbow */
char *colorScheme = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, GTEX_COLORS, 
                        GTEX_COLORS_DEFAULT);
if (sameString(colorScheme, GTEX_COLORS_GTEX))
    {
    extras->colors = getGtexTissueColors();
    }
else
    {
    int expCount = geneBed->expCount;
    extras->colors = getRainbow(&saturatedRainbowAtPos, expCount);
    }
while (geneBed != NULL)
    {
    AllocVar(geneInfo);
    geneInfo->geneBed = geneBed;
    geneInfo->geneModel = hashFindVal(modelHash, geneBed->geneId);
    slAddHead(&list, geneInfo);
    geneBed = geneBed->next;
    geneInfo->geneBed->next = NULL;
    }
slReverse(&list);
tg->items = list;
}

/***********************************************/
/* Draw */

/* Bargraph layouts for three window sizes */
#define WIN_MAX_GRAPH 20000
#define MAX_GRAPH_HEIGHT 100
#define MAX_BAR_WIDTH 5
#define MAX_GRAPH_PADDING 2

#define WIN_MED_GRAPH 500000
#define MED_GRAPH_HEIGHT 60
#define MED_BAR_WIDTH 3
#define MED_GRAPH_PADDING 1

#define MIN_GRAPH_HEIGHT 20
#define MIN_BAR_WIDTH 1
#define MIN_GRAPH_PADDING 0

#define MARGIN_WIDTH 1

static int gtexBarWidth()
{
#ifdef MULTI_REGION
int winSize = virtWinBaseCount; // GALT CHANGED OLD winEnd - winStart;
#else
int winSize = winEnd - winStart;
#endif
if (winSize < WIN_MAX_GRAPH)
    return MAX_BAR_WIDTH;
else if (winSize < WIN_MED_GRAPH)
    return MED_BAR_WIDTH;
else
    return MIN_BAR_WIDTH;
}

static int gtexGraphPadding()
{
#ifdef MULTI_REGION
int winSize = virtWinBaseCount; // GALT CHANGED OLD winEnd - winStart;
#else
int winSize = winEnd - winStart;
#endif

if (winSize < WIN_MAX_GRAPH)
    return MAX_GRAPH_PADDING;
else if (winSize < WIN_MED_GRAPH)
    return MED_GRAPH_PADDING;
else
    return MIN_GRAPH_PADDING;
}

static int gtexGraphHeight()
{
#ifdef MULTI_REGION
int winSize = virtWinBaseCount; // GALT CHANGED OLD winEnd - winStart;
#else
int winSize = winEnd - winStart;
#endif
if (winSize < WIN_MAX_GRAPH)
    return MAX_GRAPH_HEIGHT;
else if (winSize < WIN_MED_GRAPH)
    return MED_GRAPH_HEIGHT;
else
    return MIN_GRAPH_HEIGHT;
}

static int gtexGraphWidth(struct gtexGeneInfo *geneInfo)
/* Width of GTEx graph in pixels */
{
int barWidth = gtexBarWidth();
int padding = gtexGraphPadding();
struct gtexGeneBed *geneBed = geneInfo->geneBed;
int count = geneBed->expCount;
int labelWidth = geneInfo->medians2 ? tl.mWidth : 0;
return (barWidth * count) + (padding * (count-1)) + labelWidth + 2;
}

static int gtexGraphX(struct gtexGeneBed *gtex)
/* Locate graph on X, relative to viewport. Return -1 if it won't fit */
{
int start = max(gtex->chromStart, winStart);
//int end = min(gtex->chromEnd, winEnd);
//if (start > end)
    //return -1;
double scale = scaleForWindow(insideWidth, winStart, winEnd);
int x1 = round((start - winStart) * scale);
//int x2 = round((end - winStart) * scale);
//int width = gtexGraphWidth(gtex);
//if (x1 + width > x2)
    //return -1;
return x1;
}

static int gtexGeneHeight()
{
    return 8; 
}

static int gtexGeneMargin()
{
    return 1;
}

static int valToHeight(double val, double maxVal, int maxHeight, boolean doLogTransform)
/* Log-scale and convert a value from 0 to maxVal to 0 to maxHeight-1 */
{
if (val == 0.0)
    return 0;
double scaled = 0.0;
if (doLogTransform)
    scaled = log10(val+1.0) / log10(maxVal+1.0);
else
    scaled = val/maxVal;
if (scaled < 0)
    warn("scaled=%f\n", scaled);
return (scaled * (maxHeight-1));
}

static void drawGraphBase(struct hvGfx *hvg, int x, int y, struct gtexGeneInfo *geneInfo)
/* Draw faint line under graph to delineate extent when bars are missing (tissue w/ 0 expression) */
{
Color lightGray = MAKECOLOR_32(0xD1, 0xD1, 0xD1);
int graphWidth = gtexGraphWidth(geneInfo);
hvGfxBox(hvg, x, y, graphWidth, 1, lightGray);
}

static void gtexGeneDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y, 
                double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw tissue expression bar graph over gene model. 
   Optionally, draw a second graph under gene, to compare sample sets */
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
boolean doLogTransform = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, GTEX_LOG_TRANSFORM, 
                                                GTEX_LOG_TRANSFORM_DEFAULT);
// Color in dense mode using transcriptClass
Color statusColor = getTranscriptStatusColor(hvg, geneBed);
if (vis != tvFull && vis != tvPack)
    {
    bedDrawSimpleAt(tg, geneBed, hvg, xOff, y, scale, font, statusColor, vis);
    return;
    }
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
if (extras->isComparison && (tg->visibility == tvFull || tg->visibility == tvPack))
        //&& gtexGraphHeight() != MIN_GRAPH_HEIGHT)
    // compute medians based on configuration (comparisons, and later, filters)
    loadComputedMedians(geneInfo, extras);

int heightPer = tg->heightPer;
int graphX = gtexGraphX(geneBed);
if (graphX < 0)
    return;
int yZero = gtexGraphHeight() + y-1;  // yZero is bottom of graph

#ifndef MULTI_REGION
int x1 = xOff + graphX;         // x1 is at left of graph

int keepX = x1;
drawGraphBase(hvg, keepX, yZero+1, geneInfo);

int startX = x1;
int i;
int expCount = geneBed->expCount;
double maxMedian = ((struct gtexGeneExtras *)tg->extraUiData)->maxMedian;
struct rgbColor lineColor = {.r=0};
int lineColorIx = hvGfxFindColorIx(hvg, lineColor.r, lineColor.g, lineColor.b);
int barWidth = gtexBarWidth();
int graphPadding = gtexGraphPadding();
char *colorScheme = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, GTEX_COLORS, 
                        GTEX_COLORS_DEFAULT);
Color labelColor = MG_GRAY;

// add labels to comparison graphs
// TODO: generalize
if (geneInfo->medians2)
    {
    hvGfxText(hvg, x1, yZero - tl.fontHeight, labelColor, font, "F");
    hvGfxText(hvg, x1, yZero + gtexGeneHeight() + gtexGeneMargin(), labelColor, font, "M");
    startX = startX + tl.mWidth+2;
    x1 = startX;
    }
for (i=0; i<expCount; i++)
    {
    struct rgbColor fillColor = extras->colors[i];
    if (barWidth == 1 && sameString(colorScheme, GTEX_COLORS_GTEX))
        {
        // brighten colors a bit so they'll be more visible at this scale
        fillColor = gtexTissueBrightenColor(fillColor);
        }
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);

    double expScore = (geneInfo->medians1 ? geneInfo->medians1[i] : geneBed->expScores[i]);
    int height = valToHeight(expScore, maxMedian, gtexGraphHeight(), doLogTransform);
    if (graphPadding == 0 || sameString(colorScheme, GTEX_COLORS_GTEX))
        hvGfxBox(hvg, x1, yZero-height+1, barWidth, height, fillColorIx);
    else
        hvGfxOutlinedBox(hvg, x1, yZero-height+1, barWidth, height, fillColorIx, lineColorIx);
    x1 = x1 + barWidth + graphPadding;
    }
#endif

// draw gene model
int yGene = yZero + gtexGeneMargin()-1;
tg->heightPer = gtexGeneHeight()+1;
struct linkedFeatures *lf = linkedFeaturesFromGenePred(tg, geneInfo->geneModel, FALSE);
lf->filterColor = statusColor;
linkedFeaturesDrawAt(tg, lf, hvg, xOff, yGene, scale, font, color, tvSquish);
tg->heightPer = heightPer;

if (!geneInfo->medians2)
    return;

#ifndef MULTI_REGION
// draw comparison graph (upside down)
x1 = startX;
yZero = yGene + gtexGeneHeight() + 1; // yZero is at top of graph
drawGraphBase(hvg, keepX, yZero-1, geneInfo);

for (i=0; i<expCount; i++)
    {
    struct rgbColor fillColor = extras->colors[i];
    if (barWidth == 1 && sameString(colorScheme, GTEX_COLORS_GTEX))
        {
        // brighten colors a bit so they'll be more visible at this scale
        struct hslColor hsl = mgRgbToHsl(fillColor);
        hsl.s = min(1000, hsl.s + 300);
        fillColor = mgHslToRgb(hsl);
        }
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    double expScore = geneInfo->medians2[i];
    int height = valToHeight(expScore, maxMedian, gtexGraphHeight(), doLogTransform);
    if (graphPadding == 0 || sameString(colorScheme, GTEX_COLORS_GTEX))
        hvGfxBox(hvg, x1, yZero, barWidth, height, fillColorIx);
    else
        hvGfxOutlinedBox(hvg, x1, yZero, barWidth, height, fillColorIx, lineColorIx);
    x1 = x1 + barWidth + graphPadding;
    }
#endif
}

#ifdef MULTI_REGION
static int gtexGeneNonPropPixelWidth(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
int graphWidth = gtexGraphWidth(geneInfo);
return graphWidth;
}
#endif

#ifdef MULTI_REGION
static void gtexGeneNonPropDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y,
                double scale, MgFont *font, Color color, enum trackVisibility vis)
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;

// Color in dense mode using transcriptClass
// GALT REMOVE Color statusColor = getTranscriptStatusColor(hvg, geneBed);
if (vis != tvFull && vis != tvPack)
    {
    //GALT bedDrawSimpleAt(tg, geneBed, hvg, xOff, y, scale, font, statusColor, vis);
    return;
    }
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
if (extras->isComparison && (tg->visibility == tvFull || tg->visibility == tvPack))
        //&& gtexGraphHeight() != MIN_GRAPH_HEIGHT)
    // compute medians based on configuration (comparisons, and later, filters)
    loadComputedMedians(geneInfo, extras);
int i;
int expCount = geneBed->expCount;
double maxMedian = ((struct gtexGeneExtras *)tg->extraUiData)->maxMedian;
struct rgbColor lineColor = {.r=0};
int lineColorIx = hvGfxFindColorIx(hvg, lineColor.r, lineColor.g, lineColor.b);
// GALT REMOVE int heightPer = tg->heightPer;

int graphX = gtexGraphX(geneBed);
if (graphX < 0)
    return;
int x1 = xOff + graphX; // x1 is at left of graph
int startX = x1;
int yZero = gtexGraphHeight() + y - 1; // yZero is at bottom of graph

// draw faint line under graph to delineate extent when bars are missing (tissue w/ 0 expression)
// TODO: skip missing bars
Color lightGray = MAKECOLOR_32(0xD1, 0xD1, 0xD1);
int graphWidth = gtexGraphWidth(geneInfo);
hvGfxBox(hvg, x1, yZero+1, graphWidth, 1, lightGray);

int barWidth = gtexBarWidth();
int graphPadding = gtexGraphPadding();

char *colorScheme = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, GTEX_COLORS,
                        GTEX_COLORS_DEFAULT);
Color labelColor = MG_GRAY;

if (geneInfo->medians2)
    {
    // add labels to comparison graphs
    // TODO: generalize
    hvGfxText(hvg, x1, yZero-tl.fontHeight, labelColor, font, "F");
    hvGfxText(hvg, x1, yZero + gtexGeneHeight() + gtexGeneMargin(), labelColor, font, "M");
    startX = startX + tl.mWidth + 2;
    x1 = startX;
    }
for (i=0; i<expCount; i++)
    {
    struct rgbColor fillColor = extras->colors[i];
    if (barWidth == 1 && sameString(colorScheme, GTEX_COLORS_GTEX))
        {
        // brighten colors a bit so they'll be more visible at this scale
        fillColor = gtexTissueBrightenColor(fillColor);
        }
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);

    double expScore = (geneInfo->medians1 ? geneInfo->medians1[i] : geneBed->expScores[i]);
    int height = valToHeight(expScore, maxMedian, gtexGraphHeight(), doLogTransform);
    if (graphPadding == 0 || sameString(colorScheme, GTEX_COLORS_GTEX))
        hvGfxBox(hvg, x1, yZero-height, barWidth, height, fillColorIx);
    else
        hvGfxOutlinedBox(hvg, x1, yZero-height, barWidth, height, fillColorIx, lineColorIx);
    x1 = x1 + barWidth + graphPadding;
    }

// mark gene extent
int yGene = yZero + gtexGeneMargin() - 1;

/* GALT NOT DONE HERE NOW
// draw gene model
tg->heightPer = gtexGeneHeight()+1;
struct linkedFeatures *lf = linkedFeaturesFromGenePred(tg, geneInfo->geneModel, FALSE);
lf->filterColor = statusColor;
// GALT linkedFeaturesDrawAt(tg, lf, hvg, xOff, yGene, scale, font, color, tvSquish);
tg->heightPer = heightPer;
*/
if (!geneInfo->medians2)
    return;
// draw comparison graph (upside down)
x1 = startX;
yZero = yGene + gtexGeneHeight(); // yZero is at top of graph
for (i=0; i<expCount; i++)
    {
    struct rgbColor fillColor = extras->colors[i];
    if (barWidth == 1 && sameString(colorScheme, GTEX_COLORS_GTEX))
        {
        // brighten colors a bit so they'll be more visible at this scale
        struct hslColor hsl = mgRgbToHsl(fillColor);
        hsl.s = min(1000, hsl.s + 300);
        fillColor = mgHslToRgb(hsl);
        }
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    double expScore = geneInfo->medians2[i];
    int height = valToHeight(expScore, maxMedian, gtexGraphHeight(), doLogTransform);
    if (graphPadding == 0 || sameString(colorScheme, GTEX_COLORS_GTEX))
        hvGfxBox(hvg, x1, yZero, barWidth, height, fillColorIx);
    else
        hvGfxOutlinedBox(hvg, x1, yZero, barWidth, height, fillColorIx, lineColorIx);
    x1 = x1 + barWidth + graphPadding;
    }
}
#endif

static void gtexGeneMapItem(struct track *tg, struct hvGfx *hvg, void *item, char *itemName, 
                        char *mapItemName, int start, int end, int x, int y, int width, int height)
/* Create a map box for each tissue (bar in the graph) or a single map for squish/dense modes */
{
if (tg->visibility == tvDense || tg->visibility == tvSquish)
    {
    genericMapItem(tg, hvg, item, itemName, itemName, start, end, x, y, width, height);
    return;
    }
struct gtexTissue *tissues = getTissues();
struct gtexTissue *tissue = NULL;
struct gtexGeneInfo *geneInfo = item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
int barWidth = gtexBarWidth();
int padding = gtexGraphPadding();
double maxMedian = ((struct gtexGeneExtras *)tg->extraUiData)->maxMedian;

int graphX = gtexGraphX(geneBed);
if (graphX < 0)
    return;
// x1 is at left of graph
int x1 = insideX + graphX;

if (geneInfo->medians2)
    {
    // skip over labels in comparison graphs
    x1 = x1 + tl.mWidth+ 2;
    }
int i = 0;
int yZero = gtexGraphHeight() + y - 1;
boolean doLogTransform = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, GTEX_LOG_TRANSFORM, 
                                                GTEX_LOG_TRANSFORM_DEFAULT);
for (tissue = tissues; tissue != NULL; tissue = tissue->next, i++)
    {
    double expScore = geneBed->expScores[i];
    int height = valToHeight(expScore, maxMedian, gtexGraphHeight(), doLogTransform);
    int yMedian = yZero - height;
    mapBoxHc(hvg, start, end, x1, yMedian+1, barWidth, height, tg->track, mapItemName, tissue->description);
    // add map box to comparison graph
    if (geneInfo->medians2)
        {
        double expScore = geneInfo->medians2[i];
        int height = valToHeight(expScore, maxMedian, gtexGraphHeight(), doLogTransform);
        int y = yZero + gtexGeneHeight() + gtexGeneMargin();
        mapBoxHc(hvg, start, end, x1, y, barWidth, height, tg->track, mapItemName, tissue->description);
        }
    x1 = x1 + barWidth + padding;
    }
}

static char *gtexGeneItemName(struct track *tg, void *item)
/* Return gene name */
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
return geneBed->name;
}

static int gtexGeneItemHeight(struct track *tg, void *item)
{
if ((item == NULL) || (tg->visibility == tvSquish) || (tg->visibility == tvDense))
    return 0;
int extra = 0;
if (((struct gtexGeneExtras *)tg->extraUiData)->isComparison)
    extra = gtexGraphHeight() + 2;
//uglyf("GTEX itemHeight extra = %d<br>", extra);
return gtexGraphHeight() + gtexGeneMargin() + gtexGeneHeight() + extra;
}

static int gtexTotalHeight(struct track *tg, enum trackVisibility vis)
/* Figure out total height of track */
{
int height;
int extra = 0;
if (((struct gtexGeneExtras *)tg->extraUiData)->isComparison)
    extra = gtexGraphHeight() + 2;
if (tg->visibility == tvSquish || tg->visibility == tvDense)
    height = 10;
else 
    height = gtexGraphHeight() + gtexGeneMargin() + gtexGeneHeight() + extra;
//uglyf("GTEX totalHeight = %d<br>", height);
return tgFixedTotalHeightOptionalOverflow(tg, vis, height, height, FALSE);
}

static int gtexGeneItemStart(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
return geneBed->chromStart;
}

static int gtexGeneItemEnd(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
double scale = scaleForWindow(insideWidth, winStart, winEnd);
int graphWidth = gtexGraphWidth(geneInfo);
return max(geneBed->chromEnd, max(winStart, geneBed->chromStart) + graphWidth/scale);
}

void gtexGeneMethods(struct track *tg)
{
tg->drawItemAt = gtexGeneDrawAt;
tg->loadItems = gtexGeneLoadItems;
//tg->freeItems = gtexGeneFreeItems;
tg->mapItem = gtexGeneMapItem;
tg->itemName = gtexGeneItemName;
tg->mapItemName = gtexGeneItemName;
tg->itemHeight = gtexGeneItemHeight;
tg->itemStart = gtexGeneItemStart;
tg->itemEnd = gtexGeneItemEnd;
tg->totalHeight = gtexTotalHeight;
#ifdef MULTI_REGION
tg->nonPropDrawItemAt = gtexGeneNonPropDrawAt;
tg->nonPropPixelWidth = gtexGeneNonPropPixelWidth;
#endif
}



