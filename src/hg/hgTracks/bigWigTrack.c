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

static char const rcsid[] = "$Id: bigWigTrack.c,v 1.11 2010/05/11 01:43:26 kent Exp $";

static void bigWigDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
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

static void bigWigLoadItems(struct track *tg)
/* Fill up tg->items with bedGraphItems derived from a bigWig file */
{
char *extTableString = trackDbSetting(tg->tdb, "extTable");

if (extTableString != NULL)
    {
    // if there's an extra table, read this one in too
    struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
    char *fileName = bbiNameFromSettingOrTable(tg->tdb, conn, tg->table);
    struct bbiFile *bbiFile = bigWigFileOpen(fileName);
    slAddHead(&tg->bbiFile, bbiFile);

    fileName = bbiNameFromSettingOrTable(tg->tdb, conn, extTableString);
    bbiFile = bigWigFileOpen(fileName);
    slAddHead(&tg->bbiFile, bbiFile);

    hFreeConn(&conn);
    }
else
    {
    if (tg->bbiFile == NULL)
	{
	/* Figure out bigWig file name. */
	struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
	char *fileName = bbiNameFromSettingOrTable(tg->tdb, conn, tg->table);
	tg->bbiFile = bigWigFileOpen(fileName);
	hFreeConn(&conn);
	}
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
