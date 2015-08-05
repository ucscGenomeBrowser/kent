/* GTEX tracks  */

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

//#define GENCODE_CODING_COLOR 0x0c0c78    // rgb(12,12,120)
//#define GENCODE_NONCODING_COLOR 0x006400 // rgb(0,100,0)
//#define GENCODE_PROBLEM_COLOR 0xfe000  //rgb(254,0,0)  

#define GENCODE_CODING_COLOR "12,12,120"
#define GENCODE_NONCODING_COLOR "0,100,0"
#define GENCODE_PROBLEM_COLOR "254,0,0"

#define UNKNOWN_COLOR "1,1,1"

static struct statusColors
    {
    Color coding;
    Color nonCoding;
    Color problem;
    Color unknown;
    } statusColors = {0,0,0};

static int findColorIx(struct hvGfx *hvg, char *rgb)
{
unsigned char red, green, blue;
parseColor(rgb, &red, &green, &blue);
return (hvGfxFindColorIx(hvg, red, green, blue));
//return MAKECOLOR_32(red, green, blue);
}

static void initGeneColors(struct hvGfx *hvg)
{
if (statusColors.coding != 0)
    return;
statusColors.coding = findColorIx(hvg, GENCODE_CODING_COLOR);
statusColors.nonCoding = findColorIx(hvg, GENCODE_NONCODING_COLOR);
statusColors.problem = findColorIx(hvg, GENCODE_PROBLEM_COLOR);
statusColors.unknown = findColorIx(hvg, UNKNOWN_COLOR);
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
    return statusColors.nonCoding;
if (sameString(geneBed->transcriptClass, "problem"))
    return statusColors.problem;
return statusColors.unknown;
}

static int gtexBarWidth()
{
int winSize = winEnd - winStart;
if (winSize < WIN_MAX_GRAPH)
    return MAX_BAR_WIDTH;
else if (winSize < WIN_MED_GRAPH)
    return MED_BAR_WIDTH;
else
    return MIN_BAR_WIDTH;
}

static int gtexGraphPadding()
{
int winSize = winEnd - winStart;
if (winSize < WIN_MAX_GRAPH)
    return MAX_GRAPH_PADDING;
else if (winSize < WIN_MED_GRAPH)
    return MED_GRAPH_PADDING;
else
    return MIN_GRAPH_PADDING;
}

static int gtexGraphHeight()
{
int winSize = winEnd - winStart;
if (winSize < WIN_MAX_GRAPH)
    return MAX_GRAPH_HEIGHT;
else if (winSize < WIN_MED_GRAPH)
    return MED_GRAPH_HEIGHT;
else
    return MIN_GRAPH_HEIGHT;
}

static int gtexGraphWidth(struct gtexGeneBed *gtex)
/* Width of GTEx graph in pixels */
{
int barWidth = gtexBarWidth();
int padding = gtexGraphPadding();
int count = gtex->expCount;
return (barWidth * count) + (padding * (count-1));
}


static int valToHeight(double val, double maxVal, int maxHeight)
/* Log-scale and Convert a value from 0 to maxVal to 0 to maxHeight-1 */
// TODO: support linear or log scale
{
if (val == 0.0)
    return 0;
// smallest counts are 1x10e-3, translate to counter negativity
double scaled = (log10(val) + 3.001)/(log10(maxVal) + 3.001);
if (scaled < 0)
    warn("scaled=%f\n", scaled);
//uglyf("%.2f -> %.2f height %d", val, scaled, (int)scaled * (maxHeight-1));
return (scaled * (maxHeight-1));
}

// TODO: whack this
static int valToY(double val, double maxVal, int maxHeight)
/* Convert a value from 0 to maxVal to 0 to height-1 */
{
if (val == 0.0)
    return 0;
double scaled = (log10(val)+3.001)/(log10(maxVal)+3.001);
if (scaled < 0)
    warn("scaled=%f\n", scaled);
int y = scaled * (maxHeight);
//uglyf("%.2f -> %.2f height %d", val, scaled, (int)scaled * (maxHeight-1));
return (maxHeight-1) - y;
}


/* Cache tissue metadata */

