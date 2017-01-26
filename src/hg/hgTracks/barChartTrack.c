/* barGraph tracks - display a colored bargraph across each region in a file */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hgTracks.h"
#include "bed.h"
#include "hvGfx.h"
#include "spaceSaver.h"
#include "rainbow.h"
#include "barChartBed.h"
#include "barChartCategory.h"
#include "barChartUi.h"


struct barChartTrack
/* Track info */
    {
    boolean noWhiteout;         /* Suppress whiteout of graph background (allow highlight, blue lines) */
    double maxMedian;           /* Maximum median across all categories */
    struct rgbColor *colors;    /* Colors  for all categories */
    boolean doLogTransform;     /* Log10(x+1) */
    struct barChartCategory *categories; /* Cache category names, colors */
    struct hash *categoryFilter;  /* NULL out excluded factors */
    };

struct barChartItem
/* BED item plus computed values for display */
    {
    struct barChartItem *next;  /* Next in singly linked list */
    struct barChartBed *bed;    /* Item coords, name, exp count and values */
    int height;                 /* Item height in pixels */
    };

/***********************************************/
/* Cache category info */

struct barChartCategory *getCategories(char *track)
/* Get and cache category info */
{
static struct barChartCategory *categs = NULL;

if (!categs)
    categs = barChartGetCategories(database, track);
return categs;
}

int getCategoryCount(char *track)
/* Get and cache the number of categories */
{
static int categCount = 0;

if (!categCount)
    categCount = slCount(getCategories(track));
return categCount;
}

/* TODO: Do we need names ? */

char *getCategoryName(char *track, int id)
/* Get category name from id, cacheing */
{
static char **categNames = NULL;
struct barChartCategory *categ;
int count = getCategoryCount(track);
if (!categNames)
    {
    struct barChartCategory *categs = getCategories(track);
    AllocArray(categNames, count);
    for (categ = categs; categ != NULL; categ = categ->next)
        categNames[categ->id] = cloneString(categ->name);
    }
if (id >= count)
    errAbort("Bar chart track: can't find id %d\n", id);
return categNames[id];
}

char *getCategoryLabel(char *track, int id)
/* Get category descriptive label from id, cacheing */
{
static char **categLabels = NULL;
struct barChartCategory *categ;
int count = getCategoryCount(track);
if (!categLabels)
    {
    struct barChartCategory *categs = getCategories(track);
    AllocArray(categLabels, count);
    for (categ = categs; categ != NULL; categ = categ->next)
        categLabels[categ->id] = cloneString(categ->label);
    }
if (id >= count)
    errAbort("Bar chart track: can't find id %d\n", id);
return categLabels[id];
}

struct rgbColor *getCategoryColors(char *track)
/* Get RGB colors from category table */
{
struct barChartCategory *categs = getCategories(track);
struct barChartCategory *categ = NULL;
int count = slCount(categs);
struct rgbColor *colors;
AllocArray(colors, count);
int i = 0;
for (categ = categs; categ != NULL; categ = categ->next)
    {
    // TODO: reconcile 
    colors[i] = (struct rgbColor){.r=COLOR_32_BLUE(categ->color), .g=COLOR_32_GREEN(categ->color), .b=COLOR_32_RED(categ->color)};
    //colors[i] = mgColorIxToRgb(NULL, categ->color);
    i++;
    }
return colors;
}

/*****************************************************************/
/* Convenience functions for draw */

static int barChartSquishItemHeight()
/* Height of squished item (request to have it larger than usual) */
{
return tl.fontHeight - tl.fontHeight/2;
}

static int barChartBoxModelHeight()
/* Height of indicator box drawn under graph to show gene extent */
{
long winSize = virtWinBaseCount;
//FIXME: dupes!!

// TODO: trackDb settings ?
#define WIN_MAX_GRAPH 50000
#define WIN_MED_GRAPH 500000

#define MAX_BAR_CHART_MODEL_HEIGHT     2
#define MED_BAR_CHART_MODEL_HEIGHT     2
#define MIN_BAR_CHART_MODEL_HEIGHT     1

if (winSize < WIN_MAX_GRAPH)
    return MAX_BAR_CHART_MODEL_HEIGHT;
else if (winSize < WIN_MED_GRAPH)
    return MED_BAR_CHART_MODEL_HEIGHT;
else
    return MIN_BAR_CHART_MODEL_HEIGHT;
}

