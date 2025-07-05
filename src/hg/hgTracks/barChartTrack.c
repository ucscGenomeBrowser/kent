/* barGraph tracks - display a colored bargraph across each region in a file */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing extrasrmation. */

#include "common.h"
#include "hgTracks.h"
#include "bed.h"
#include "hvGfx.h"
#include "spaceSaver.h"
#include "hubConnect.h"
#include "fieldedTable.h"
#include "facetedTable.h"
#include "barChartBed.h"
#include "barChartCategory.h"
#include "barChartUi.h"

// If a category contributes more than this percentage, its color is displayed in squish mode
// Could be a trackDb setting

struct barChartTrack
/* Track extras */
    {
    boolean noWhiteout;         /* Suppress whiteout of graph background (allow highlight, blue lines) */
    double maxMedian;           /* Maximum median across all categories */
    boolean doLogTransform;     /* Log10(x+1) */
    char *unit;                /* Units for category values (e.g. RPKM) */
    struct barChartCategory *categories; /* Category names, colors, etc. */
    int categCount;             /* Count of categories - derived from above */
    char **categNames;          /* Category names  - derived from above */
    char **categLabels;         /* Category labels  - derived from above */
    struct rgbColor *colors;    /* Colors  for all categories */
    struct hash *categoryFilter; /* NULL out excluded factors */
    struct fieldedTable *statsTable;  /* Filled in if barChartStats setting is there */
    struct facetedTable *facetsTable;  /* Filled in if barChartFacets setting is there */
    struct facetedTableMergedOffset *facetsMergeList; /* Filled in if barChartMerge setting is on */
    int maxViewLimit;

    // dimensions for drawing
    char *maxGraphSize;         /* optionally limit graph size (override semantic zoom)
                                     small, medium, or large (default) */
     int winMaxGraph;             /* Draw large graphs if window size smaller than this */
     int winMedGraph;             /* Draw medium graphs if window size greater than this 
                                        and smaller than winMaxGraph */
     int winSmallGraph;		/* Draw small graphs if windowSize between this and 
                                 * win medGraph, draw tiny if smaller */

    int squishHeight;           /* Height of item in squish mode (larger than typical) */
    int boxModelHeight;         /* Height of indicator box drawn under graph to show gene extent */
    int modelHeight;            /* Height of box drawn under graph with margin */
    int margin;			/* Added to pixel height to help separate things */

    // Individual bar dimensions
    double barWidth;               /* Width of individual bar in pixels */
    int padding;		   /* Pixel width between bars */
    int maxHeight;		   /* Maximum bar height. */
    boolean stretchToItem;	   /* If item is wider than chart, stretch to fit? */
    };

struct barChartItem
/* BED item plus computed values for display */
    {
    struct barChartItem *next;  /* Next in singly linked list */
    struct bed *bed;            /* Item coords, name, exp count and values */
    int height;                 /* Item height in pixels */
    };

/***********************************************/
/* Organize category info */

struct barChartCategory *getCategories(struct track *tg)
/* Get and cache category extras */
{
struct barChartTrack *extras;
extras = (struct barChartTrack *)tg->extraUiData;
if (extras->categories == NULL)
    {
    extras->categories = barChartUiGetCategories(database, tg->tdb, extras->facetsMergeList);
    }
return extras->categories;
}

int getCategoryCount(struct track *tg)
/* Get and cache the number of categories */
{
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
if (extras->categCount == 0)
    extras->categCount = slCount(getCategories(tg));
return extras->categCount;
}

static char *getCategoryName(struct track *tg, int id)
/* Get category name from id, cacheing */
{
struct barChartCategory *categ;
int count = getCategoryCount(tg);
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
if (!extras->categNames)
    {
    struct barChartCategory *categs = getCategories(tg);
    AllocArray(extras->categNames, count);
    for (categ = categs; categ != NULL; categ = categ->next)
        extras->categNames[categ->id] = cloneString(categ->name);
    }
if (id >= count)
    {
    warn("Bar chart track '%s': can't find sample ID %d in category file with %d categories. The category file is a two-column file where sample IDs are associated to categories, specified with the setting 'barChartSampleUrl'.\n", tg->shortLabel, id, count);
    return NULL;        // Exclude this category
    }
return extras->categNames[id];
}

