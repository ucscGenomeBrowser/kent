/* GTEX tracks  */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hgTracks.h"
#include "hvGfx.h"
#include "rainbow.h"
#include "gtexGeneBed.h"
#include "gtexTissue.h"
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

static int valToY(double val, double maxVal, int height)
/* Convert a value from 0 to maxVal to 0 to height-1 */
{
double scaled = val/maxVal;
int y = scaled * (height);
return (height-1) - y;
//int y = scaled * (height-1);
//return (height - 1) - y;
}

struct gtexGeneExtras 
    {
    double maxExp;
    };


/* Cache tissue metadata */

struct gtexTissue *getGtexTissues()
/* Get tissue metadata from database */
{
static struct gtexTissue *gtexTissues = NULL;
if (gtexTissues == NULL)
    {
    char query[1024];
    struct sqlConnection *conn = sqlConnect("hgFixed");
    sqlSafef(query, sizeof(query), "select * from gtexTissue order by id");
    gtexTissues = gtexTissueLoadByQuery(conn, query);
    }
return gtexTissues;
}

struct rgbColor *getGtexTissueColors()
/* Get RGB colors from tissue table */
{
struct gtexTissue *tissues = getGtexTissues();
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

static int gtexGraphWidth(struct gtexGeneBed *gtex)
/* Width of GTEx graph in pixels */
{
int barWidth = gtexBarWidth();
int padding = gtexGraphPadding();
int count = gtex->expCount;
return (barWidth * count) + (padding * (count-1));
}

static int gtexGraphX(struct gtexGeneBed *gtex)
/* Locate graph on X, relative to viewport. Return -1 if it won't fit */
{
int start = max(gtex->chromStart, winStart);
int end = min(gtex->chromEnd, winEnd);
if (start > end)
    return -1;
double scale = scaleForWindow(insideWidth, winStart, winEnd);
int x1 = round((start - winStart) * scale);
int x2 = round((end - winStart) * scale);
int width = gtexGraphWidth(gtex);
if (x1 + width > x2)
    return -1;
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

static void gtexGeneDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y, 
                    double scale, MgFont *font, Color color, enum trackVisibility vis)
{
struct gtexGeneBed *geneBed = item;
initGeneColors(hvg);
//warn("item: %s, xOff=%d\n", geneBed->name, xOff);
// Color using transcriptClass
Color statusColor;
if (geneBed->transcriptClass == NULL)
    statusColor = statusColors.unknown;
else if (sameString(geneBed->transcriptClass, "coding"))
    statusColor = statusColors.coding;
else if (sameString(geneBed->transcriptClass, "nonCoding"))
    statusColor = statusColors.nonCoding;
else if (sameString(geneBed->transcriptClass, "problem"))
    statusColor = statusColors.problem;
else 
    statusColor = statusColors.unknown;

if (vis != tvFull && vis != tvPack)
    {
    bedDrawSimpleAt(tg, item, hvg, xOff, y, scale, font, statusColor, vis);
    return;
    }
int i;
int expCount = geneBed->expCount;
double maxExp = ((struct gtexGeneExtras *)tg->extraUiData)->maxExp;
struct rgbColor lineColor = {.r=0};
int lineColorIx = hvGfxFindColorIx(hvg, lineColor.r, lineColor.g, lineColor.b);
int heightPer = tg->heightPer;

// x1 is at left of graph
int x1 = gtexGraphX(geneBed) + xOff;

// yZero is at bottom of graph
int yZero = gtexGraphHeight() + y - 1;

//uglyf("DRAW: xOff=%d, x1=%d, y=%d, yZero=%d<br>", xOff, x1, y, yZero);

int barWidth = gtexBarWidth();
int graphPadding = gtexGraphPadding();
char *colorScheme = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, GTEX_COLORS, 
                        GTEX_COLORS_DEFAULT);
struct rgbColor *colors;
if (sameString(colorScheme, GTEX_COLORS_GTEX))
    {
    // retrieve from table
    // TODO: cache this
    colors = getGtexTissueColors();
    }
else
    {
    // currently the only other choice
    // TODO: cache this
    colors = getRainbow(&saturatedRainbowAtPos, expCount);
    //colors = getRainbow(&lightRainbowAtPos, expCount);
    }
for (i=0; i<expCount; i++)
    {
    struct rgbColor fillColor = colors[i];
    if (barWidth == 1 && sameString(colorScheme, GTEX_COLORS_GTEX))
        {
        // brighten colors a bit so they'll be more visible at this scale
        struct hslColor hsl = mgRgbToHsl(fillColor);
        hsl.s = min(1000, hsl.s + 300);
        fillColor = mgHslToRgb(hsl);
        }
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    double expScore = geneBed->expScores[i];
    int yMedian = valToY(expScore, maxExp, gtexGraphHeight()) + y;
    //int height = yZero - yMedian + 1;
    int height = yZero - yMedian;
    // TODO: adjust yGene instead of yMedian+1 to get gene track distance as desired
    //if (i ==0) uglyf("DRAW: expScore=%.2f, maxExp=%.2f, graphHeight=%d, y=%d<br>", expScore, maxExp, gtexGraphHeight(), y);
    //if (i ==0) uglyf("DRAW: yZero=%d, yMedian=%d, height=%d<br>", yZero, yMedian, height);
    if (graphPadding == 0 || sameString(colorScheme, GTEX_COLORS_GTEX))
        hvGfxBox(hvg, x1, yMedian+1, barWidth, height, fillColorIx);
    else
        hvGfxOutlinedBox(hvg, x1, yMedian+1, barWidth, height, fillColorIx, lineColorIx);
    x1 = x1 + barWidth + graphPadding;
    }
// mark gene extent
int yGene = yZero + gtexGeneMargin() - 1;

#ifdef NO_GENE
// underline
int start = max(geneBed->chromStart, winStart);
int end = min(geneBed->chromEnd, winEnd);
if (start > end)
    return;
int xStart = round((start - winStart) * scale)+xOff;
int width = round((end-start)*scale);
hvGfxBox(hvg, xStart, yGene, width, gtexGeneHeight(), lineColorIx);
//uglyf("DRAW: xStart=%d, yGene=%d, width=%d, height=%d<br> ", xStart, yGene, width, gtexGeneHeight());
#endif

// load & draw gene model

char query[1024];
char **row;
sqlSafef(query, sizeof query, "select * from gtexGeneModel where name='%s'", geneBed->geneId);
struct sqlConnection *conn = hAllocConn(database);
if (conn == NULL)
    return;
//uglyf("query: %s<br>", query);
struct sqlResult *sr = sqlGetResult(conn, query);
struct genePred *geneModel = NULL;
if (sr != NULL)
    {
    if ((row = sqlNextRow(sr)) != NULL)
        geneModel = genePredLoad(row);
    sqlFreeResult(&sr);
    }
if (geneModel == NULL)
    return;
struct linkedFeatures *lf = linkedFeaturesFromGenePred(tg, geneModel, FALSE);
tg->heightPer = gtexGeneHeight()+1;
lf->filterColor = statusColor;
linkedFeaturesDrawAt(tg, lf, hvg, xOff, yGene, scale, font, color, tvSquish);
tg->heightPer = heightPer;
hFreeConn(&conn);
}

