/* hic -- draw Hi-C maps */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "obscure.h"
#include "hgTracks.h"
#include "bedCart.h"
#include "bigWarn.h"
#include "interact.h" // for interact structures
#include "hicUi.h"
#include "Cstraw.h"
#include "hic.h"
#include "htmlColor.h"


static int hicTotalHeight(struct track *tg, enum trackVisibility vis)
/* Calculate height of the track across all windows.  For arc and triangle, this
 * depends on the actual interactions being drawn.  For square, it just depends
 * on the height of the highest square (yes, this currently means smaller
 * squares are stretched vertically in multi-region - it's not ideal). */
{
int maxHeight = 0;

struct track *trackScan = tg;
while (trackScan != NULL) // Canvas max height for this and previous windows ...
    {
    if ((int)trackScan->maxRange > maxHeight)
        maxHeight = (int)trackScan->maxRange;
    trackScan = trackScan->prevWindow;
    }
trackScan = tg->nextWindow;
while (trackScan != NULL) // ... and look at following windows ...
    {
    if ((int)trackScan->maxRange > maxHeight)
        maxHeight = (int)trackScan->maxRange;
    trackScan = trackScan->nextWindow;
    }

// Later, try making pack 0.5, squish 0.25, and dense 0.125.  The various
// draw functions will also have to be modified accordingly.
if ( tg->visibility == tvDense)
    maxHeight *= 0.5;
return maxHeight;
}

struct hicMeta *grabHeader(struct track *tg)
/* Fetch a hicMeta structure that describes the Hi-C data associated with
 * the track. */
{
char *filename = trackDbSettingOrDefault(tg->tdb, "bigDataUrl", NULL);
struct hicMeta *metaResult = NULL;
if (filename == NULL)
    {
    warn("Missing bigDataUrl setting for track %s", tg->track);
    return NULL;
    }
char *errMsg = hicLoadHeader(filename, &metaResult);
if (errMsg != NULL)
    {
    tg->networkErrMsg = errMsg;
    return NULL;
    }
return metaResult;
}


int fetchResolution(struct track *tg)
/* Retrieve the binsize to draw the track in.  If the track UI has the resolution
 * set as "auto", find the widest resolution that still splits the window into 5000
 * pieces (if possible, otherwise select the smallest resolution). */
{
if (tg->customPt == NULL)
    tg->customPt = grabHeader(tg);
if (tg->customPt == 0)
    return -1;

struct hicMeta *meta = (struct hicMeta*) tg->customPt;
char *binSizeString = hicUiFetchResolution(cart, tg->track, meta);
if (binSizeString == NULL)
    {
    warn("Empty binSize string");
    return 0;
    }
int result;
if (sameString(binSizeString, "Auto"))
    {
    int idealRes = (winEnd-winStart)/5000;
    char *autoRes = meta->resolutions[meta->nRes-1];
    int i;
    for (i=meta->nRes-1; i>= 0; i--)
        {
        if (atoi(meta->resolutions[i]) < idealRes)
            {
            autoRes = meta->resolutions[i];
            break;
            }
        }
    result = atoi(autoRes);
    }
else
    result = atoi(binSizeString);
return result;
}

char *fetchNormalization(struct track *tg)
/* Fetch the normalization to use for this track. */
{
if (tg->customPt == NULL)
    tg->customPt = grabHeader(tg);
if (tg->customPt == NULL)
    return NULL;
struct hicMeta *meta = (struct hicMeta*) tg->customPt;
return hicUiFetchNormalization(cart, tg->track, meta);
}


