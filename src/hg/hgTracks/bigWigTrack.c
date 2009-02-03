/* bigWigTrack - stuff to handle loading and display of bigWig type tracks in browser.   */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
// #include "wiggle.h"
// #include "scoredRef.h"
// #include "customTrack.h"
#include "localmem.h"
#include "wigCommon.h"
#include "bbiFile.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: bigWigTrack.c,v 1.2 2009/02/03 03:31:47 kent Exp $";

void wigDrawPredraw(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis, struct preDrawElement *preDraw,
	int preDrawZero, int preDrawSize, double *retGraphUpperLimit, double *retGraphLowerLimit);
/* Draw once we've figured out predraw... */

static void bigWigDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
/* Allocate predraw area. */
int preDrawZero, preDrawSize;
struct preDrawElement *preDraw = initPreDraw(width, &preDrawSize, &preDrawZero);

/* Figure out bigWig file name. */
struct sqlConnection *conn = hAllocConn(database);
char query[256];
safef(query, sizeof(query), "select fileName from %s", tg->mapName);
char *wigFileName = sqlQuickString(conn, query);
if (wigFileName == NULL)
    errAbort("Missing fileName in %s table", tg->mapName);
hFreeConn(&conn);

/* Get summary info from bigWig */
int summarySize = width;
struct bbiSummaryElement *summary;
AllocArray(summary, summarySize);
if (bigWigSummaryArrayExtended(wigFileName, chromName, winStart, winEnd, summarySize, summary))
    {
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

    /* Call actual graphing routine. */
    wigDrawPredraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis,
		   preDraw, preDrawZero, preDrawSize, NULL, NULL);
    }

freeMem(preDraw);
freeMem(summary);
}

static void bigWigLoadItems(struct track *tg)
/* Fill up tg->items with bedGraphItems derived from a bigWig file */
{
#ifdef OLD
/* Figure out bigWig file name. */
struct sqlConnection *conn = hAllocConn(database);
char query[256];
safef(query, sizeof(query), "select fileName from %s", tg->mapName);
char *wigFileName = sqlQuickString(conn, query);
if (wigFileName == NULL)
    errAbort("Missing fileName in %s table", tg->mapName);
hFreeConn(&conn);

/* Get interval list from bigWig. */
struct lm *lm = lmInit(0);
struct bbiFile *bwf = bigWigFileOpen(wigFileName);
struct bbiInterval *bi, *biList = bigWigIntervalQuery(bwf, chromName, winStart, winEnd, lm);

/* Convert from bigWig's interval structure to bedGraphItem. */
struct bedGraphItem *bgList = NULL;
for (bi = biList; bi != NULL; bi = bi->next)
    {
    struct bedGraphItem *bg;
    AllocVar(bg);
    bg->start = bi->start;
    bg->end = bi->end;
    bg->dataValue = bi->val;
    slAddHead(&bgList, bg);
    }
slReverse(&bgList);

/* Clean up and go home. */
bbiFileClose(&bwf);
lmCleanup(&lm);
tg->items = bgList;
#endif /* OLD */
}

void bigWigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
/* Set up bigWig methods. */
{
bedGraphMethods(track, tdb, wordCount, words);
track->loadItems = bigWigLoadItems;
track->drawItems = bigWigDrawItems;
}
