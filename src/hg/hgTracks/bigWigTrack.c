/* bigWigTrack - stuff to handle loading and display of bigWig type tracks in browser.   */

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "localmem.h"
#include "wigCommon.h"
#include "bbiFile.h"
#include "bigWig.h"
#include "errCatch.h"
#include "container.h"
#include "bigWarn.h"
#include "mathWig.h"
#include "float.h"
#include "hubConnect.h"
#include "chromAlias.h"
#include "hgMaf.h"
#include "quickLift.h"
#include "chainNetDbLoad.h"
#include "chain.h"
#include "bigChain.h"

static void summaryToPreDraw( struct bbiFile *bbiFile, char *chrom, int start, int end, int summarySize, struct bbiSummaryElement *summary, struct preDrawElement *preDraw, int preDrawZero, boolean flip)
/* Calculate summary and fill in predraw with results. */
{
if (bigWigSummaryArrayExtended(bbiFile, chrom, start, end, summarySize, summary))
    {
    /* Convert format to predraw */
    int i;
    for (i=0; i<summarySize; ++i)
        {
        struct preDrawElement *pe = &preDraw[i + preDrawZero];
        struct bbiSummaryElement *be = &summary[i];
        if (flip)
            be = &summary[(summarySize - 1) - i];
        pe->count = be->validCount;
        pe->min = be->minVal;
        pe->max = be->maxVal;
        pe->sumData = be->sumData;
        pe->sumSquares = be->sumSquares;
        }
    }
}

struct preDrawContainer *bigWigLoadPreDraw(struct track *tg, int seqStart, int seqEnd, int width)
/* Do bits that load the predraw buffer tg->preDrawContainer. */
{
/* Just need to do this once... */
if (tg->preDrawContainer != NULL)
    return tg->preDrawContainer;

char *quickLiftFile = cloneString(trackDbSetting(tg->tdb, "quickLiftUrl"));
struct preDrawContainer *preDrawList = NULL;
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    /* Allocate predraw area. */

    /* Get summary info from bigWig */
    int summarySize = width;
    struct bbiSummaryElement *summary;
    AllocArray(summary, summarySize);

    struct bbiFile *bbiFile, *bbiNext;
    for(bbiFile = tg->bbiFile; bbiFile ; bbiFile = bbiNext)
	{
	struct preDrawContainer *pre = initPreDrawContainer(width);
	slAddHead(&preDrawList, pre);

        if (quickLiftFile != NULL)
            {
            char *linkFileName = bigChainGetLinkFile(quickLiftFile);

            // get the chain that maps to our window coordinates
            struct chain  *chain, *chainList = chainLoadIdRangeHub(NULL, quickLiftFile, linkFileName, chromName, winStart, winEnd, -1);

            // go through each block of each chain and grab a summary from the query coordinates
            for(chain = chainList; chain; chain = chain->next)
                {
                struct chain *retChain, *retChainToFree;
                char *chrom = chain->qName;
                chainSubsetOnT(chain, winStart, winEnd, &retChain, &retChainToFree);
                struct cBlock *cb;
                for(cb = retChain->blockList; cb; cb = cb->next)
                    {
                    // figure out where in the summary array the target coordinates put us
                    int tSize = retChain->tEnd - retChain->tStart;
                    int summaryOffset = (((double)cb->tStart - retChain->tStart) / tSize ) * summarySize;
                    int summarySizeBlock = (((double)cb->tEnd - cb->tStart) / tSize ) * summarySize;

                    // grab the data using query coordinates
                    if (summarySizeBlock != 0)
                        {
                        int start = cb->qStart;
                        int end = cb->qEnd;
                        boolean flip = FALSE;
                        if (chain->qStrand == '-')
                            {
                            start = chain->qSize - cb->qEnd;
                            end = chain->qSize - cb->qStart;
                            flip = TRUE;
                            }

                        summaryToPreDraw(bbiFile, chrom, start, end, summarySizeBlock, &summary[summaryOffset], &pre->preDraw[summaryOffset], pre->preDrawZero, flip);
                        }
                    }
                }

            }
        else
            {
            // if we're not quicklifting we can grab the whole summary from the window coordinates
            summaryToPreDraw(bbiFile, chromName, winStart, winEnd, summarySize, summary, pre->preDraw, pre->preDrawZero, FALSE);
            }

        // I don't think tg->bbiFile is ever a list of more than one.  Verify this
	bbiNext = bbiFile->next;
        if (bbiNext != NULL)
            errAbort("multiple bbiFiles found");

	bigWigFileClose(&bbiFile);
	}
    tg->bbiFile = NULL;
    freeMem(summary);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    tg->networkErrMsg = cloneString(errCatch->message->string);
    }
