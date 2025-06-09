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