static void loadAndFilterItems(struct track *tg)
/* Load all Hi-C items in the current region and identify the window height
 * and median value for this region. */
{
int *x, *y, numRecords;
double *counts;
int binSize = fetchResolution(tg);
if (binSize == 0)
    return;
struct hicMeta *meta = (struct hicMeta*)tg->customPt;
char *normalization = fetchNormalization(tg);

// Later, we should validate that this file is for the current assembly (see the hicMeta structure)

// Note: This is giving it a 0-based, full-closed window. Straw seems to use 1-based coordinates
// by default, but accepts 0 as the start of a window without complaint.
struct dyString *windowPos = dyStringNew(0);
// Pad the start because we want to display partial interactions if the end of an interacting block is
// in view but not the start.  Straw won't report interactions if the start of the block isn't in the
// supplied position range.
int strawStart = winStart - binSize + 1;
if (strawStart < 0) strawStart = 0;

char *hicChromName = chromName;
if (meta->ucscToAlias != NULL)
    {
    hicChromName = (char*) hashFindVal(meta->ucscToAlias, chromName);
    if (hicChromName == NULL)
        {
        hicChromName = chromName;
        }
    }
dyStringPrintf(windowPos, "%s:%d:%d", hicChromName, strawStart, winEnd-1);

char *filename = trackDbSettingOrDefault(tg->tdb, "bigDataUrl", NULL);
if (filename == NULL)
    return;

tg->networkErrMsg = Cstraw(normalization, filename, binSize, dyStringContents(windowPos), dyStringContents(windowPos), "BP", &x, &y, &counts, &numRecords);

// Using the interact structure because it has convenient fields, but this is not interact data and
// shouldn't be passed to those functions.
struct interact *hicItems = NULL;
int i = 0, filtNumRecords = 0;
tg->maxRange = 0.0; // the max height of an interaction in this window
double *countsCopy = NULL;
AllocArray(countsCopy, numRecords);
for (i=0; i<numRecords; i++)
    {
    char *drawMode = hicUiFetchDrawMode(cart, tg->track);
    if (isnan(counts[i]))
        {
        // Yes, apparently NAN is possible with normalized values in some methods.  Ignore those.
        continue;
        }
    if (sameString(drawMode, HIC_DRAW_MODE_ARC))
        {
        // we omit self-interactions in arc mode (they'd just be weird vertical lines)
        if (x[i] == y[i])
            continue;
        }
    countsCopy[filtNumRecords++] = counts[i];
    struct interact *new = NULL;
    AllocVar(new);
    new->chrom = cloneString(chromName);
    // x is always <= y, so x is always the start
    new->chromStart = x[i];
    new->chromEnd = y[i]+binSize;
    new->name = NULL;
    new->score = 0; // ignored
    new->value = counts[i];
    new->exp = NULL;
    new->color = 0;
    new->sourceChrom = cloneString(chromName);
    new->sourceStart = x[i];
    new->sourceEnd = x[i]+binSize;
    new->targetChrom = cloneString(chromName);
    new->targetStart = y[i];
    new->targetEnd = y[i]+binSize;
    new->targetName = new->targetStrand = NULL;

    // Calculate the track draw height required to see this item
    int leftx = new->chromStart > winStart ? new->chromStart : winStart;
    int rightx = new->chromEnd < winEnd ? new->chromEnd : winEnd;
    double thisHeight = scaleForWindow(insideWidth, winStart, winEnd)*(rightx - leftx)/2.0; // triangle or arc
    if (sameString(drawMode,HIC_DRAW_MODE_SQUARE))
        thisHeight = scaleForWindow(insideWidth, winStart, winEnd)*(winEnd-winStart); // square - always draw the full square

    if (thisHeight > tg->maxRange)
        tg->maxRange = thisHeight;

    slAddHead(&hicItems, new);
    }

// Heuristic for auto-scaling the color gradient based on the scores in view - draw the max color value
// at or above 2*median score.
tg->graphUpperLimit = 2.0*doubleMedian(filtNumRecords, countsCopy);
free(countsCopy);
tg->items = hicItems;
}

void hicLoadItems(struct track *tg)
/* Load Hi-C items in (mostly) interact format */
{
char *filename = trackDbSettingOrDefault(tg->tdb, "bigDataUrl", NULL);
if (filename == NULL)
    return;
tg->customPt = grabHeader(tg);
if (tg->customPt == NULL)
    return;
loadAndFilterItems(tg);
}


Color *colorSetForHic(struct hvGfx *hvg, struct track *tg, int bucketCount)
/* Create the gradient color array for drawing a Hi-C heatmap */
{
struct rgbColor rgbLow;
char *lowColorText = hicUiFetchBgColor(cart, tg->track); // This is an HTML color like #ffed02
unsigned lowRgbVal = 0;
if (!htmlColorForCode(lowColorText, &lowRgbVal))
{
    warn("Bad RGB background color value %s for track %s", lowColorText, tg->track);
    return NULL;
}
int r, g, b;
htmlColorToRGB(lowRgbVal, &r, &g, &b);

rgbLow.r=(unsigned char)r;
rgbLow.g=(unsigned char)g;
rgbLow.b=(unsigned char)b;

struct rgbColor rgbHigh;
char *highColorText = hicUiFetchDrawColor(cart, tg->track); // This is an HTML color like #ffed02
unsigned highRgbVal = 0;
if (!htmlColorForCode(highColorText, &highRgbVal))
{
    warn("Bad RGB color value %s for track %s", highColorText, tg->track);
    return NULL;
}
htmlColorToRGB(highRgbVal, &r, &g, &b);
rgbHigh.r=(unsigned char)r;
rgbHigh.g=(unsigned char)g;
rgbHigh.b=(unsigned char)b;
Color *colorIxs = NULL;
AllocArray(colorIxs, bucketCount);
hvGfxMakeColorGradient(hvg, &rgbLow, &rgbHigh, bucketCount, colorIxs);
return colorIxs;
}