errCatchFree(&errCatch);
tg->preDrawContainer = preDrawList;
return preDrawList;
}

static void bigWigPreDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
wigLogoMafCheck(tg, seqStart, seqEnd);

if (tg->networkErrMsg == NULL)
    {
    /* Call pre graphing routine. */
    wigPreDrawPredraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis,
		   tg->preDrawContainer, tg->preDrawContainer->preDrawZero, tg->preDrawContainer->preDrawSize, 
		   &tg->graphUpperLimit, &tg->graphLowerLimit);
    }
}


static void bigWigDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
if (tg->networkErrMsg == NULL)
    {
    /* Call actual graphing routine. */
    wigDrawPredraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis,
		   tg->preDrawContainer, tg->preDrawContainer->preDrawZero, tg->preDrawContainer->preDrawSize, 
		   tg->graphUpperLimit, tg->graphLowerLimit);
    }
else
    bigDrawWarning(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
maybeDrawQuickLiftLines(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
}

static void bigWigOpenCatch(struct track *tg, char *fileName)
/* Try to open big wig file, store error in track struct */
{
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    struct bbiFile *bbiFile = bigWigFileOpenAlias(fileName, chromAliasFindAliases);
    slAddHead(&tg->bbiFile, bbiFile);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    tg->networkErrMsg = cloneString(errCatch->message->string);
    tg->drawItems = bigDrawWarning;
    if (!sameOk(parentContainerType(tg), "multiWig"))
	tg->totalHeight = bigWarnTotalHeight;
    }
errCatchFree(&errCatch);
}

static void dataToPixelsUp(double *data, struct preDrawContainer *pre)
/* Up sample data into pixels. */
{
int preDrawZero = pre->preDrawZero;
unsigned size = winEnd - winStart;
double dataPerPixel = size / (double) insideWidth;
int pixel;

for (pixel=0; pixel<insideWidth; ++pixel)
    {
    struct preDrawElement *pe = &pre->preDraw[pixel + preDrawZero];
    unsigned index = pixel * dataPerPixel;
    pe->count = 1;
    pe->min = data[index];
    pe->max = data[index];
    pe->sumData = data[index] ;
    pe->sumSquares = data[index] * data[index];
    }
}

static void dataToPixelsDown(double *data, struct preDrawContainer *pre)
/* Down sample data into pixels. */
{
int preDrawZero = pre->preDrawZero;
unsigned size = winEnd - winStart;
double dataPerPixel = size / (double) insideWidth;
int pixel;

for (pixel=0; pixel<insideWidth; ++pixel)
    {
    struct preDrawElement *pe = &pre->preDraw[pixel + preDrawZero];
    double startReal = pixel * dataPerPixel;
    double endReal = (pixel + 1) * dataPerPixel;
    unsigned startUns = startReal;
    unsigned endUns = endReal;
    double realCount, realSum, realSumSquares, max, min;

    realCount = realSum = realSumSquares = 0.0;
    max = -DBL_MAX;
    min = DBL_MAX;

    assert(startUns != endUns);
    unsigned ceilUns = ceil(startReal);

    if ((ceilUns != startUns) && !isnan(data[startUns]))
	{
	/* need a fraction of the first count */
	double frac = (double)ceilUns - startReal;
	realCount = frac;
	realSum = frac * data[startUns];
	realSumSquares = realSum * realSum;
        if (max < data[startUns])
            max = data[startUns];
        if (min > data[startUns])
            min = data[startUns];
	startUns++;
	}

    // add in all the data that are totally in this pixel
    for(; startUns < endUns; startUns++)
	{
        if (!isnan(data[startUns]))
            {
            realCount += 1.0;
            realSum += data[startUns];
            realSumSquares += data[startUns] * data[startUns];
            if (max < data[startUns])
                max = data[startUns];
            if (min > data[startUns])
                min = data[startUns];
            }
	}

    // add any fraction of the count that's only partially in this pixel
    double lastFrac = endReal - endUns;
    double lastSum = lastFrac * data[endUns];
    if ((lastFrac > 0.0) && (endUns < size) && !isnan(data[endUns]))
	{
	if (max < data[endUns])
	    max = data[endUns];
	if (min > data[endUns])
	    min = data[endUns];
	realCount += lastFrac;
	realSum += lastSum;
	realSumSquares += lastSum * lastSum;
	}

    pe->count = normalizeCount(pe, realCount, min, max, realSum, realSumSquares);
    }
}

static void dataToPixels(double *data, struct preDrawContainer *pre)
/* Sample data into pixels. */
{
unsigned size = winEnd - winStart;
double dataPerPixel = size / (double) insideWidth;

if (dataPerPixel <= 1.0)
    dataToPixelsUp(data, pre);
else
    dataToPixelsDown(data, pre);
}