char *getCategoryLabel(struct track *tg, int id)
/* Get category descriptive label from id, cacheing */
{
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
struct barChartCategory *categ;
int count = getCategoryCount(tg);
if (!extras->categLabels)
    {
    struct barChartCategory *categs = getCategories(tg);
    AllocArray(extras->categLabels, count);
    for (categ = categs; categ != NULL; categ = categ->next)
        extras->categLabels[categ->id] = cloneString(categ->label);
    }
if (id >= count)
    errAbort("Bar chart track: can't find id %d\n", id);
return extras->categLabels[id];
}

struct rgbColor *getCategoryColors(struct track *tg)
/* Get RGB colors from category table */
{
struct barChartCategory *categs = getCategories(tg);
struct barChartCategory *categ = NULL;
int count = slCount(categs);
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
if (!extras->colors)
    {
    AllocArray(extras->colors, count);
    int i = 0;
    for (categ = categs; categ != NULL; categ = categ->next)
        {
        // takes advantage of bedColorToRgb assuming alpha will never be 0, and assigning 0xff instead
        extras->colors[i] = bedColorToRgb(categ->color);
        i++;
        }
    }
return extras->colors;
}

static void fillInTables(struct track *tg, struct barChartTrack *extras)
/* Fill in statTable and facetsTable on extras */
{
char *barChartStatsUrl = hReplaceGbdb(trackDbSetting(tg->tdb, "barChartStatsUrl"));
if (barChartStatsUrl != NULL)
    {
    extras->statsTable = fieldedTableFromTabFile(barChartStatsUrl,
	    barChartStatsUrl, NULL, 0);
    char *barChartFacets = trackDbSetting(tg->tdb, "barChartFacets");
    if (barChartFacets != NULL)
	{
	extras->facetsTable = facetedTableFromTable(extras->statsTable, tg->track, barChartFacets);
	if (trackDbSettingOn(tg->tdb, "barChartMerge"))
	    {
	    struct facetedTableMergedOffset *facetsMergeList = facetedTableMakeMergedOffsets(
									extras->facetsTable, cart);
	    assert(facetsMergeList != NULL);
	    extras->facetsMergeList = facetsMergeList;
	    }
	}
    }
}

static void filterCategories(struct track *tg)
/* Check cart for category selection.  NULL out unselected categorys in category list */
{
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
if (extras->facetsMergeList == NULL)
    {
    struct barChartCategory *categ = NULL;
    extras->categories = getCategories(tg);
    extras->categoryFilter = hashNew(0);
    if (extras->facetsTable != NULL && extras->statsTable != NULL)
	{
	struct facetedTable *facTab = extras->facetsTable;
	if (extras->facetsMergeList == NULL)
	    {
	    struct slInt *sel, *selList = facetedTableSelectOffsets(facTab, cart);
	    for (sel = selList; sel != NULL; sel = sel->next)
		{
		char numBuf[16];
		safef(numBuf, sizeof(numBuf), "%d", sel->val);
		char *numCopy = lmCloneString(extras->categoryFilter->lm, numBuf);
		hashAdd(extras->categoryFilter, numCopy, numCopy);
		}
	    }
	return;
	}
    else if (cartListVarExistsAnyLevel(cart, tg->tdb, FALSE, BAR_CHART_CATEGORY_SELECT))
	{
	struct slName *selectedValues = cartOptionalSlNameListClosestToHome(cart, tg->tdb, 
							    FALSE, BAR_CHART_CATEGORY_SELECT);
	if (selectedValues != NULL)
	    {
	    struct slName *name;
	    for (name = selectedValues; name != NULL; name = name->next)
		{
		hashAdd(extras->categoryFilter, name->name, name->name);
		}
	    return;
	    }
	}
    /* no filter */
    for (categ = extras->categories; categ != NULL; categ = categ->next)
	hashAdd(extras->categoryFilter, categ->name, categ->name);
    }
}