struct gtexTissue *getTissues()
/* Get tissue metadata from database */
{
static struct gtexTissue *gtexTissues = NULL;
if (gtexTissues == NULL)
    gtexTissues = gtexGetTissues();
return gtexTissues;
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

/* Track info generated during load, used by draw */

struct gtexGeneExtras 
/* Track info */
    {
    double maxMedian;
    char *graphType;
    boolean isComparison;
    struct rgbColor *colors;
    };

struct gtexGeneInfo
/* GTEx gene model, names, and expression medians */
    {
    struct gtexGeneInfo *next;  /* Next in singly linked list */
    struct gtexGeneBed *geneBed;/* Gene name, id, canonical transcript, exp count and medians 
                                        from BED table */
    struct genePred *geneModel; /* Gene structure from model table */
    float *medians1;            /* Computed medians */
    float *medians2;            /* Computed medians for comparison (inverse) graph */
    };

static struct gtexGeneBed *loadComputedMedians(struct gtexGeneBed *geneBed, char *graphType)
/* Compute medians based on graph type.  Returns a list of 2 for comparison graph types */
/* TODO: add support for filter function */
{
/* FIXME: dummy load of two for display implementation */
struct gtexGeneBed *medians = NULL, *medians2 = NULL;

AllocVar(medians);
medians->expCount = geneBed->expCount;
int i;

#ifdef NEW
struct sqlConnection *conn = hAllocConn("hgFixed");
if (conn == NULL)
    return NULL;
char query[1024];

// FIXME: experiment with query on sex

sqlSafef(query, sizeof(query), "select gtexTissue.id, gtexSampleData.score from gtexTissue, gtexSampleData, gtexSample, gtexDonor where gtexSampleData.tissue=gtexTissue.name and gtexSampleData.geneId='%s' and gtexSampleData.sample=gtexSample.name and gtexSample.donor=gtexDonor.name and gtexDonor.gender='F'", geneBed->geneId);
struct slDouble **scores = NULL, *score = NULL;
AllocArray(scores, geneBed->expCount);
struct slPair *tissueScore = NULL, *tissueScores = sqlQuickPairList(conn, query);
for (tissueScore = tissueScores; tissueScore != NULL; tissueScore = tissueScore->next)
    {
    AllocVar(score);
    i = sqlUnsigned(tissueScore->name);
    if (scores[i] == NULL)
        scores[i] = score;
    else
        slAddHead(&scores[i], score);
    }
hFreeConn(&conn);
#endif

AllocArray(medians->expScores, medians->expCount);
for (i=0; i<geneBed->expCount; i++)
    //medians->expScores[i] = slDoubleMedian(scores[i]);
    medians->expScores[i] = geneBed->expScores[i];

AllocVar(medians2);
medians2->expCount = geneBed->expCount;
AllocArray(medians2->expScores, medians2->expCount);
for (i = 0; i < medians2->expCount; ++i)
    medians2->expScores[i] = geneBed->expScores[i];

medians->next = medians2;

return medians;
}

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

static void gtexGeneLoadItems(struct track *tg)
{
// Get track UI info
struct gtexGeneExtras *extras;
AllocVar(extras);
tg->extraUiData = extras;

// TODO: move test to lib
char *graphType = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, GTEX_GRAPH, 
                                                GTEX_GRAPH_DEFAULT);
extras->graphType = cloneString(graphType);
if (sameString(graphType, GTEX_GRAPH_AGE) || sameString(graphType, GTEX_GRAPH_SEX))
    extras->isComparison = TRUE;

extras->maxMedian = gtexMaxMedianScore(NULL);

// Construct track items

// Get geneModels in range
//TODO: version the table name ?
char *modelTable = "gtexGeneModel";
struct hash *modelHash = loadGeneModels(modelTable);

// Get geneBeds (names and all-sample tissue median scores) in range
bedLoadItem(tg, tg->table, (ItemLoader)gtexGeneBedLoad);

// Create geneInfo items with BED and geneModels attached
struct gtexGeneInfo *geneInfo = NULL, *list = NULL;
struct gtexGeneBed *geneBed = (struct gtexGeneBed *)tg->items;

char *colorScheme = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, GTEX_COLORS, 
                        GTEX_COLORS_DEFAULT);
