/* barGraph tracks - display a colored bargraph across each region in a file */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hgTracks.h"
#include "bed.h"
#include "hvGfx.h"
#include "spaceSaver.h"
#include "hubConnect.h"
#include "barChartBed.h"
#include "barChartCategory.h"
#include "barChartUi.h"

// If a category contributes more than this percentage, its color is displayed in squish mode
// Could be a trackDb setting
#define SPECIFICITY_THRESHOLD   10

struct barChartTrack
/* Track info */
    {
    boolean noWhiteout;         /* Suppress whiteout of graph background (allow highlight, blue lines) */
    double maxMedian;           /* Maximum median across all categories */
    int maxHeight;              /* Maximum height in pixels for track */
    boolean doLogTransform;     /* Log10(x+1) */
    boolean doAutoScale;        /* Scale to maximum in window, alternative to log */
    char *unit;                /* Units for category values (e.g. RPKM) */
    struct barChartCategory *categories; /* Category names, colors, etc. */
    int categCount;             /* Count of categories - derived from above */
    char **categNames;          /* Category names  - derived from above */
    char **categLabels;         /* Category labels  - derived from above */
    struct rgbColor *colors;    /* Colors  for all categories */
    struct hash *categoryFilter;  /* NULL out excluded factors */
    };

struct barChartItem
/* BED item plus computed values for display */
    {
    struct barChartItem *next;  /* Next in singly linked list */
    struct bed *bed;            /* Item coords, name, exp count and values */
    double maxScore;            /* Maximum expScore in bed */
    int height;                 /* Item height in pixels */
    };

/***********************************************/
/* Organize category info */

struct barChartCategory *getCategories(struct track *tg)
/* Get and cache category info */
{
struct barChartTrack *info = (struct barChartTrack *)tg->extraUiData;
if (info->categories == NULL)
    info->categories = barChartUiGetCategories(database, tg->tdb);
return info->categories;
}

int getCategoryCount(struct track *tg)
/* Get and cache the number of categories */
{
struct barChartTrack *info = (struct barChartTrack *)tg->extraUiData;
if (info->categCount == 0)
    info->categCount = slCount(getCategories(tg));
return info->categCount;
}

char *getCategoryName(struct track *tg, int id)
/* Get category name from id, cacheing */
{
struct barChartCategory *categ;
int count = getCategoryCount(tg);
struct barChartTrack *info = (struct barChartTrack *)tg->extraUiData;
if (!info->categNames)
    {
    struct barChartCategory *categs = getCategories(tg);
    AllocArray(info->categNames, count);
    for (categ = categs; categ != NULL; categ = categ->next)
        info->categNames[categ->id] = cloneString(categ->name);
    }
if (id >= count)
    {
    warn("Bar chart track: can't find id %d\n", id);
    return NULL;        // Exclude this category
    }
return info->categNames[id];
}

char *getCategoryLabel(struct track *tg, int id)
/* Get category descriptive label from id, cacheing */
{
struct barChartTrack *info = (struct barChartTrack *)tg->extraUiData;
struct barChartCategory *categ;
int count = getCategoryCount(tg);
if (!info->categLabels)
    {
    struct barChartCategory *categs = getCategories(tg);
    AllocArray(info->categLabels, count);
    for (categ = categs; categ != NULL; categ = categ->next)
        info->categLabels[categ->id] = cloneString(categ->label);
    }
if (id >= count)
    errAbort("Bar chart track: can't find id %d\n", id);
return info->categLabels[id];
}