static int filteredCategoryCount(struct barChartTrack *extras)
/* Count of categories to display */
{
if (extras->facetsMergeList != NULL)
    return slCount(extras->facetsMergeList);
else {
    if (extras->categoryFilter==NULL)
        errAbort("Internal error: the number of barchart colors doesn't match the barchart labels");
    return hashNumEntries(extras->categoryFilter);
}
}

static boolean filterCategory(struct barChartTrack *extras, char *name)
/* Does category pass filter */
{
if (name == NULL)
    return FALSE;
else if (extras->facetsMergeList)
    return TRUE;
return (hashLookup(extras->categoryFilter, name) != NULL);
}

static int maxCategoryForItem(struct bed *bed)
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
double threshold = 5.4 * totalScore / bed->expCount; /* The 54 cats in  and 10% of that for backwards
						      * compatability with the GTEX 54 element track */
if (totalScore < 1 || maxScore <= threshold)
    return -1;
return maxNum;
}

static Color barChartItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* A bit of category-specific coloring in squish mode only, on bed item */
{
struct bed *bed = item;
int id = maxCategoryForItem(bed);
if (id < 0)
    return MG_BLACK;
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
struct rgbColor color = extras->colors[id];
return hvGfxFindColorIx(hvg, color.r, color.g, color.b);
}

// Convenience functions for draw

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
return valToHeight(useVal, useMax, maxHeight, doLogTransform);
}

static int chartHeight(struct track *tg, struct barChartItem *itemInfo)
/* Determine height in pixels of graph.  This will be the box for category with highest value */
{
struct bed *bed = itemInfo->bed;
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
int i;
double maxExp = 0.0;
int expCount = bed->expCount;
double expScore;
for (i=0; i<expCount; i++)
    {
    if (!filterCategory(extras, getCategoryName(tg, i)))
        continue;
    expScore = bed->expScores[i];
    maxExp = max(maxExp, expScore);
    }
return valToClippedHeight(maxExp, extras->maxMedian, extras->maxViewLimit, extras->maxHeight, 
        extras->doLogTransform);
}

enum trackVisibility trackVisAfterLimit(struct track *tg)
/* Somewhat carefully figure out track visibility after limits may or may not have been set */
{
enum trackVisibility vis = tg->visibility;
if (tg->limitedVisSet)
    vis = tg->limitedVis;
return vis;
}

static int chartItemHeightOptionalMax(struct track *tg, void *item, boolean isMax)
{
struct barChartTrack *extras = tg->extraUiData;
// It seems that this can be called early or late
enum trackVisibility vis = trackVisAfterLimit(tg);

int height;
if (vis == tvSquish || vis == tvDense)
    {
    if (vis == tvSquish)
        {
        tg->lineHeight = extras->squishHeight;
        tg->heightPer = tg->lineHeight;
        }
    height = tgFixedItemHeight(tg, item);
    return height;
    }
if (isMax)
    {
    int extra = 0;
    height= extras->maxHeight + extras->margin + extras->modelHeight + extra;
    return height;
    }
if (item == NULL)
    return 0;
struct barChartItem *itemInfo = (struct barChartItem *)item;

if (itemInfo->height != 0)
    {
    return itemInfo->height;
    }
int topGraphHeight = chartHeight(tg, itemInfo);
topGraphHeight = max(topGraphHeight, tl.fontHeight);
int bottomGraphHeight = 0;
height = topGraphHeight + bottomGraphHeight + extras->margin + extras->modelHeight;
return height;
}

static int barChartItemHeight(struct track *tg, void *item)
{
int height = chartItemHeightOptionalMax(tg, item, FALSE);
return height;
}

static int chartStandardWidth(struct track *tg, struct barChartItem *itemInfo)
/* How wide is chart if we go by standard bar chart size? */
{
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
int count = filteredCategoryCount(extras);
return (extras->barWidth * count) + (extras->padding * (count-1)) + 2;
}