if (sameString(colorScheme, GTEX_COLORS_GTEX))
    {
    // retrieve from table
    extras->colors = getGtexTissueColors();
    }
else
    {
    // currently the only other choice
    int expCount = geneBed->expCount;
    extras->colors = getRainbow(&saturatedRainbowAtPos, expCount);
    //colors = getRainbow(&lightRainbowAtPos, expCount);
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

static void gtexGeneDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y, 
                double scale, MgFont *font, Color color, enum trackVisibility vis)
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;

// Color in dense mode using transcriptClass
Color statusColor = getTranscriptStatusColor(hvg, geneBed);
if (vis != tvFull && vis != tvPack)
    {
    bedDrawSimpleAt(tg, geneBed, hvg, xOff, y, scale, font, statusColor, vis);
    return;
    }
struct gtexGeneBed *computedMedians = NULL;     // 1 or 2 (if comparison) 
                                                // with medians computed for sample subsets

struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
if ((extras->isComparison) &&
        (tg->visibility == tvFull || tg->visibility == tvPack))
        //&& gtexGraphHeight() != MIN_GRAPH_HEIGHT)
    {
    // compute medians based on configuration (comparisons, and later, filters)
    computedMedians = loadComputedMedians(geneBed, extras->graphType);
    }
int i;
int expCount = geneBed->expCount;
double maxMedian = ((struct gtexGeneExtras *)tg->extraUiData)->maxMedian;
struct rgbColor lineColor = {.r=0};
int lineColorIx = hvGfxFindColorIx(hvg, lineColor.r, lineColor.g, lineColor.b);
int heightPer = tg->heightPer;

int graphX = gtexGraphX(geneBed);
if (graphX < 0)
    return;
// x1 is at left of graph
int x1 = xOff + graphX;
int startX = x1;

// yZero is at bottom of graph
int yZero = gtexGraphHeight() + y - 1;

// draw faint line under graph to delineate extent when bars are missing (tissue w/ 0 expression)
// TODO: skip missing bars -- then we can lose the gray line (at least for non-comparison mode)
Color lightGray = MAKECOLOR_32(0xD1, 0xD1, 0xD1);
int graphWidth = gtexGraphWidth(geneBed);
hvGfxBox(hvg, x1, yZero+1, graphWidth, 1, lightGray);

//uglyf("DRAW: xOff=%d, x1=%d, y=%d, yZero=%d<br>", xOff, x1, y, yZero);

int barWidth = gtexBarWidth();
int graphPadding = gtexGraphPadding();

// Move to loader
char *colorScheme = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, GTEX_COLORS, 
                        GTEX_COLORS_DEFAULT);
for (i=0; i<expCount; i++)
    {
    struct rgbColor fillColor = extras->colors[i];
    if (barWidth == 1 && sameString(colorScheme, GTEX_COLORS_GTEX))
        {
        // brighten colors a bit so they'll be more visible at this scale
        fillColor = gtexTissueBrightenColor(fillColor);
        }
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    double expScore = geneBed->expScores[i];
    int height = valToHeight(expScore, maxMedian, gtexGraphHeight());
    // TODO: adjust yGene to get gene track distance as desired
    //if (i ==0) uglyf("DRAW: expScore=%.2f, maxMedian=%.2f, graphHeight=%d, y=%d<br>", expScore, maxMedian, gtexGraphHeight(), y);
    //if (i ==0) uglyf("DRAW: yZero=%d, yMedian=%d, height=%d<br>", yZero, yMedian, height);
    if (graphPadding == 0 || sameString(colorScheme, GTEX_COLORS_GTEX))
        hvGfxBox(hvg, x1, yZero-height, barWidth, height, fillColorIx);
    else
        hvGfxOutlinedBox(hvg, x1, yZero-height, barWidth, height, fillColorIx, lineColorIx);
    x1 = x1 + barWidth + graphPadding;
    }

// mark gene extent
int yGene = yZero + gtexGeneMargin() - 1;