struct rgbColor *getCategoryColors(struct track *tg)
/* Get RGB colors from category table */
{
struct barChartCategory *categs = getCategories(tg);
struct barChartCategory *categ = NULL;
int count = slCount(categs);
struct barChartTrack *info = (struct barChartTrack *)tg->extraUiData;
if (!info->colors)
    {
    AllocArray(info->colors, count);
    int i = 0;
    for (categ = categs; categ != NULL; categ = categ->next)
        {
        info->colors[i] = (struct rgbColor){.r=COLOR_32_BLUE(categ->color), .g=COLOR_32_GREEN(categ->color), .b=COLOR_32_RED(categ->color)};
        i++;
        }
    }
return info->colors;
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

static int barChartMaxHeight(int maxHeight)
/* Set maximum graph height based on window size */
{
// scale based on subjective aesthetic (previous hardcoded were 175/100)
#define WIN_MED_GRAPH_SCALE     .57

long winSize = virtWinBaseCount;
if (winSize < WIN_MAX_GRAPH)
    return maxHeight;
else if (winSize < WIN_MED_GRAPH)
    return maxHeight * WIN_MED_GRAPH_SCALE;
else
    return tl.fontHeight * 4;
}

static int barChartItemHeight(struct track *tg, void *item);

static void filterCategories(struct track *tg)
/* Check cart for category selection.  NULL out unselected categorys in category list */
{
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
struct barChartCategory *categ = NULL;
extras->categories = getCategories(tg);
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
if (name == NULL)
    return FALSE;
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
return (hashLookup(extras->categoryFilter, name) != NULL);
}

static int maxCategoryForItem(struct bed *bed, int threshold)
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

double barChartMaxExpScore(struct track *tg, struct barChartItem *itemInfo)
/* Determine maximum expScore in a barChart, set it and return it */
{
// use preset if available
double maxScore = itemInfo->maxScore;
if (maxScore > 0.0)
    return maxScore;

// determine from bed
maxScore = 0.0;
struct bed *bed = (struct bed *)itemInfo->bed;
int i;
int expCount = bed->expCount;
double expScore;
for (i=0; i<expCount; i++)
    {
    if (!filterCategory(tg, getCategoryName(tg, i)))
        continue;
    expScore = bed->expScores[i];
    maxScore = max(maxScore, expScore);
    }
itemInfo->maxScore = maxScore;
return maxScore;
}

static Color barChartItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* A bit of category-specific coloring in squish mode only, on bed item */
{
struct bed *bed = (struct bed *)item;
int id = maxCategoryForItem(bed, SPECIFICITY_THRESHOLD);
if (id < 0)
    return MG_BLACK;
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
struct rgbColor color = extras->colors[id];
return hvGfxFindColorIx(hvg, color.r, color.g, color.b);
}

static void barChartLoadItems(struct track *tg)
/* Load method for track items */
{
/* Initialize colors for visibilities that don't display actual barchart */
if (tg->visibility == tvSquish || tg->limitedVis == tvSquish)
    tg->itemColor = barChartItemColor;
tg->colorShades = shadesOfGray;

/* Get track UI info */
struct barChartTrack *extras;
AllocVar(extras);
tg->extraUiData = extras;

struct trackDb *tdb = tg->tdb;
extras->doLogTransform = cartUsualBooleanClosestToHome(cart, tdb, FALSE, BAR_CHART_LOG_TRANSFORM, 
                                                BAR_CHART_LOG_TRANSFORM_DEFAULT);
extras->doAutoScale = cartUsualBooleanClosestToHome(cart, tdb, FALSE, BAR_CHART_AUTOSCALE,
                                                BAR_CHART_AUTOSCALE_DEFAULT);
extras->maxMedian = barChartUiMaxMedianScore(tdb);
extras->noWhiteout = cartUsualBooleanClosestToHome(cart, tdb, FALSE, BAR_CHART_NO_WHITEOUT,
                                                        BAR_CHART_NO_WHITEOUT_DEFAULT);
extras->unit = trackDbSettingClosestToHomeOrDefault(tdb, BAR_CHART_UNIT, "");

int min, max, deflt, current;
barChartUiFetchMinMaxPixels(cart, tdb, &min, &max, &deflt, &current);
extras->maxHeight = barChartMaxHeight(current);

/* Get bed (names and all-sample category median scores) in range */
loadSimpleBedWithLoader(tg, (bedItemLoader)barChartSimpleBedLoad);

/* Create itemInfo items with BED and geneModels */
struct barChartItem *itemInfo = NULL, *infoList = NULL;
struct bed *bed = (struct bed *)tg->items;

/* Load category colors */
extras->colors = getCategoryColors(tg);

filterCategories(tg);

/* create list of barChart items */
double maxScoreInWindow = 0;
while (bed != NULL)
    {
    AllocVar(itemInfo);
    itemInfo->bed = bed;
    slAddHead(&infoList, itemInfo);
    bed = bed->next;
    itemInfo->bed->next = NULL;
    itemInfo->maxScore = barChartMaxExpScore(tg, itemInfo);
    maxScoreInWindow = max(maxScoreInWindow, itemInfo->maxScore);
    }
if (extras->doAutoScale)
    extras->maxMedian = maxScoreInWindow;
else
    extras->maxMedian = max(maxScoreInWindow, extras->maxMedian);

/* determine graph heights */
for (itemInfo = infoList; itemInfo != NULL; itemInfo = itemInfo->next)
    itemInfo->height = barChartItemHeight(tg, itemInfo);

/* replace item list with wrapped beds */
slReverse(&infoList);
tg->items = infoList;
}

/***********************************************/
/* Draw */

/* Bargraph layouts for three window sizes */
#define WIN_MAX_GRAPH 50000
#define MAX_BAR_WIDTH 5
#define MAX_GRAPH_PADDING 2

#define WIN_MED_GRAPH 500000
#define MED_BAR_WIDTH 3
#define MED_GRAPH_PADDING 1

#define MIN_BAR_WIDTH 1
#define MIN_GRAPH_PADDING 0

#define MARGIN_WIDTH 1


static int barChartBarWidth(struct track *tg)
{
long winSize = virtWinBaseCount;
int scale = (getCategoryCount(tg) < 15 ? 2 : 1);
if (winSize < WIN_MAX_GRAPH)
    return MAX_BAR_WIDTH * scale;
else if (winSize < WIN_MED_GRAPH)
    return MED_BAR_WIDTH * scale;
else
    return MIN_BAR_WIDTH * scale;
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

static boolean barChartUseViewLimit(struct barChartTrack *extras)
{
return !extras->doLogTransform && !extras->doAutoScale;
}

static int barChartWidth(struct track *tg, struct barChartItem *itemInfo)
/* Width of bar chart in pixels */
{
int barWidth = barChartBarWidth(tg);
int padding = barChartPadding();
int count = filteredCategoryCount(tg);
return (barWidth * count) + (padding * (count-1)) + 2;
}

static int barChartX(struct bed *bed)
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

static int valToClippedHeight(double val, double maxVal, int maxView, struct barChartTrack *extras)
/* Convert a value from 0 to maxVal to 0 to maxHeight-1, with clipping, or log transform the value */
{
double useVal = val;
double useMax = maxVal;
if (barChartUseViewLimit(extras))
    {
    useMax = maxView;
    if (val > maxView)
        useVal = maxView;
    }
return valToHeight(useVal, useMax, extras->maxHeight, extras->doLogTransform);
}

static int barChartHeight(struct track *tg, struct barChartItem *itemInfo)
/* Determine height in pixels of graph.  This will be the box for category with highest value */
{
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
double maxExp = barChartMaxExpScore(tg, itemInfo);
double viewMax = (double)cartUsualIntClosestToHome(cart, tg->tdb, FALSE, 
                                BAR_CHART_MAX_VIEW_LIMIT, BAR_CHART_MAX_VIEW_LIMIT_DEFAULT);
double maxMedian = extras->maxMedian;
return valToClippedHeight(maxExp, maxMedian, viewMax, extras);
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
struct bed *bed = (struct bed *)itemInfo->bed;
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
struct bed *bed = (struct bed *)itemInfo->bed;
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
int barWidth = barChartBarWidth(tg);
int graphPadding = barChartPadding();
char *colorScheme = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, BAR_CHART_COLORS, 
                        BAR_CHART_COLORS_DEFAULT);
Color clipColor = MG_MAGENTA;

// draw bar graph
double viewMax = (double)cartUsualIntClosestToHome(cart, tg->tdb, FALSE, 
                                BAR_CHART_MAX_VIEW_LIMIT, BAR_CHART_MAX_VIEW_LIMIT_DEFAULT);
double maxMedian = extras->maxMedian;
int i;
int expCount = bed->expCount;
struct barChartCategory *categ;
for (i=0, categ=extras->categories; i<expCount && categ != NULL; i++, categ=categ->next)
    {
    if (!filterCategory(tg, categ->name))
        continue;
    struct rgbColor fillColor = extras->colors[i];
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    double expScore = bed->expScores[i];
    int height = valToClippedHeight(expScore, maxMedian, viewMax, extras);
    if (graphPadding == 0 || sameString(colorScheme, BAR_CHART_COLORS_USER))
        hvGfxBox(hvg, x1, yZero-height+1, barWidth, height, fillColorIx);
    else
        hvGfxOutlinedBox(hvg, x1, yZero-height+1, barWidth, height, fillColorIx, lineColorIx);
    // mark clipped bar with magenta tip
    if (barChartUseViewLimit(extras) && expScore > viewMax)
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
    height = extras->maxHeight + barChartMargin() + barChartModelHeight(extras) + extra;
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

static char *barChartMapText(struct track *tg, struct barChartCategory *categ, double expScore)
/* Construct mouseover text for a chart bar */
{
static char buf[128];
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
safef(buf, sizeof(buf), "%s (%.1f %s)", categ->label, expScore, extras->unit);
return buf;
}

static int barChartItemStart(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct barChartItem *itemInfo = (struct barChartItem *)item;
struct bed *bed = (struct bed *)itemInfo->bed;
return bed->chromStart;
}

static int barChartItemEnd(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct barChartItem *itemInfo = (struct barChartItem *)item;
struct bed *bed = (struct bed *)itemInfo->bed;
double scale = scaleForWindow(insideWidth, winStart, winEnd);
int graphWidth = barChartWidth(tg, itemInfo);
return max(bed->chromEnd, max(winStart, bed->chromStart) + graphWidth/scale);
}

static void getItemX(int start, int end, int *x1, int *x2)
/* Return startX, endX based on item coordinates and current window */
// Residual (largely replaced by drawScaledBox -- still used by gene model bmap box
{
int s = max(start, winStart);
int e = min(end, winEnd);
double scale = scaleForWindow(insideWidth, winStart, winEnd);
assert(x1);
*x1 = round((double)((int)s-winStart)*scale + insideX);
assert(x2);
*x2 = round((double)((int)e-winStart)*scale + insideX);
}

static void barChartMapItem(struct track *tg, struct hvGfx *hvg, void *item, char *itemName, 
                        char *mapItemName, int start, int end, int x, int y, int width, int height)
/* Create a map box on item and label, and one for each category (bar in the graph) in
 * pack or full mode.  Just single map for squish/dense modes */
{
if (tg->limitedVis == tvDense)
    {
    genericMapItem(tg, hvg, item, itemName, itemName, start, end, x, y, width, height);
    return;
    }
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
struct barChartItem *itemInfo = (struct barChartItem *)item;
struct bed *bed = (struct bed *)itemInfo->bed;
int itemStart = bed->chromStart;
int itemEnd = bed->chromEnd;
int x1, x2;
if (tg->limitedVis == tvSquish)
    {
    int categId = maxCategoryForItem(bed, SPECIFICITY_THRESHOLD);
    char *maxCateg = "";
    if (categId > 1)
        maxCateg = getCategoryLabel(tg, categId);
    char buf[128];
    safef(buf, sizeof buf, "%s %s", bed->name, maxCateg);
    getItemX(itemStart, itemEnd, &x1, &x2);
    int width = max(1, x2-x1);
    mapBoxHc(hvg, itemStart, itemEnd, x1, y, width, height, 
                 tg->track, mapItemName, buf);
    return;
    }
int topGraphHeight = barChartHeight(tg, itemInfo);
topGraphHeight = max(topGraphHeight, tl.fontHeight);        // label
int yZero = topGraphHeight + y - 1;  // yZero is bottom of graph

// add map box to item label

int labelWidth = mgFontStringWidth(tl.font, itemName);
getItemX(start, end, &x1, &x2);
if (x1-labelWidth <= insideX)
    labelWidth = 0;
// map over label
int itemHeight = itemInfo->height;
mapBoxHc(hvg, itemStart, itemEnd, x1-labelWidth, y, labelWidth, itemHeight-3, 
                    tg->track, mapItemName, itemName);

// add maps to category bars
struct barChartCategory *categs = getCategories(tg);
struct barChartCategory *categ = NULL;
int barWidth = barChartBarWidth(tg);
int padding = barChartPadding();
double maxMedian = extras->maxMedian;

int graphX = barChartX(bed);
if (graphX < 0)
    return;

// x1 is at left of graph
x1 = insideX + graphX;

double viewMax = (double)cartUsualIntClosestToHome(cart, tg->tdb, FALSE, 
                                BAR_CHART_MAX_VIEW_LIMIT, BAR_CHART_MAX_VIEW_LIMIT_DEFAULT);
int i = 0;
for (categ = categs; categ != NULL; categ = categ->next, i++)
    {
    if (!filterCategory(tg, categ->name))
        continue;
    double expScore = bed->expScores[i];
    int height = valToClippedHeight(expScore, maxMedian, viewMax, extras);
    mapBoxHc(hvg, itemStart, itemEnd, x1, yZero-height, barWidth, height, tg->track, mapItemName,  
                barChartMapText(tg, categ, expScore));
    x1 = x1 + barWidth + padding;
    }

// map over background of chart
int graphWidth = barChartWidth(tg, itemInfo);
getItemX(start, end, &x1, &x2);
mapBoxHc(hvg, itemStart, itemEnd, x1, y, graphWidth, itemHeight-3,
                    tg->track, mapItemName, itemName);
}

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

static char *barChartMapItemName(struct track *tg, void *item)
/* Return item name for click handler */
{
struct barChartItem *chartItem = (struct barChartItem *)item;
struct bed *bed = (struct bed *)chartItem->bed;
return bed->name;
}

static char *barChartItemName(struct track *tg, void *item)
/* Return item name for labeling */
{
struct barChartItem *chartItem = (struct barChartItem *)item;
struct bed *bed = (struct bed *)chartItem->bed;
if (tg->isBigBed)
    return bigBedItemName(tg, bed);
return bed->name;
}

void barChartMethods(struct track *tg)
/* Bar Chart track type: draw fixed width chart of colored bars over a BED item */
{
tg->bedSize = 8;
bedMethods(tg);
tg->canPack = TRUE;
tg->drawItemAt = barChartDrawAt;
tg->preDrawItems = barChartPreDrawItems;
tg->loadItems = barChartLoadItems;
tg->mapItem = barChartMapItem;
tg->itemName = barChartItemName;
tg->mapItemName = barChartMapItemName;
tg->itemHeight = barChartItemHeight;
tg->itemStart = barChartItemStart;
tg->itemEnd = barChartItemEnd;
tg->totalHeight = barChartTotalHeight;
tg->nonPropDrawItemAt = barChartNonPropDrawAt;
tg->nonPropPixelWidth = barChartNonPropPixelWidth;
}

void barChartCtMethods(struct track *tg)
/* Bar Chart track methods for custom track */
{
barChartMethods(tg);
}