static int windowsTotalIntersection(struct window *list, char *chrom, int chromStart, int chromEnd)
/* Return total size all bits of region defined by chrom/start/end that intersects windows list */
{
if (list == NULL || list->next == NULL)
    {
    double scale = scaleForWindow(insideWidth, winStart, winEnd);
    int leftBlank = round(scale * (max(winStart, chromStart) - winStart));
    int rightBlank = round(scale * (winEnd - min(winEnd, chromEnd)));
    return insideWidth - leftBlank - rightBlank;
    }
long long totalGenoSize = 0;
int totalPixelSize = 0;
struct window *w;
int totalIntersect = 0;
for (w = list; w != NULL; w = w->next)
    {
    totalPixelSize += w->insideWidth;;
    totalGenoSize += w->winEnd - w->winStart;

    if (sameString(w->chromName, chrom))
        {
	int single = rangeIntersection(w->winStart, w->winEnd, chromStart, chromEnd);
	if (single > 0)
	    totalIntersect += single;
	}
    }
if (totalIntersect == 0)
    return 0;
else
    {
    return totalPixelSize * (double)totalIntersect / totalGenoSize;
    }
}

static int chartWidth(struct track *tg, struct barChartItem *itemInfo)
/* How wide is the chart? */
{
struct bed *bed = itemInfo->bed;
int itemSize = windowsTotalIntersection(windows, bed->chrom, bed->chromStart, bed->chromEnd);
int standardSize =  chartStandardWidth(tg, itemInfo);
struct barChartTrack *extras = tg->extraUiData;
if (extras->stretchToItem)
    return max(standardSize, itemSize);
else
    return standardSize;
}

static int chartWidthToBarWidth(struct track *tg, int graphWidth)
/* After possibly expanding the size of a particular item's graph to fit the item,
 * we may need a revised calculation for the size of the bars in that chart */
{
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
int barCount = filteredCategoryCount(extras);
int spaceForBars = graphWidth - 2 - (barCount-1)*extras->padding;
return spaceForBars/barCount;
}


static void  mergeBedScores( struct facetedTableMergedOffset *facetsMergeList, 
    struct bed *bedList)
/* Return a merged down version of bed.  Bed serves as both input and output.  */
/* Maybe a use for a nondestructive-to-inputs version will come up soon. This one is faster
 * for the first use case though. */
{
if (bedList == NULL)
    return;
int outSize = slCount(facetsMergeList);
if (outSize > bedList->expCount)
     {
     errAbort("problem in mergeBedScores(): bed->expCount = %d, outSize = %d", 
	bedList->expCount, outSize);
     }
float outBuf[outSize];
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    facetedTableMergeVals(facetsMergeList, bed->expScores, bed->expCount, outBuf, outSize);
    bed->expCount = outSize;
    memcpy(bed->expScores, outBuf, sizeof(outBuf));
    }
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
if (!tg->extraUiData)
    {
    AllocVar(extras);
    tg->extraUiData = extras;
    }
extras = (struct barChartTrack *)tg->extraUiData;

loadSimpleBedWithLoader(tg, (bedItemLoader)barChartSimpleBedLoad);
fillInTables(tg, extras);
struct bed *bedList = tg->items;
if (extras->facetsMergeList)
    {
    mergeBedScores(extras->facetsMergeList, bedList);
    }

extras->colors = getCategoryColors(tg);

struct trackDb *tdb = tg->tdb;
extras->doLogTransform = barChartIsLogTransformed(cart, tdb->track, tdb);
extras->maxMedian = barChartUiMaxMedianScore(tdb);
extras->noWhiteout = cartUsualBooleanClosestToHome(cart, tdb, FALSE, 
                                                        BAR_CHART_NO_WHITEOUT, BAR_CHART_NO_WHITEOUT_DEFAULT);
extras->maxViewLimit = barChartCurViewMax(cart, tg->track, tg->tdb);
extras->maxGraphSize = trackDbSettingClosestToHomeOrDefault(tdb, 
                                BAR_CHART_MAX_GRAPH_SIZE, BAR_CHART_MAX_GRAPH_SIZE_DEFAULT);
extras->unit = trackDbSettingClosestToHomeOrDefault(tdb, BAR_CHART_UNIT, "");

