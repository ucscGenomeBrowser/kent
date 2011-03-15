/* bigWigTrack - stuff to handle loading and display of bigWig type tracks in browser.   */

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

static void bigWigDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    /* Allocate predraw area. */
    int preDrawZero, preDrawSize;
    struct preDrawContainer *preDrawList = NULL;

    /* Get summary info from bigWig */
    int summarySize = width;
    struct bbiSummaryElement *summary;
    AllocArray(summary, summarySize);

    struct bbiFile *bbiFile ;
    for(bbiFile = tg->bbiFile; bbiFile ; bbiFile = bbiFile->next)
	{
	struct preDrawContainer *preDrawContainer;
	struct preDrawElement *preDraw = initPreDraw(width, &preDrawSize, &preDrawZero);
	AllocVar(preDrawContainer);
	preDrawContainer->preDraw = preDraw;
	slAddHead(&preDrawList, preDrawContainer);

	if (bigWigSummaryArrayExtended(bbiFile, chromName, winStart, winEnd, summarySize, summary))
	    {
	    /* Convert format to predraw */
	    int i;
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
	}

    /* Call actual graphing routine. */
    wigDrawPredraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis,
		   preDrawList, preDrawZero, preDrawSize, &tg->graphUpperLimit, &tg->graphLowerLimit);

    struct preDrawContainer *nextContain;
    for(; preDrawList ; preDrawList = nextContain)
	{
	nextContain = preDrawList->next;
	freeMem(preDrawList->preDraw);
	freeMem(preDrawList);
	}
    freeMem(summary);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    tg->networkErrMsg = cloneString(errCatch->message->string);
    bigDrawWarning(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
    }
errCatchFree(&errCatch);
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


static void bigWigLoadItems(struct track *tg)
/* Fill up tg->items with bedGraphItems derived from a bigWig file */
{
char *extTableString = trackDbSetting(tg->tdb, "extTable");

if (tg->bbiFile == NULL)
    {
    /* Figure out bigWig file name. */
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

void bigWigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
/* Set up bigWig methods. */
{
bedGraphMethods(track, tdb, wordCount, words);
track->loadItems = bigWigLoadItems;
track->drawItems = bigWigDrawItems;
}
