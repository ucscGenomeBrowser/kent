/* bigWigTrack - stuff to handle loading and display of bigWig type tracks in browser.   */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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

struct preDrawContainer *bigWigLoadPreDraw(struct track *tg, int seqStart, int seqEnd, int width)
/* Do bits that load the predraw buffer tg->preDrawContainer. */
{
/* Just need to do this once... */
if (tg->preDrawContainer != NULL)
    return tg->preDrawContainer;

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

	if (bigWigSummaryArrayExtended(bbiFile, chromName, winStart, winEnd, summarySize, summary))
	    {
	    /* Convert format to predraw */
	    int i;
	    int preDrawZero = pre->preDrawZero;
	    struct preDrawElement *preDraw = pre->preDraw;
	    for (i=0; i<summarySize; ++i)
		{
		struct preDrawElement *pe = &preDraw[i + preDrawZero];
		struct bbiSummaryElement *be = &summary[i];
		pe->count = be->validCount;
		pe->min = be->minVal;
		pe->max = be->maxVal;
		pe->sumData = be->sumData;
		pe->sumSquares = be->sumSquares;
		}
	    }
	bbiNext = bbiFile->next;
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
}

static void bigWigOpenCatch(struct track *tg, char *fileName)
/* Try to open big wig file, store error in track struct */
{
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    struct bbiFile *bbiFile = bigWigFileOpen(fileName);
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
    max = min = data[startUns];

    assert(startUns != endUns);
    unsigned ceilUns = ceil(startReal);

    if (ceilUns != startUns)
	{
	/* need a fraction of the first count */
	double frac = (double)ceilUns - startReal;
	realCount = frac;
	realSum = frac * data[startUns];
	realSumSquares = realSum * realSum;
	startUns++;
	}

    // add in all the data that are totally in this pixel
    for(; startUns < endUns; startUns++)
	{
	realCount += 1.0;
	realSum += data[startUns];
	realSumSquares += data[startUns] * data[startUns];
	if (max < data[startUns])
	    max = data[startUns];
	if (min > data[startUns])
	    min = data[startUns];
	}

    // add any fraction of the count that's only partially in this pixel
    double lastFrac = endReal - endUns;
    double lastSum = lastFrac * data[endUns];
    if ((lastFrac > 0.0) && (endUns < size))
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
boolean missingIsZero = FALSE;
if ((missingSetting = trackDbSetting(tg->tdb, "missingMethod")) != NULL)
    {
    missingIsZero = sameString(missingSetting, "missing");
    }
char *equation = cloneString(trackDbSetting(tg->tdb, "mathDataUrl"));

struct preDrawContainer *pre = tg->preDrawContainer = initPreDrawContainer(insideWidth);
double *data;
if (missingIsZero)
    data = mathWigGetValuesMissing(equation, chromName, winStart, winEnd);
else
    data = mathWigGetValues(equation, chromName, winStart, winEnd);
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
    if (tg->parallelLoading) // do not use mysql during parallel-fetch
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