/* Set barchart dimensions to draw.  For three window sizes */

#define MAX_BAR_CHART_MODEL_HEIGHT     2
#define MED_BAR_CHART_MODEL_HEIGHT     2
#define SMALL_BAR_CHART_MODEL_HEIGHT   1
#define MIN_BAR_CHART_MODEL_HEIGHT     1

#define WIN_MAX_GRAPH_DEFAULT 50000
#define MAX_GRAPH_HEIGHT 175
#define MAX_BAR_WIDTH 5
#define MAX_GRAPH_PADDING 2

#define WIN_MED_GRAPH_DEFAULT 300000
#define MED_GRAPH_HEIGHT 100
#define MED_BAR_WIDTH 3
#define MED_GRAPH_PADDING 1

#define WIN_SMALL_GRAPH_DEFAULT 2000000
#define SMALL_GRAPH_HEIGHT 75
#define SMALL_BAR_WIDTH 1
#define SMALL_GRAPH_PADDING 0

#define MIN_BAR_WIDTH 0.5
#define MIN_GRAPH_PADDING 0

extras->winMaxGraph = WIN_MAX_GRAPH_DEFAULT;
extras->winMedGraph = WIN_MED_GRAPH_DEFAULT;
extras->winSmallGraph = WIN_SMALL_GRAPH_DEFAULT;
char *setting = trackDbSetting(tdb, BAR_CHART_SIZE_WINDOWS);
if (isNotEmpty(setting))
    {
    char *words[2];
    int ct = chopLine(setting, words);
    if (ct == 2)
        {
        extras->winMaxGraph = atoi(words[0]);
        extras->winMedGraph = atoi(words[1]);
        }
    }


/* Create itemInfo items with BED and geneModels */
struct barChartItem *itemInfo = NULL, *list = NULL;


filterCategories(tg);

/* Test that configuration matches data file */
if (bedList != NULL)
    {
    int categCount = getCategoryCount(tg);
    int expCount = bedList->expCount;
    if (categCount != expCount)
        warn("Bar chart track: category count mismatch between trackDb (%d) and data file (%d)",
                            categCount, expCount);
    }

int barCount = filteredCategoryCount(extras);

// users can set minimum bar width and padding in trackDb
char *userSettingMinBarWidth = trackDbSetting(tdb, BAR_CHART_USER_BAR_MIN_WIDTH);
char *userSettingMinBarPadding = trackDbSetting(tdb, BAR_CHART_USER_BAR_MIN_PADDING);

/* Scaling here is pretty ad-hoc! */
double scale = 1.0;
if (barCount <= 20)
    scale = 2.5;
else if (barCount <= 40)
    scale = 1.6;
else if (barCount <= 60)
    scale = 1.0;
else
    scale = 60.0/barCount;
#ifdef OLD
else if (barCount <= 120)
    scale = 0.8;
else if (barCount <= 200)
    scale = 0.6;
else  if (barCount <= 300)
    scale = 0.5;
else
    scale = 0.5 * 300/barCount;
else  if (barCount <= 500)
    scale = 0.4;
else 
    scale = 0.4 * 500/barCount;
else  if (barCount <= 1000)
    scale = 0.3;
else if (barCount <= 2000)
    scale = 0.2;
else if (barCount <= 5000)
    scale = 0.1;
else
    scale = 500/barCount;
#endif /* OLD */

long winSize = virtWinBaseCount;
if (winSize < extras->winMaxGraph && 
        sameString(extras->maxGraphSize, BAR_CHART_MAX_GRAPH_SIZE_LARGE))
    {
    extras->boxModelHeight = MAX_BAR_CHART_MODEL_HEIGHT;
    extras->barWidth = MAX_BAR_WIDTH * scale;
    extras->padding = MAX_GRAPH_PADDING * scale;
    extras->maxHeight = MAX_GRAPH_HEIGHT;
    }
else if (winSize < extras->winMedGraph)
    {
    extras->boxModelHeight = MED_BAR_CHART_MODEL_HEIGHT;
    extras->barWidth = MED_BAR_WIDTH * scale;
    extras->padding = MED_GRAPH_PADDING * scale;
    extras->maxHeight = MED_GRAPH_HEIGHT;
    }