double getHicMaxScore(struct track *tg)
/* Return the score at which we should reach the maximum intensity color. */
{
if (hicUiFetchAutoScale(cart, tg->track))
    return tg->graphUpperLimit;
else
    return hicUiFetchMaxValue(cart, tg->track);
}

void calcItemLeftRightBoundaries(int *leftStart, int *leftEnd, int *rightStart, int *rightEnd,
        int binSize, struct interact *hicItem)
{
*leftStart = hicItem->sourceStart - winStart;
*leftEnd = *leftStart + binSize;
*rightStart = hicItem->targetStart - winStart;
*rightEnd = *rightStart + binSize;
if (*leftStart < 0)
    *leftStart = 0;
if (*leftEnd > winEnd-winStart)
    *leftEnd = winEnd-winStart;
if (*rightStart < 0)
    *rightStart = 0;
if (*rightEnd > winEnd-winStart)
    *rightEnd = winEnd-winStart;
}


static void drawHicTriangle(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw a list of Hi-C interactions in a triangle */
{
double xScale = scaleForWindow(width, seqStart, seqEnd);
double yScale = xScale;
int maxHeight = tg->height;
struct interact *hicItem = NULL;
int binSize = fetchResolution(tg);
if (binSize == 0)
    return;

if (vis == tvDense)
    {
    yScale *= 0.5;
    }

double maxScore = getHicMaxScore(tg);
Color *colorIxs = colorSetForHic(hvg, tg, HIC_SCORE_BINS+1);
if (colorIxs == NULL)
    return; // something went wrong with colors

for (hicItem = (struct interact *)tg->items; hicItem; hicItem = hicItem->next)
    {
    int leftStart, leftEnd, rightStart, rightEnd;
    calcItemLeftRightBoundaries(&leftStart, &leftEnd, &rightStart, &rightEnd, binSize, hicItem);

    int colorIx;
    if (hicItem->value > maxScore)
        colorIx = colorIxs[HIC_SCORE_BINS];
    else
        colorIx = colorIxs[(int)(HIC_SCORE_BINS * hicItem->value/maxScore)];
    // adding four polygon points for a diamond, based on the starts and ends of the source and target coordinate ranges.
    struct gfxPoly *diamond = gfxPolyNew();

    // left point of the diamond
    double x = xScale * (leftStart+rightStart)/2.0;
    double y = yScale * (rightStart-leftStart)/2.0;
    gfxPolyAddPoint(diamond, (int)x+xOff, maxHeight-(int)y+yOff);

    // top point of the diamond
    x = xScale * (leftStart+rightEnd)/2.0;
    y = yScale * (rightEnd-leftStart)/2.0;
    gfxPolyAddPoint(diamond, (int)x+xOff, maxHeight-(int)y+yOff);

    // right point of the diamond
    x = xScale * ((leftEnd + rightEnd)/2.0);
    y = yScale * (rightEnd-leftEnd)/2.0;
    gfxPolyAddPoint(diamond, (int)x+xOff, maxHeight-(int)y+yOff);

    // bottom point of the diamond
    x = xScale * (leftEnd+rightStart)/2.0;
    y = yScale * (rightStart-leftEnd)/2.0;
    gfxPolyAddPoint(diamond, (int)x+xOff, maxHeight-(int)y+yOff);
    hvGfxDrawPoly(hvg, diamond, colorIx, TRUE);
    gfxPolyFree(&diamond);
    }
}



static void drawHicSquare(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw a Hi-C heatmap as a square */
{
double xScale = scaleForWindow(width, seqStart, seqEnd);
double yScale = xScale;
int maxHeight = tg->height;
struct interact *hicItem = NULL;
int binSize = fetchResolution(tg);
if (binSize == 0)
    return;

if (vis == tvDense)
    {
    yScale *= 0.5;
    }

double maxScore = getHicMaxScore(tg);
Color *colorIxs = colorSetForHic(hvg, tg, HIC_SCORE_BINS+1);
if (colorIxs == NULL)
    return; // something went wrong with colors

for (hicItem = (struct interact *)tg->items; hicItem; hicItem = hicItem->next)
    {
    int leftStart, leftEnd, rightStart, rightEnd;
    calcItemLeftRightBoundaries(&leftStart, &leftEnd, &rightStart, &rightEnd, binSize, hicItem);

    int colorIx;
    if (hicItem->value > maxScore)
        colorIx = colorIxs[HIC_SCORE_BINS];
    else
        colorIx = colorIxs[(int)(HIC_SCORE_BINS * hicItem->value/maxScore)];
    double x = xScale * leftStart;
    double y = yScale * ((winEnd-winStart)-rightStart);
    hvGfxBox(hvg, (int)x+xOff, maxHeight-(int)y+yOff, (int)(xScale*(leftEnd-leftStart))+1, (int)(yScale*(rightEnd-rightStart))+1, colorIx);
    if (leftStart != rightStart)
        {
        x = xScale * rightStart;
        y = yScale * ((winEnd-winStart)-leftStart);
        hvGfxBox(hvg, (int)x+xOff, maxHeight-(int)y+yOff, (int)(xScale*(rightEnd-rightStart))+1, (int)(yScale*(leftEnd-leftStart))+1, colorIx);
        }
    }
    // Draw top-left to bottom-right diagonal axis line in black
    int colorIx = hvGfxFindColorIx(hvg, 0, 0, 0);
    hvGfxLine(hvg, xOff, yOff, ((winEnd-winStart)*xScale)+xOff, maxHeight+yOff, colorIx);
}


int cmpHicItems(const void *elem1, const void *elem2)
/* Comparison function for sorting Hi-C interactions by score */
{
struct interact *item1 = *((struct interact**)elem1);
struct interact *item2 = *((struct interact**)elem2);
if (item1->value > item2->value)
    return 1;
if (item1->value < item2->value)
    return -1;
return 0;
}

static void drawHicArc(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw Hi-C interactions in arc mode */
{
double xScale = scaleForWindow(width, seqStart, seqEnd);
double yScale = xScale;
int maxHeight = tg->height;
struct interact *hicItem = NULL;
int binSize = fetchResolution(tg);
if (binSize == 0)
    return;

if (vis == tvDense)
    {
    yScale *= 0.5;
    }

double maxScore = getHicMaxScore(tg);
Color *colorIxs = colorSetForHic(hvg, tg, HIC_SCORE_BINS+1);
if (colorIxs == NULL)
    return; // something went wrong with colors

slSort(&tg->items, cmpHicItems); // So that the darkest arcs are drawn on top and not lost
for (hicItem = (struct interact *)tg->items; hicItem; hicItem = hicItem->next)
    {
    int leftStart = hicItem->sourceStart - winStart;
    int leftMidpoint = leftStart + binSize/2;
    int rightStart = hicItem->targetStart - winStart;
    int rightMidpoint = rightStart + binSize/2;
    if ((leftMidpoint < 0) || (leftMidpoint > winEnd-winStart))
        continue;  // skip this item - we'd be drawing to a point off the screen
    if ((rightMidpoint < 0) || (rightMidpoint > winEnd-winStart))
        continue;  // skip this item - we'd be drawing to a point off the screen

    int colorIx;
    if (hicItem->value > maxScore)
        colorIx = colorIxs[HIC_SCORE_BINS];
    else
        colorIx = colorIxs[(int)(HIC_SCORE_BINS * hicItem->value/maxScore)];

    double leftx = xScale * leftMidpoint;
    double rightx = xScale * rightMidpoint;
    double midx = xScale * (rightMidpoint+leftMidpoint)/2.0;
    double midy = yScale * (rightMidpoint-leftMidpoint)/2.0;
    midy *= 1.5; // Heuristic scaling for better use of vertical space
    hvGfxCurve(hvg, (int)leftx+xOff, maxHeight+yOff, (int)midx+xOff, maxHeight-(int)midy+yOff,
            (int)rightx+xOff, maxHeight+yOff, colorIx, FALSE);
    }
}

void hicDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw a set of Hi-C interactions with the current user settings. */
{
char *drawMode = hicUiFetchDrawMode(cart, tg->track);
if (sameString(drawMode,HIC_DRAW_MODE_SQUARE))
    drawHicSquare(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
else if (sameString(drawMode,HIC_DRAW_MODE_TRIANGLE))
    drawHicTriangle(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
else if (sameString(drawMode,HIC_DRAW_MODE_ARC))
    drawHicArc(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
else
    warn ("Unknown draw mode %s for track %s", drawMode, tg->track);
}


void doLeftLabelsExceptNot(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
                               int xOff, int yOff, int width, int height, boolean withCenterLabels,
                               MgFont *font, Color color, enum trackVisibility vis)
/* A no-op function. There are no left labels associated with this track type. */
{
return;
}

void hicMethods(struct track *tg)
/* Hi-C track type methods */
{
tg->bedSize = 12;
tg->loadItems = hicLoadItems;
tg->drawItems = hicDrawItems;
tg->totalHeight = hicTotalHeight;
tg->drawLeftLabels = doLeftLabelsExceptNot; // If we don't pretend to do it ourselves, hgTracks tries and fails badly
tg->mapsSelf = 1;
}

void hicCtMethods(struct track *tg)
/* Hi-C track methods for custom track */
{
hicMethods(tg);
}