static int barChartItemHeight(struct track *tg, void *item);

static void filterCategories(struct track *tg)
/* Check cart for category selection.  NULL out unselected categorys in category list */
{
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
struct barChartCategory *categ = NULL;
extras->categories = getCategories(tg->table);
extras->categoryFilter = hashNew(0);
if (cartListVarExistsAnyLevel(cart, tg->tdb, FALSE, BAR_CHART_CATEGORY_SELECT))
    {
    struct slName *selectedValues = cartOptionalSlNameListClosestToHome(cart, tg->tdb, 
                                                        FALSE, BAR_CHART_CATEGORY_SELECT);
    if (selectedValues != NULL)
        {
        struct slName *name;
        for (name = selectedValues; name != NULL; name = name->next)
            hashAdd(extras->categoryFilter, name->name, name->name);
        return;
        }
    }
/* no filter */
for (categ = extras->categories; categ != NULL; categ = categ->next)
    hashAdd(extras->categoryFilter, categ->name, categ->name);
}

static int filteredCategoryCount(struct track *tg)
/* Count of categories to display */
{
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
return hashNumEntries(extras->categoryFilter);
}

static boolean filterCategory(struct track *tg, char *name)
/* Does category pass filter */
{
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
return (hashLookup(extras->categoryFilter, name) != NULL);
}

static int maxCategoryForItem(struct barChartBed *bed, int threshold)
/* Return id of highest valued category for an item, if significantly higher than median.
 * If none are over threshold, return -1 */
{
double maxScore = 0.0, expScore;
double totalScore = 0.0;
int maxNum = 0, i;
int expCount = bed->expCount;
for (i=0; i<expCount; i++)
    {
    expScore = bed->expScores[i];
    if (expScore > maxScore)
        {
        maxScore = max(maxScore, expScore);
        maxNum = i;
        }
    totalScore += expScore;
    }
// threshold to consider this item category specific -- a category contributes > threshold % to 
// total level
if (totalScore < 1 || maxScore <= (totalScore * threshold * .01))
    return -1;
return maxNum;
}

static Color barChartItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* A bit of category-specific coloring in squish mode only, on bed item */
// TODO: Need a good function here to pick threshold from category count. 
//      Also maybe trackDb setting
{
struct barChartBed *bed = (struct barChartBed *)item;
int threshold = 10;
int id = maxCategoryForItem(bed, threshold);
if (id < 0)
    return MG_BLACK;
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
struct rgbColor color = extras->colors[id];
return hvGfxFindColorIx(hvg, color.r, color.g, color.b);
}

static void barChartLoadItems(struct track *tg)
/* Load method for track items */
{
if (tg->visibility == tvSquish || tg->limitedVis == tvSquish)
    tg->itemColor = barChartItemColor;

/* Get track UI info */
struct barChartTrack *extras;
AllocVar(extras);
tg->extraUiData = extras;

extras->doLogTransform = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, BAR_CHART_LOG_TRANSFORM, 
                                                BAR_CHART_LOG_TRANSFORM_DEFAULT);
extras->maxMedian = barChartUiMaxMedianScore();
extras->noWhiteout = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, BAR_CHART_NO_WHITEOUT,
                                                        BAR_CHART_NO_WHITEOUT_DEFAULT);

/* Get bed (names and all-sample category median scores) in range */
char *filter = getScoreFilterClause(cart, tg->tdb, NULL);
bedLoadItemWhere(tg, tg->table, filter, (ItemLoader)barChartBedLoad);

/* Create itemInfo items with BED and geneModels */
struct barChartItem *itemInfo = NULL, *list = NULL;
struct barChartBed *bed = (struct barChartBed *)tg->items;

/* Load category colors */
#ifdef COLOR_SCHEME
char *colorScheme = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, BAR_CHART_COLORS, 
                        BAR_CHART_COLORS_DEFAULT);