else if (winSize < extras->winSmallGraph)
    {
    extras->boxModelHeight = SMALL_BAR_CHART_MODEL_HEIGHT;
    extras->barWidth = SMALL_BAR_WIDTH * scale;
    extras->padding = SMALL_GRAPH_PADDING * scale;
    extras->maxHeight = SMALL_GRAPH_HEIGHT;
    }
else
    {
    extras->boxModelHeight = MIN_BAR_CHART_MODEL_HEIGHT;
    extras->barWidth = MIN_BAR_WIDTH * scale;
    extras->padding = MIN_GRAPH_PADDING * scale;
    extras->maxHeight = tl.fontHeight * 4;
    }

if (extras->barWidth > 1)
    extras->barWidth = floor(extras->barWidth);
    
if (extras->barWidth <= 1 && extras->padding == 1)
   {
   extras->barWidth = 2;
   extras->padding = 0;
   }
if (extras->barWidth < 1)
    {
    extras->barWidth = 1;
    extras->padding = 0;
    }
else
    extras->barWidth = round(extras->barWidth);
if (userSettingMinBarWidth)
    {
    int userMinBarWidth = sqlUnsigned(userSettingMinBarWidth);
    extras->barWidth = max(userMinBarWidth, extras->barWidth);
    }
if (userSettingMinBarPadding)
    {
    int userMinBarPadding = sqlUnsigned(userSettingMinBarPadding);
    extras->padding = max(userMinBarPadding, extras->padding);
    }

extras->modelHeight =  extras->boxModelHeight + 3;
extras->margin = 1;
extras->squishHeight = tl.fontHeight - tl.fontHeight/2;
extras->stretchToItem = trackDbSettingOn(tg->tdb, "barChartStretchToItem");

struct bed *bed, *next;
for (bed = bedList; bed != NULL; bed = next)
    {
    next = bed->next;
    bed->next = NULL;	/* Pop it off list */
    AllocVar(itemInfo);
    itemInfo->bed = (struct bed *)bed;
    slAddHead(&list, itemInfo);
    itemInfo->height = barChartItemHeight(tg, itemInfo);
    }
slReverse(&list);
tg->items = list;
}

/***********************************************/
/* Draw */

static int barChartX(struct bed *bed)
/* Locate chart on X, relative to viewport. */
{
// use the max() here because otherwise items that begin to the left
// of the window may not be visible
int start = max(bed->chromStart, winStart);
double scale = scaleForWindow(insideWidth, winStart, winEnd);
int x1 = round((start - winStart) * scale);
return x1;
}


static void drawGraphBox(struct track *tg, struct barChartItem *itemInfo, struct hvGfx *hvg, int x, int y)
/* Draw white background for graph */
{
Color lighterGray = MAKECOLOR_32(0xE8, 0xE8, 0xE8);
int width = chartWidth(tg, itemInfo);
int height = chartHeight(tg, itemInfo);
hvGfxOutlinedBox(hvg, x, y-height, width, height, MG_WHITE, lighterGray);
}

static void drawGraphBase(struct track *tg, struct barChartItem *itemInfo, struct hvGfx *hvg, int x, int y)
/* Draw faint line under graph to delineate extent when bars are missing (category w/ 0 value) */
{
Color lightGray = MAKECOLOR_32(0xD1, 0xD1, 0xD1);
int graphWidth = chartWidth(tg, itemInfo);
hvGfxBox(hvg, x, y, graphWidth, 1, lightGray);
}