static void gtexGeneMapItem(struct track *tg, struct hvGfx *hvg, void *item, char *itemName, 
                        char *mapItemName, int start, int end, int x, int y, int width, int height)
/* Create a map box for each tissue (bar in the graph) */
{
//uglyf("map item: itemName=%s, mapItemName=%s, start=%d, end=%d, x=%d, y=%d, width=%d, height=%d, insideX=%d",
        //itemName, mapItemName, start, end, x, y, width, height, insideX);
struct gtexTissue *tissues = getGtexTissues();
struct gtexTissue *tissue = NULL;
struct gtexGeneBed *gtex = item;
int barWidth = gtexBarWidth();
int padding = gtexGraphPadding();
double maxExp = ((struct gtexGeneExtras *)tg->extraUiData)->maxExp;

//int start1 = max(start, winStart);
// TODO: support reverse display (hvg->rc)
//double scale = scaleForWindow(width, start, end);
//int x1 = round((start-winStart)*scale) + x;

// skip over label.  TODO: check w/ different modes, and look for a better way
// map box on label
int labelWidth = mgFontStringWidth(tl.font, gtex->name) + 3;
//mapBoxHc(hvg, start, end, x, y, labelWidth, height, tg->track, mapItemName, mapItemName);
int x1 = x + labelWidth;

int i = 0;
int yZero = gtexGraphHeight() + y - 1;
for (tissue = tissues; tissue != NULL; tissue = tissue->next, i++)
    {
    double expScore = gtex->expScores[i];
    int yMedian = valToY(expScore, maxExp, gtexGraphHeight()) + y;
    int height = yZero - yMedian;
    //hvGfxBox(hvg, x1, yMedian, barWidth, yBase - yMedian, fillColorIx);
    //int yBase = valToY(0, maxExp, heightPer) + y;
    //int height = yBase - yMedian;
    // TODO: call genericMapItem
    //genericMapItem(tg, hvg, item, itemName, tissue->description, start, end, x1, y, barWidth, height);
    //uglyf("id=%d, mapItemName= %s, start=%d, end=%d, x1=%d, y=%d (expScore=%.2f, yMedian=%d, yBase=%d), height=%d.  ",
                //i, mapItemName, start, end, x1, y1, expScore, yMedian, yBase, height);
    mapBoxHc(hvg, start, end, x1, yMedian+1, barWidth, height, tg->track, mapItemName, tissue->description);
    #ifdef DEBUG
    if (i ==0)
        {
        uglyf("MAP: expScore=%.2f, maxExp=%.2f, graphHeight=%d, y=%d<br>", expScore, maxExp, gtexGraphHeight(), y);
        uglyf("MAP: yZero=%d, yMedian=%d, height=%d<br>", yZero, yMedian, height); 
        break;
        }
    #endif
    x1 = x1 + barWidth + padding;
    }
}