#else
char *colorScheme = BAR_CHART_COLORS_DEFAULT;
#endif
if (sameString(colorScheme, BAR_CHART_COLORS_USER))
    {
    extras->colors = getCategoryColors(tg->table);
    }
else
    {
    if (bed)
	{
	int expCount = bed->expCount;
	extras->colors = getRainbow(&saturatedRainbowAtPos, expCount);
	}
    }
filterCategories(tg);

while (bed != NULL)
    {
    AllocVar(itemInfo);
    itemInfo->bed = bed;
    slAddHead(&list, itemInfo);
    bed = bed->next;
    itemInfo->bed->next = NULL;
    itemInfo->height = barChartItemHeight(tg, itemInfo);
    }
slReverse(&list);
tg->items = list;
}

/***********************************************/
/* Draw */

/* Bargraph layouts for three window sizes */
#define WIN_MAX_GRAPH 50000
#define MAX_GRAPH_HEIGHT 175
#define MAX_BAR_WIDTH 5
#define MAX_GRAPH_PADDING 2

#define WIN_MED_GRAPH 500000
#define MED_GRAPH_HEIGHT 100
#define MED_BAR_WIDTH 3
#define MED_GRAPH_PADDING 1

#define MIN_BAR_WIDTH 1
#define MIN_GRAPH_PADDING 0

#define MARGIN_WIDTH 1


static int barChartBarWidth()
{
long winSize = virtWinBaseCount;
if (winSize < WIN_MAX_GRAPH)
    return MAX_BAR_WIDTH;
else if (winSize < WIN_MED_GRAPH)
    return MED_BAR_WIDTH;
else
    return MIN_BAR_WIDTH;
}

static int barChartModelHeight(struct barChartTrack *extras)
{
return barChartBoxModelHeight()+3;
}

static int barChartPadding()
{
long winSize = virtWinBaseCount;

if (winSize < WIN_MAX_GRAPH)
    return MAX_GRAPH_PADDING;
else if (winSize < WIN_MED_GRAPH)
    return MED_GRAPH_PADDING;
else
    return MIN_GRAPH_PADDING;
}

static int barChartMaxHeight()
{
long winSize = virtWinBaseCount;
if (winSize < WIN_MAX_GRAPH)
    return MAX_GRAPH_HEIGHT;
else if (winSize < WIN_MED_GRAPH)
    return MED_GRAPH_HEIGHT;
else
    return tl.fontHeight * 4;
}

static int barChartWidth(struct track *tg, struct barChartItem *itemInfo)
/* Width of bar chart in pixels */
{
int barWidth = barChartBarWidth();
int padding = barChartPadding();
int count = filteredCategoryCount(tg);
return (barWidth * count) + (padding * (count-1)) + 2;
}

static int barChartX(struct barChartBed *bed)
/* Locate chart on X, relative to viewport. */
{
int start = max(bed->chromStart, winStart);
double scale = scaleForWindow(insideWidth, winStart, winEnd);
int x1 = round((start - winStart) * scale);
return x1;
}

static int barChartMargin()
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

static int valToClippedHeight(double val, double maxVal, int maxView, int maxHeight, 
                                        boolean doLogTransform)
/* Convert a value from 0 to maxVal to 0 to maxHeight-1, with clipping, or log transform the value */
{
double useVal = val;
double useMax = maxVal;
if (!doLogTransform)
    {
    useMax = maxView;
    if (val > maxView)
        useVal = maxView;
    }
return valToHeight(useVal, useMax, barChartMaxHeight(), doLogTransform);
}