static void barChartDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y, 
                double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw bar chart over item */
{
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
struct barChartItem *itemInfo = (struct barChartItem *)item;
struct bed *bed = itemInfo->bed;
if (vis == tvDense)
    {
    bedDrawSimpleAt(tg, bed, hvg, xOff, y, scale, font, MG_WHITE, vis);     // color ignored (using grayscale)
    return;
    }
if (vis == tvSquish)
    {
    Color color = barChartItemColor(tg, bed, hvg);
    int height = extras->squishHeight;
    drawScaledBox(hvg, bed->chromStart, bed->chromEnd, scale, xOff, y, height, color);
    return;
    }

// draw line to show range of item on the genome (since chart is fixed width)
int topGraphHeight = chartHeight(tg, itemInfo);
topGraphHeight = max(topGraphHeight, tl.fontHeight);
int yZero = topGraphHeight + y - 1;  // yZero is bottom of graph
int yGene = yZero + extras->margin;
int heightPer = tg->heightPer;
tg->heightPer = extras->modelHeight;
int height = extras->boxModelHeight;
drawScaledBox(hvg, bed->chromStart, bed->chromEnd, scale, xOff, yGene+1, height, 
                MG_GRAY);
tg->heightPer = heightPer;
}

static int barChartNonPropPixelWidth(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct barChartItem *itemInfo = (struct barChartItem *)item;
int graphWidth = chartWidth(tg, itemInfo);
return graphWidth;
}

static void barChartNonPropDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y,
                double scale, MgFont *font, Color color, enum trackVisibility vis)
{
if (vis != tvFull && vis != tvPack)
    return;
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
struct barChartItem *itemInfo = (struct barChartItem *)item;
struct bed *bed = itemInfo->bed;
int topGraphHeight = chartHeight(tg, itemInfo);
int graphWidth = chartWidth(tg, itemInfo);
topGraphHeight = max(topGraphHeight, tl.fontHeight);
int yZero = topGraphHeight + y - 1;  // yZero is bottom of graph

int graphX = barChartX(bed);
int x0 = xOff + graphX;         // x0 is at left of graph
int x1 = x0;
drawGraphBase(tg, itemInfo, hvg, x0, yZero+1);

if (!extras->noWhiteout)
    drawGraphBox(tg, itemInfo, hvg, x0, yZero+1);

struct rgbColor lineColor = {.r=0};
int lineColorIx = hvGfxFindColorIx(hvg, lineColor.r, lineColor.g, lineColor.b);
int barWidth = chartWidthToBarWidth(tg, graphWidth);
char *colorScheme = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, BAR_CHART_COLORS, 
                        BAR_CHART_COLORS_DEFAULT);
Color clipColor = MG_MAGENTA;

// draw bar graph
int i;
int expCount = bed->expCount;
struct barChartCategory *categ;

for (i=0, categ=extras->categories; i<expCount && categ != NULL; i++, categ=categ->next)
    {
    if (!filterCategory(extras, categ->name))
        continue;

    // note - removed userMinBarWidth here, as it's already been imposed in the initial barWidth calculation.
    // Stretch mode only ever extends bars, not shrinks them, so the minimum doesn't need to be re-imposed here.
    // stop before going off the right edge
    if (x1 + barWidth > x0 + graphWidth)
        break;

    struct rgbColor fillColor = extras->colors[i];
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    double expScore = bed->expScores[i];
    int height = valToClippedHeight(expScore, extras->maxMedian, extras->maxViewLimit, 
                                        extras->maxHeight, extras->doLogTransform);
    boolean isClipped = (!extras->doLogTransform && expScore > extras->maxViewLimit);
    int barTop = yZero - height + 1;

    if (extras->padding == 0 || sameString(colorScheme, BAR_CHART_COLORS_USER))
        hvGfxBox(hvg, x1, barTop, barWidth, height, fillColorIx);
    else
        hvGfxOutlinedBox(hvg, x1, barTop, barWidth, height, fillColorIx, lineColorIx);

    // mark clipped bar with magenta tip
    if (isClipped)
        hvGfxBox(hvg, x1, barTop, barWidth, 2, clipColor);
    x1 += barWidth + extras->padding;
    }
}

static char *chartMapText(struct track *tg, struct barChartCategory *categ, double expScore)
/* Construct mouseover text for a chart bar */
{
static char buf[256];
struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
safef(buf, sizeof(buf), "%s (%.1f %s)", categ->label, expScore, extras->unit);
subChar(buf, '_', ' ');
return buf;
}

static int barChartItemStart(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct barChartItem *itemInfo = (struct barChartItem *)item;
struct bed *bed = itemInfo->bed;
return bed->chromStart;
}