static void mathWigLoadItems(struct track *tg)
/* Fill up tg->items with bedGraphItems derived from a bigWig file */
{
char *missingSetting;
boolean missingIsZero = TRUE;
if ((missingSetting = cartOrTdbString(cart, tg->tdb, "missingMethod", NULL)) != NULL)
    {
    missingIsZero = differentString(missingSetting, "missing");
    }
char *equation = cloneString(trackDbSetting(tg->tdb, "mathDataUrl"));

struct preDrawContainer *pre = tg->preDrawContainer = initPreDrawContainer(insideWidth);
double *data;
data = mathWigGetValues(database, equation, chromName, winStart, winEnd, missingIsZero);
dataToPixels(data, pre);

free(data);
}

static void bigWigLoadItems(struct track *tg)
/* Fill up tg->items with bedGraphItems derived from a bigWig file */
{
char *extTableString = trackDbSetting(tg->tdb, "extTable");

if (tg->bbiFile == NULL)
    {
    /* Figure out bigWig file name. */
    if (isHubTrack(database) || tg->parallelLoading) // do not use mysql during parallel-fetch or if assembly hub
	{
	char *fileName = cloneString(trackDbSetting(tg->tdb, "bigDataUrl"));
	bigWigOpenCatch(tg, fileName);
	}
    else
	{
	struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
	char *fileName = bbiNameFromSettingOrTable(tg->tdb, conn, tg->table);
	bigWigOpenCatch(tg, fileName);
	// if there's an extra table, read this one in too
	if (extTableString != NULL)
	    {
	    fileName = bbiNameFromSettingOrTable(tg->tdb, conn, extTableString);
	    bigWigOpenCatch(tg, fileName);
	    }
	hFreeConn(&conn);
	}
    }
bigWigLoadPreDraw(tg, winStart, winEnd, insideWidth);
}

void mathWigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
/* Set up bigWig methods. */
{
bigWigMethods(track, tdb, wordCount, words);
track->loadItems = mathWigLoadItems;
}

static struct preDrawContainer *gc5BaseOnTheFlyLoadPreDraw(struct track *tg,
    int seqStart, int seqEnd, int width)
/* Compute GC percent directly from genome sequence in winSize-base
 * non-overlapping windows.  Values are 0-100 to match gc5BaseBw type
 * bigWig 0 100 range.  wigTrack.c windowing/smoothing handles averaging
 * when multiple windows fall on the same pixel.
 */
{
char *winSizeString = trackDbSetting(tg->tdb, "calcWinSize");
int winSize = (int)sqlLongLong(winSizeString);

if (tg->preDrawContainer)
    return tg->preDrawContainer;

struct preDrawContainer *pre = tg->preDrawContainer = initPreDrawContainer(width);
struct preDrawElement *preDraw = pre->preDraw;
int preDrawZero = pre->preDrawZero;
int preDrawSize = pre->preDrawSize;

struct gcOnTheFlyWindow *windows = NULL;
int windowCount = gcOnTheFlyCompute(database, chromName, seqStart, seqEnd,
    winSize, &windows);
if (windowCount == 0)
    return pre;

/* pixelsPerBase is based on the visible window, not the extended fetch range,
 * so that pixel coordinates are correct relative to the display. */
double pixelsPerBase = (double)width / (winEnd - winStart);
int span = winSize;
int wi;

for (wi = 0; wi < windowCount; wi++)
    {
    double gcPct = windows[wi].gcPct;
    int chromPos = windows[wi].chromStart;

    /* Map this span to pixel coordinates relative to winStart.
     * Data outside the visible window lands in the preDraw margin slots
     * (negative or >= width), which is correct for smoothing at edges.
     * Fill every pixel covered by this span. */
    int x1 = (int)((chromPos - winStart) * pixelsPerBase);
    int x2 = (int)((chromPos + span - winStart) * pixelsPerBase);
    int xi;
    for (xi = x1; xi <= x2; ++xi)
        {
        int xCoord = preDrawZero + xi;
        if (xCoord >= 0 && xCoord < preDrawSize)
            {
            preDraw[xCoord].count++;
            if (gcPct > preDraw[xCoord].max) preDraw[xCoord].max = gcPct;
            if (gcPct < preDraw[xCoord].min) preDraw[xCoord].min = gcPct;
            preDraw[xCoord].sumData    += gcPct;
            preDraw[xCoord].sumSquares += gcPct * gcPct;
            }
        }
    }

freeMem(windows);
return pre;
}