static int barChartHeight(struct track *tg, struct barChartItem *itemInfo)
/* Determine height in pixels of graph.  This will be the box for category with highest value */
{
struct barChartBed *bed = (struct barChartBed *)itemInfo->bed;
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
int i;
double maxExp = 0.0;
int expCount = bed->expCount;
double expScore;
for (i=0; i<expCount; i++)
    {
    if (!filterCategory(tg, getCategoryName(tg->table, i)))
        continue;
    expScore = bed->expScores[i];
    maxExp = max(maxExp, expScore);
    }
double viewMax = (double)cartUsualIntClosestToHome(cart, tg->tdb, FALSE, 
                                BAR_CHART_MAX_LIMIT, BAR_CHART_MAX_LIMIT_DEFAULT);
// TODO: add trackDb settings for MAX_LIMIT values ?
double maxMedian = ((struct barChartTrack *)tg->extraUiData)->maxMedian;
return valToClippedHeight(maxExp, maxMedian, viewMax, barChartMaxHeight(), extras->doLogTransform);
}

static void drawGraphBox(struct track *tg, struct barChartItem *itemInfo, struct hvGfx *hvg, int x, int y)
/* Draw white background for graph */
{
Color lighterGray = MAKECOLOR_32(0xF3, 0xF3, 0xF3);
int width = barChartWidth(tg, itemInfo);
int height = barChartHeight(tg, itemInfo);
hvGfxOutlinedBox(hvg, x, y-height, width, height, MG_WHITE, lighterGray);
}

static void drawGraphBase(struct track *tg, struct barChartItem *itemInfo, struct hvGfx *hvg, int x, int y)
/* Draw faint line under graph to delineate extent when bars are missing (category w/ 0 value) */
{
Color lightGray = MAKECOLOR_32(0xD1, 0xD1, 0xD1);
int graphWidth = barChartWidth(tg, itemInfo);
hvGfxBox(hvg, x, y, graphWidth, 1, lightGray);
}

static void barChartDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y, 
                double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw bar chart over item */
{
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
struct barChartItem *itemInfo = (struct barChartItem *)item;
struct barChartBed *bed = (struct barChartBed *)itemInfo->bed;
if (vis == tvDense)
    {
    bedDrawSimpleAt(tg, bed, hvg, xOff, y, scale, font, MG_WHITE, vis);     // color ignored (using grayscale)
    return;
    }
if (vis == tvSquish)
    {
    Color color = barChartItemColor(tg, bed, hvg);
    int height = barChartSquishItemHeight();
    drawScaledBox(hvg, bed->chromStart, bed->chromEnd, scale, xOff, y, height, color);
    return;
    }

int graphX = barChartX(bed);
if (graphX < 0)
    return;

// draw line to show range of item on the genome (since chart is fixed width)
int topGraphHeight = barChartHeight(tg, itemInfo);
topGraphHeight = max(topGraphHeight, tl.fontHeight);
int yZero = topGraphHeight + y - 1;  // yZero is bottom of graph
int yGene = yZero + barChartMargin();
int heightPer = tg->heightPer;
tg->heightPer = barChartModelHeight(extras);
int height = barChartBoxModelHeight();
drawScaledBox(hvg, bed->chromStart, bed->chromEnd, scale, xOff, yGene+1, height, 
                MG_GRAY);
tg->heightPer = heightPer;
}

static int barChartNonPropPixelWidth(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct barChartItem *itemInfo = (struct barChartItem *)item;
int graphWidth = barChartWidth(tg, itemInfo);
return graphWidth;
}

static void barChartNonPropDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y,
                double scale, MgFont *font, Color color, enum trackVisibility vis)
{
if (vis != tvFull && vis != tvPack)
    return;
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
struct barChartItem *itemInfo = (struct barChartItem *)item;
struct barChartBed *bed = (struct barChartBed *)itemInfo->bed;
int topGraphHeight = barChartHeight(tg, itemInfo);
topGraphHeight = max(topGraphHeight, tl.fontHeight);
int yZero = topGraphHeight + y - 1;  // yZero is bottom of graph

int graphX = barChartX(bed);
int x1 = xOff + graphX;         // x1 is at left of graph
int keepX = x1;
drawGraphBase(tg, itemInfo, hvg, keepX, yZero+1);

if (!extras->noWhiteout)
    drawGraphBox(tg, itemInfo, hvg, keepX, yZero+1);

struct rgbColor lineColor = {.r=0};
int lineColorIx = hvGfxFindColorIx(hvg, lineColor.r, lineColor.g, lineColor.b);
int barWidth = barChartBarWidth();
int graphPadding = barChartPadding();
char *colorScheme = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, BAR_CHART_COLORS, 
                        BAR_CHART_COLORS_DEFAULT);