static int barChartItemEnd(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct barChartItem *itemInfo = (struct barChartItem *)item;
struct bed *bed = itemInfo->bed;
double scale = scaleForWindow(insideWidth, winStart, winEnd);
int graphWidth = chartWidth(tg, itemInfo);
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
struct bed *bed = itemInfo->bed;
int itemStart = bed->chromStart;
int itemEnd = bed->chromEnd;
int x1, x2;
if (tg->limitedVis == tvSquish)
    {
    int categId = maxCategoryForItem(bed);
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
int topGraphHeight = chartHeight(tg, itemInfo);
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


int graphX = barChartX(bed);
int graphWidth = chartWidth(tg, itemInfo);
int barCount = filteredCategoryCount(extras);

char label[256];
if (barCount <= graphWidth) // Don't create map boxes if less than one pixel per bar
    {
    // add maps to category bars
    struct barChartCategory *categs = getCategories(tg);
    struct barChartCategory *categ = NULL;
    int x0 = insideX + graphX;
    int x1 = x0;
    int i = 0, barWidth = max(1,chartWidthToBarWidth(tg, graphWidth));
    int extraAtTop = 4;
    for (categ = categs; categ != NULL; categ = categ->next, i++)
        {
        if (!filterCategory(extras, categ->name))
            continue;
        double expScore = bed->expScores[i];
        int height = valToClippedHeight(expScore, extras->maxMedian, extras->maxViewLimit,
                                            extras->maxHeight, extras->doLogTransform);
        height = min(height+extraAtTop, extras->maxHeight);
        mapBoxHc(hvg, itemStart, itemEnd, x1, yZero-height, barWidth, height,
                            tg->track, mapItemName, chartMapText(tg, categ, expScore));
        x1 += barWidth + extras->padding;
        }
    safef(label, sizeof(label), 
	"%s - click for faceted view or hover over a bar for sample values", 
	itemName);
    }
else
    // We should never get here; chartWidth always returns a value where at least one pixel
    // of width is allocated to every category being drawn
    safef(label, sizeof(label), 
	    "%s - zoom in to resolve individual bars or click for details", 
	    itemName);
// map over background of chart
getItemX(start, end, &x1, &x2);
mapBoxHc(hvg, itemStart, itemEnd, x1, y, graphWidth, itemHeight-3,
                    tg->track, mapItemName, label);
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
    struct barChartTrack *extras = (struct barChartTrack *)tg->extraUiData;
    if (extras)  // has it been initialized yet?
	{
	heightPer = extras->squishHeight * 2;  // the squish packer halves this
	lineHeight=heightPer+1;
	}
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

    if (tg->ss)  // got past trackLoadingInProgress
	{
	struct spaceSaver *ss = findSpaceSaver(tg, vis); // ss is a list now
//	assert(ss); // viz matches, we have the right one

	if (ss && ss->rowCount != 0)
	    {
	    if (!ss->rowSizes)
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
	    height = spaceSaverGetRowHeightsTotal(ss);
	    }
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
struct bed *bed = chartItem->bed;
return bed->name;
}

static char *barChartItemName(struct track *tg, void *item)
/* Return item name for labeling */
{
struct barChartItem *chartItem = (struct barChartItem *)item;
struct bed *bed = chartItem->bed;
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
tg->loadItems = barChartLoadItems;
tg->itemName = barChartItemName;
tg->mapItemName = barChartMapItemName;
tg->itemHeight = barChartItemHeight;
tg->itemStart = barChartItemStart;
tg->itemEnd = barChartItemEnd;
tg->totalHeight = barChartTotalHeight;
tg->preDrawItems = barChartPreDrawItems;
tg->drawItemAt = barChartDrawAt;
tg->mapItem = barChartMapItem;
tg->nonPropDrawItemAt = barChartNonPropDrawAt;
tg->nonPropPixelWidth = barChartNonPropPixelWidth;
}

void barChartCtMethods(struct track *tg)
/* Bar Chart track methods for custom track */
{
barChartMethods(tg);
}