static void gc5BaseOnTheFlyLoadItems(struct track *tg)
/* Compute GC percent from genome sequence; called in the loadItems phase
 * just as bigWigLoadItems calls bigWigLoadPreDraw to fill preDrawContainer.
 * Fetch sequence that covers the preDraw smoothing margins on each side so
 * the smoothing/averaging machinery has data at the window edges. */
{
tg->items = NULL;
tg->mapsSelf = TRUE;
/* Extend the fetch range by wiggleSmoothingMax pixels worth of bases on
 * each side, rounded up to the nearest 5-base span. */
double basesPerPixel = (double)(winEnd - winStart) / insideWidth;
int marginBases = ((int)(wiggleSmoothingMax * basesPerPixel) / 5 + 1) * 5;
int fetchStart = max(0, winStart - marginBases);
int fetchEnd   = min(hChromSize(database, chromName), winEnd + marginBases);
gc5BaseOnTheFlyLoadPreDraw(tg, fetchStart, fetchEnd, insideWidth);
}

void bigWigMethods(struct track *track, struct trackDb *tdb,
	int wordCount, char *words[])
/* Set up bigWig methods. */
{
bedGraphMethods(track, tdb, wordCount, words);
track->loadItems = bigWigLoadItems;
track->preDrawItems = bigWigPreDrawItems;
track->preDrawMultiRegion = wigMultiRegionGraphLimits;
track->drawItems = bigWigDrawItems;
track->loadPreDraw = bigWigLoadPreDraw;
}

void gc5BaseOnTheFlyMethods(struct track *tg, struct cart *cart)
/* Install on-the-fly GC computation methods on a track that was
 *     loaded from trackDb.  Overrides the bigWig loadItems/loadPreDraw
 *     so data is computed from genome sequence instead of read from a file.
 */
{
tg->loadItems   = gc5BaseOnTheFlyLoadItems;
tg->loadPreDraw = gc5BaseOnTheFlyLoadPreDraw;
}

struct track *gc5BaseOnTheFlyTg(struct cart *cart)
/* Create an on-the-fly GC percent track computed directly from
 *     genome sequence.  Default visibility is dense; the cart
 *     override happens later in the getTrackList() visibility loop.
 */
{
struct track *tg = trackNew();
struct trackDb *tdb = trackDbNew();
char longLabel[1024];
safef(longLabel, sizeof(longLabel), "GC FLY Percent in %s-Base Windows", gcOnFlyWinSize(cart));

/* Fill in trackDb fields needed by wigCartOptionsNew and bigWigMethods. */
tdb->track      = cloneString(GC_ON_FLY_TRACK_NAME);
tdb->table      = cloneString(GC_ON_FLY_TRACK_NAME);
tdb->type       = cloneString("bigWig 0 100");
tdb->shortLabel = cloneString(GC_ON_FLY_TRACK_LABEL);
tdb->longLabel  = cloneString(longLabel);
tdb->grp        = cloneString("map");
tdb->canPack    = 0;
tdb->visibility = tvHide;

/* Add wig display settings to match what gc5BaseBw trackDb would have. */
trackDbAddSetting(tdb, "autoScale",         "Off");
trackDbAddSetting(tdb, "viewLimits",        "30:70");
trackDbAddSetting(tdb, "maxHeightPixels",   "128:36:16");
trackDbAddSetting(tdb, "graphTypeDefault",  "Bar");
trackDbAddSetting(tdb, "gridDefault",       "OFF");
trackDbAddSetting(tdb, "windowingFunction", "Mean");
trackDbAddSetting(tdb, "color",             "0,0,0");
trackDbAddSetting(tdb, "altColor",          "128,128,128");
// trackDbAddSetting(tdb, "gcComputeOnTheFly", "on");
// trackDbAddSetting(tdb, "gcOnTheFlyMaxBases", "500000");
// trackDbAddSetting(tdb, "gcFallbackBigWig", "/gbdb/ce1x/bbi/gc5BaseBw/gc5Base.bw");
trackDbAddSetting(tdb, "calcWinSize", gcOnFlyWinSize(cart));
trackDbPolish(tdb);

/* Set up bigWig display methods (drawItems, preDrawItems, etc.) */
char *words[] = {"bigWig", "0", "100"};
bigWigMethods(tg, tdb, 3, words);

/* Override data loading with on-the-fly sequence computation. */
tg->loadItems   = gc5BaseOnTheFlyLoadItems;
tg->loadPreDraw = gc5BaseOnTheFlyLoadPreDraw;

/* Fill in track struct fields. */
tg->tdb              = tdb;
tg->track            = tdb->track;
tg->table            = tdb->table;
tg->visibility       = tdb->visibility;
tg->shortLabel       = tdb->shortLabel;
tg->longLabel        = tdb->longLabel;
tg->priority         = tdb->priority;
tg->defaultPriority  = tg->priority;
tg->groupName        = tdb->grp;
tg->defaultGroupName = cloneString(tg->groupName);
tg->hasUi            = TRUE;
return tg;
}