Color clipColor = MG_MAGENTA;

// draw bar graph
double viewMax = (double)cartUsualIntClosestToHome(cart, tg->tdb, FALSE, 
                                BAR_CHART_MAX_LIMIT, BAR_CHART_MAX_LIMIT_DEFAULT);
double maxMedian = ((struct barChartTrack *)tg->extraUiData)->maxMedian;
int i;
int expCount = bed->expCount;
struct barChartCategory *categ;
for (i=0, categ=extras->categories; i<expCount; i++, categ=categ->next)
    {
    if (!filterCategory(tg, categ->name))
        continue;
    struct rgbColor fillColor = extras->colors[i];
    if (barWidth == 1 && sameString(colorScheme, BAR_CHART_COLORS_USER))
        {
        // brighten colors a bit so they'll be more visible at this scale
        // TODO: think about doing this
        //fillColor = barChartBrightenColor(fillColor);
        }
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    double expScore = bed->expScores[i];
    int height = valToClippedHeight(expScore, maxMedian, viewMax, 
                                        barChartMaxHeight(), extras->doLogTransform);
    if (graphPadding == 0 || sameString(colorScheme, BAR_CHART_COLORS_USER))
        hvGfxBox(hvg, x1, yZero-height+1, barWidth, height, fillColorIx);
    else
        hvGfxOutlinedBox(hvg, x1, yZero-height+1, barWidth, height, fillColorIx, lineColorIx);
    // mark clipped bar with magenta tip
    if (!extras->doLogTransform && expScore > viewMax)
        hvGfxBox(hvg, x1, yZero-height+1, barWidth, 2, clipColor);
    x1 = x1 + barWidth + graphPadding;
    }
}

static int barChartItemHeightOptionalMax(struct track *tg, void *item, boolean isMax)
{
// It seems that this can be called early or late
enum trackVisibility vis = tg->visibility;
if (tg->limitedVisSet)
    vis = tg->limitedVis;

int height;
if (vis == tvSquish || vis == tvDense)
    {
    if (vis == tvSquish)
        {
        tg->lineHeight = barChartSquishItemHeight();
        tg->heightPer = tg->lineHeight;
        }
    height = tgFixedItemHeight(tg, item);
    return height;
    }
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
if (isMax)
    {
    int extra = 0;
    height= barChartMaxHeight() + barChartMargin() + barChartModelHeight(extras) + extra;
    return height;
    }
if (item == NULL)
    return 0;
struct barChartItem *itemInfo = (struct barChartItem *)item;

if (itemInfo->height != 0)
    {
    return itemInfo->height;
    }
    int topGraphHeight = barChartHeight(tg, itemInfo);
topGraphHeight = max(topGraphHeight, tl.fontHeight);
int bottomGraphHeight = 0;
height = topGraphHeight + bottomGraphHeight + barChartMargin() + 
                barChartModelHeight(extras);
return height;
}

static int barChartItemHeight(struct track *tg, void *item)
{
int height = barChartItemHeightOptionalMax(tg, item, FALSE);
return height;
}

#ifdef LATER
static char *barChartText(struct barChartCategory *categ, double expScore, 
                                        boolean doLogTransform, char *qualifier)
/* Construct mouseover text for chart */
{
static char buf[128];
doLogTransform = FALSE; // for now, always display expression level on graph as raw RPKM
// TODO: Add units from trackDb
safef(buf, sizeof(buf), "%s (%.1f %s%s%s)", categ->label, 
                                doLogTransform ? log10(expScore+1.0) : expScore,
                                qualifier != NULL ? qualifier : "",
                                qualifier != NULL ? " " : "",
                                doLogTransform ? "log " : "");
return buf;
}
#endif

static int barChartItemStart(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct barChartItem *itemInfo = (struct barChartItem *)item;
struct barChartBed *bed = (struct barChartBed *)itemInfo->bed;
return bed->chromStart;
}