static struct gtexGeneBed *loadGtexGeneBed(char **row)
{
return gtexGeneBedLoad(row);
}

static void gtexGeneLoadItems(struct track *track)
{
bedLoadItem(track, track->table, (ItemLoader)loadGtexGeneBed);
double maxExp = 0.0;
int i;
struct gtexGeneBed *geneBed;
for (geneBed = track->items; geneBed != NULL; geneBed = geneBed->next)
    for (i=0; i<geneBed->expCount; i++)
        maxExp = (geneBed->expScores[i] > maxExp ? geneBed->expScores[i] : maxExp);
struct gtexGeneExtras *extras;
AllocVar(extras);
extras->maxExp = maxExp;
track->extraUiData = extras;
}

static int gtexGeneItemHeight(struct track *track, void *item)
{
if ((item == NULL) || (track->visibility == tvSquish) || (track->visibility == tvDense))
    return 0;
return gtexGraphHeight() + + gtexGeneMargin() + gtexGeneHeight();
}

static int gtexTotalHeight(struct track *track, enum trackVisibility vis)
/* Figure out total height of track */
{
int height;
if (track->visibility == tvSquish || track->visibility == tvDense)
    height = 10;
else 
    height = gtexGraphHeight() + gtexGeneMargin() + gtexGeneHeight();
return tgFixedTotalHeightOptionalOverflow(track, vis, height, height, FALSE);
}

void gtexGeneMethods(struct track *track)
{
track->drawItemAt = gtexGeneDrawAt;
track->loadItems = gtexGeneLoadItems;
track->mapItem = gtexGeneMapItem;
track->itemHeight = gtexGeneItemHeight;
track->totalHeight = gtexTotalHeight;
}