// draw gene model
tg->heightPer = gtexGeneHeight()+1;
struct linkedFeatures *lf = linkedFeaturesFromGenePred(tg, geneInfo->geneModel, FALSE);
lf->filterColor = statusColor;
linkedFeaturesDrawAt(tg, lf, hvg, xOff, yGene, scale, font, color, tvSquish);
tg->heightPer = heightPer;

if (!extras->isComparison || slCount(computedMedians) != 2)
    return;

// draw comparison graph (upside down)

x1 = startX;
// yZero is at top of graph
yZero = yGene + gtexGeneHeight();
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
    double expScore = geneBed->expScores[i];
    int height = valToHeight(expScore, maxMedian, gtexGraphHeight());
    // TODO: adjust yGene instead of yMedian+1 to get gene track distance as desired
    //if (i ==0) uglyf("DRAW2: expScore=%.2f, maxMedian=%.2f, graphHeight=%d, y=%d<br>", expScore, maxMedian, gtexGraphHeight(), y);
    //if (i ==0) uglyf("DRAW2: yZero=%d, height=%d<br>", yZero, height);
    if (graphPadding == 0 || sameString(colorScheme, GTEX_COLORS_GTEX))
        hvGfxBox(hvg, x1, yZero, barWidth, height, fillColorIx);
    else
        hvGfxOutlinedBox(hvg, x1, yZero, barWidth, height, fillColorIx, lineColorIx);
    x1 = x1 + barWidth + graphPadding;
    }
}

static void gtexGeneMapItem(struct track *tg, struct hvGfx *hvg, void *item, char *itemName, 
                        char *mapItemName, int start, int end, int x, int y, int width, int height)
/* Create a map box for each tissue (bar in the graph) or a single map for squish/dense modes */
{
//uglyf("map item: itemName=%s, mapItemName=%s, start=%d, end=%d, x=%d, y=%d, width=%d, height=%d, insideX=%d<br>",
        //itemName, mapItemName, start, end, x, y, width, height, insideX);

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

int i = 0;
int yZero = gtexGraphHeight() + y - 1;
for (tissue = tissues; tissue != NULL; tissue = tissue->next, i++)
    {
    double expScore = geneBed->expScores[i];
    //TODO: use valToHeight
    int yMedian = valToY(expScore, maxMedian, gtexGraphHeight()) + y;
    int height = yZero - yMedian;
    // TODO: call genericMapItem
    //genericMapItem(tg, hvg, item, itemName, tissue->description, start, end, x1, y, barWidth, height);
    mapBoxHc(hvg, start, end, x1, yMedian+1, barWidth, height, tg->track, mapItemName, tissue->description);
    //if (i==0) uglyf("MAP: expScore=%.2f, maxMedian=%.2f, graphHeight=%d, y=%d<br>", expScore, maxMedian, gtexGraphHeight(), y);
    //if (i==0) uglyf("MAP: x=%d, x1=%d, y=%d, yZero=%d<br>", x, x1, y, yZero); 
    //if (i==0) uglyf("MAP: yZero=%d, yMedian=%d, height=%d<br>", yZero, yMedian, height); 
    x1 = x1 + barWidth + padding;
    }
}

#ifdef OLD
static struct gtexGeneBed *loadGtexGeneBed(char **row)
{
struct gtexGeneBed *geneBed = gtexGeneBedLoad(row);
// TODO: rethink schemas
// for now... replace expScores with medians from tissue data file

struct gtexTissue *tissue = NULL, *tissues = getTissues();
int i=0;
char query[1024];
struct sqlConnection *conn = hAllocConn("hgFixed");
for (tissue = tissues; tissue != NULL; tissue = tissue->next, i++)
    {
    sqlSafef(query, sizeof(query), 
            "select * from gtexTissueData where geneId='%s' and tissue='%s'", 
                geneBed->geneId, tissue->name);
    struct gtexTissueData *tissueData = gtexTissueDataLoadByQuery(conn, query);
    geneBed->expScores[i] = tissueData->median;
    }
hFreeConn(&conn);
return geneBed;
}
#endif

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
int graphWidth = gtexGraphWidth(geneBed);
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
}