static int barChartItemEnd(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct barChartItem *itemInfo = (struct barChartItem *)item;
struct barChartBed *bed = (struct barChartBed *)itemInfo->bed;
double scale = scaleForWindow(insideWidth, winStart, winEnd);
int graphWidth = barChartWidth(tg, itemInfo);
return max(bed->chromEnd, max(winStart, bed->chromStart) + graphWidth/scale);
}

#ifdef MAP_ITEM

// TODO: implement

static void barChartMapItem(struct track *tg, struct hvGfx *hvg, void *item, char *itemName, 
                        char *mapItemName, int start, int end, int x, int y, int width, int height)
/* Create a map box on gene model and label, and one for each category (bar in the graph) in
 * pack or full mode.  Just single map for squish/dense modes */
{
if (tg->limitedVis == tvDense)
    {
    genericMapItem(tg, hvg, item, itemName, itemName, start, end, x, y, width, height);
    return;
    }
struct barChartItebarChartItem *itemInfo = item;
struct barChartBed *bed = (struct barChartBed *)itemInfo->bed;
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
int geneStart = bed->chromStart;
int geneEnd = bed->chromEnd;
if (tg->limitedVis == tvSquish)
    {
    int tisId = maxTissueForGene(bed);
    char *maxTissue = "";
    if (tisId > 1)
        maxTissue = getTissueDescription(tisId);
    char buf[128];
    safef(buf, sizeof buf, "%s %s", bed->name, maxTissue);
    int x1, x2;
    getItemX(geneStart, geneEnd, &x1, &x2);
    int width = max(1, x2-x1);
    mapBoxHc(hvg, geneStart, geneEnd, x1, y, width, height, 
                 tg->track, mapItemName, buf);
    return;
    }
int topGraphHeight = barChartHeight(tg, itemInfo);
topGraphHeight = max(topGraphHeight, tl.fontHeight);        // label
int yZero = topGraphHeight + y - 1;  // yZero is bottom of graph
int x1 = insideX;


// add maps to category bars
struct barChartCategory *categs = getCategories(tg->table);
struct barChartCategory *categ = NULL;
int barWidth = barChartBarWidth();
int padding = barChartPadding();
double maxMedian = ((struct barChartTrack *)tg->extraUiData)->maxMedian;

int graphX = barChartX(bed);
if (graphX < 0)
    return;

// x1 is at left of graph
x1 = insideX + graphX;

double viewMax = (double)cartUsualIntClosestToHome(cart, tg->tdb, FALSE, 
                                BAR_CHART_MAX_LIMIT, BAR_CHART_MAX_LIMIT_DEFAULT);
for (categ = categs; categ != NULL; categ = categ->next, i++)
    {
    if (!filterTissue(tg, categ->name))
        continue;
    double expScore = bed->expScores[i];
    int height = valToClippedHeight(expScore, maxMedian, viewMax, 
                                        barChartMaxHeight(), extras->doLogTransform);
    char *qualifier = NULL;
    mapBoxHc(hvg, geneStart, geneEnd, x1, yZero-height, barWidth, height, tg->track, mapItemName,  
                barChartText(categ, expScore, extras->doLogTransform, qualifier));
    x1 = x1 + barWidth + padding;
    }

// add map boxes with description to gene model
if (itemInfo->geneModel && itemInfo->description)
    {
    // perhaps these are just start, end ?
    int itemStart = itemInfo->geneModel->txStart;
    int itemEnd = barChartItemEnd(tg, item);
    int x1, x2;
    getItemX(itemStart, itemEnd, &x1, &x2);
    int w = x2-x1;
    int labelWidth = mgFontStringWidth(tl.font, itemName);
    if (x1-labelWidth <= insideX)
        labelWidth = 0;
    // map over label
    int itemHeight = itemInfo->height;
    mapBoxHc(hvg, geneStart, geneEnd, x1-labelWidth, y, labelWidth, itemHeight-3, 
                        tg->track, mapItemName, itemInfo->description);
    // map over gene model (extending to end of item)
    int geneModelHeight = barChartModelHeight(extras);
    mapBoxHc(hvg, geneStart, geneEnd, x1, y+itemHeight-geneModelHeight-3, w, geneModelHeight,
                        tg->track, mapItemName, itemInfo->description);
    } 
}

#endif

/* This is lifted nearly wholesale from gtexGene track.  Could be shared */

static int getBarChartHeight(void *item)
{
struct barChartItem *itemInfo = (struct barChartItem *)item;
assert(itemInfo->height != 0);
return itemInfo->height;
}

/* This is lifted nearly wholesale from gtexGene track.  Could be shared */

static int barChartTotalHeight(struct track *tg, enum trackVisibility vis)
/* Figure out total height of track. Set in track and also return it */
{
int height = 0;
int lineHeight = 0;
int heightPer = 0;

if (vis == tvDense)
    {
    heightPer = tl.fontHeight;
    lineHeight=heightPer+1;
    }
else if (vis == tvSquish)
    {
    // for visibility, set larger than the usual squish, which is half font height
    heightPer = barChartSquishItemHeight() * 2;  // the squish packer halves this
    lineHeight=heightPer+1;
    }
else if ((vis == tvPack) || (vis == tvFull))
    {
    heightPer = tl.fontHeight;
    lineHeight=heightPer;
    }

height = tgFixedTotalHeightOptionalOverflow(tg, vis, lineHeight, heightPer, FALSE);

if ((vis == tvPack) || (vis == tvFull))
    {
    // set variable height rows
    if (tg->ss->rowCount != 0)
        {
        if (!tg->ss->rowSizes)
	    {
	    // collect the rowSizes data across all windows
	    assert(currentWindow==windows); // first window
	    assert(tg->ss->vis == vis); // viz matches, we have the right one
	    struct spaceSaver *ssHold; 
	    AllocVar(ssHold);
	    struct track *tgSave = tg;
	    for(tg=tgSave; tg; tg=tg->nextWindow)
		{
		assert(tgSave->ss->vis == tg->ss->vis); // viz matches, we have the right one
		spaceSaverSetRowHeights(tg->ss, ssHold, getBarChartHeight);
		}
	    // share the rowSizes data across all windows
	    for(tg=tgSave; tg; tg=tg->nextWindow)
		{
		tg->ss->rowSizes = ssHold->rowSizes;
		}
	    tg = tgSave;
	    }
	struct spaceSaver *ss = findSpaceSaver(tg, vis); // ss is a list now
	assert(ss); // viz matches, we have the right one
	height = spaceSaverGetRowHeightsTotal(ss);
        }
    }
tg->height = height;

return height;
}

static void barChartPreDrawItems(struct track *tg, int seqStart, int seqEnd,
                                struct hvGfx *hvg, int xOff, int yOff, int width,
                                MgFont *font, Color color, enum trackVisibility vis)
{
if (vis == tvSquish || vis == tvDense)
    {
    // NonProp routines not relevant to these modes, and they interfere
    // NOTE: they must be installed by barChartMethods() for pack mode
    tg->nonPropDrawItemAt = NULL;
    tg->nonPropPixelWidth = NULL;
    }
}

static char *barChartItemName(struct track *tg, void *item)
/* Return item name */
{
struct barChartItem *chartItem = (struct barChartItem *)item;
struct barChartBed *bed = (struct barChartBed *)chartItem->bed;
return bed->name;
}

void barChartMethods(struct track *tg)
/* Bar Chart track type: draw fixed width chart of colored bars over a BED item */
{
bedMethods(tg);
tg->canPack = TRUE;
tg->drawItemAt = barChartDrawAt;
tg->preDrawItems = barChartPreDrawItems;
tg->loadItems = barChartLoadItems;
// TODO: restore when implemented;
//tg->mapItem = barChartMapItem;
tg->itemName = barChartItemName;
tg->mapItemName = barChartItemName;
tg->itemHeight = barChartItemHeight;
tg->itemStart = barChartItemStart;
tg->itemEnd = barChartItemEnd;
tg->totalHeight = barChartTotalHeight;
tg->nonPropDrawItemAt = barChartNonPropDrawAt;
tg->nonPropPixelWidth = barChartNonPropPixelWidth;
}

