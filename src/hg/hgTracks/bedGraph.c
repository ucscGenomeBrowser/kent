/* bedGraph - stuff to handle loading and display of
 * bedGraph type tracks in browser.   Will be graphing a specified
 *	column of a bed file.
 */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "wiggle.h"
#include "scoredRef.h"
#include "customTrack.h"
#include "wigCommon.h"

static char const rcsid[] = "$Id: bedGraph.c,v 1.24 2010/05/20 19:53:22 kent Exp $";

/*	The item names have been massaged during the Load.  An
 *	individual item may have been read in on multiple table rows and
 *	had an extension on it to make it unique from the others.  Also,
 *	each different zoom level had a different extension name.
 *	All these names were condensed into the root of the name with
 *	the extensions removed.
 */
static char *bedGraphName(struct track *tg, void *item)
/* Return name of bedGraph level track. */
{
struct bedGraphItem *bg = item;
return bg->name;
}

void ctBedGraphLoadItems(struct track *tg)
{
/*	load custom bedGraph track data	*/

/*	Verify this is a custom track	*/
if (! (isCustomTrack(tg->table) && tg->customPt))
    errAbort("ctBedGraphLoadItems: did not find a custom wiggle track: %s", tg->track);

errAbort("custom track bedGraph load items not yet implemented");
}

/*	bedGraphLoadItems - an ordinary bed load, but we are interested
 *		in only the chrom, start, end, and the graphColumn
 */
static void bedGraphLoadItems(struct track *tg)
{
struct sqlConnection *conn;
struct sqlResult *sr = (struct sqlResult *) NULL;
char **row = (char **)NULL;
int rowOffset = 0;
struct bedGraphItem *bgList = NULL;
int itemsLoaded = 0;
int colCount = 0;
struct wigCartOptions *wigCart = (struct wigCartOptions *) tg->extraUiData;
int graphColumn = 5;
char *tableName;

if(sameString(tg->table, "affyTranscription"))
    wigCart->colorTrack = "affyTransfrags";
graphColumn = wigCart->graphColumn;


#ifndef GBROWSE
if (isCustomTrack(tg->table) && tg->customPt)
    {
    struct customTrack *ct = (struct customTrack *) tg->customPt;
    tableName = ct->dbTableName;
    conn = hAllocConn(CUSTOM_TRASH);
    }
else 
#endif /* GBROWSE */
    {
    tableName = tg->table;
    conn = hAllocConnTrack(database, tg->tdb);
    }

sr = hRangeQuery(conn, tableName, chromName, winStart, winEnd, NULL,
	&rowOffset);

colCount = sqlCountColumns(sr) - rowOffset;
/*	Must have at least four good columns	*/
if (colCount < 4)
    errAbort("bedGraphLoadItems: table %s only has %d data columns, must be at least 4", tableName, colCount);

if (colCount < graphColumn)
    errAbort("bedGraphLoadItems: table %s only has %d data columns, specified graph column %d does not exist",
	tableName, colCount, graphColumn);

/*	before loop, determine actual row[graphColumn] index */
graphColumn += (rowOffset - 1);

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bedGraphItem *bg;
    struct bed *bed;

    ++itemsLoaded;

    /*	load chrom, start, end	*/
    bed = bedLoadN(row+rowOffset, 3);

    AllocVar(bg);
    bg->start = bed->chromStart;
    bg->end = bed->chromEnd;
    if ((colCount > 4) && ((graphColumn + rowOffset) != 4))
	bg->name = cloneString(row[3+rowOffset]);
    else
	{
	char name[128];
	safef(name,ArraySize(name),"%s.%d", bed->chrom, itemsLoaded);
	bg->name = cloneString(name);
	}
    bg->dataValue = sqlFloat(row[graphColumn]);
    /* filled in by DrawItems	*/
    bg->graphUpperLimit = wigEncodeStartingUpperLimit;
    bg->graphLowerLimit = wigEncodeStartingLowerLimit;
    slAddHead(&bgList, bg);
    bedFree(&bed);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);

slReverse(&bgList);
tg->items = bgList;
}	/*	bedGraphLoadItems()	*/

static void bedGraphFreeItems(struct track *tg) {
#if defined(DEBUG)
snprintf(dbgMsg, DBGMSGSZ, "I haven't seen bedGraphFreeItems ever called ?");
wigDebugPrint("bedGraphFreeItems");
#endif
}

struct preDrawContainer *bedGraphLoadPreDraw(struct track *tg, int seqStart, int seqEnd, int width)
/* Do bits that load the predraw buffer tg->preDrawContainer. */
{
/* Just need to do this once... */
if (tg->preDrawContainer)
    return tg->preDrawContainer;

struct bedGraphItem *wi;
double pixelsPerBase = scaleForPixels(width);
double basesPerPixel = 1.0;
int i;				/* an integer loop counter	*/
if (pixelsPerBase > 0.0)
    basesPerPixel = 1.0 / pixelsPerBase;

/* Allocate predraw and save it and related info in the track. */
struct preDrawContainer *pre = tg->preDrawContainer = initPreDrawContainer(width);
struct preDrawElement *preDraw = pre->preDraw;	/* to accumulate everything in prep for draw */
int preDrawZero = pre->preDrawZero;		/* location in preDraw where screen starts */
int preDrawSize = pre->preDrawSize;		/* size of preDraw array */

/*	walk through all the data fill in the preDraw array	*/
for (wi = tg->items; wi != NULL; wi = wi->next)
    {
    double dataValue = wi->dataValue;	/* the data value to graph */

    /*	Ready to draw, what do we know:
    *	the feature being processed:
    *	chrom coords:  [wi->start : wi->end)
    *	its data value: dataValue = wi->dataValue
    *
    *	The drawing window, in pixels:
    *	xOff = left margin, yOff = top margin, h = height of drawing window
    *	drawing window in chrom coords: seqStart, seqEnd
    *	'basesPerPixel' is known, 'pixelsPerBase' is known
    */
    /*	let's check end point screen coordinates.  If they are
     *	the same, then this entire data block lands on one pixel,
     *	It is OK if these end up + or -, we do want to
     *	keep track of pixels before and after the screen for
     *	later smoothing operations
     */
    int x1 = (wi->start - seqStart) * pixelsPerBase;
    int x2 = (wi->end - seqStart) * pixelsPerBase;

    if (x2 > x1)
	{
	for (i = x1; i <= x2; ++i)
	    {
	    int xCoord = preDrawZero + i;
	    if ((xCoord >= 0) && (xCoord < preDrawSize))
		{
		++preDraw[xCoord].count;
		if (dataValue > preDraw[xCoord].max)
		    preDraw[xCoord].max = dataValue;
		if (dataValue < preDraw[xCoord].min)
		    preDraw[xCoord].min = dataValue;
		preDraw[xCoord].sumData += dataValue;
		preDraw[xCoord].sumSquares += dataValue * dataValue;
		}
	    }
	}
    else
	{	/*	only one pixel for this block of data */
	int xCoord = preDrawZero + x1;
	/*	if the point falls within our array, record it.
	 *	the (wi->validCount > 0) is a safety check.  It
	 *	should always be true unless the data was
	 *	prepared incorrectly.
	 */
	if ((xCoord >= 0) && (xCoord < preDrawSize))
	    {
	    ++preDraw[xCoord].count;
	    if (dataValue > preDraw[xCoord].max)
		preDraw[xCoord].max = dataValue;
	    if (dataValue < preDraw[xCoord].min)
		preDraw[xCoord].min = dataValue;
	    preDraw[xCoord].sumData += dataValue;
	    preDraw[xCoord].sumSquares += dataValue * dataValue;
	    }
	}
    }	/*	for (wi = tg->items; wi != NULL; wi = wi->next)	*/
return pre;
}

static void bedGraphDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
struct preDrawContainer *pre = bedGraphLoadPreDraw(tg, seqStart, seqEnd, width);
if (pre != NULL)
    {
    wigDrawPredraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis,
		   pre, pre->preDrawZero, pre->preDrawSize, 
		   &tg->graphUpperLimit, &tg->graphLowerLimit);
    }
}

/*
 *	WARNING ! - track->visibility is merely the default value
 *	from the trackDb entry at this time.  It will be set after this
 *	 by hgTracks from its cart UI setting.  When called in
 *	 TotalHeight it will then be the requested visibility.
 */
void bedGraphMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
{
struct wigCartOptions *wigCart = wigCartOptionsNew(cart, tdb, wordCount, words);
wigCart->bedGraph = TRUE;	/*	signal to left labels	*/
switch (wordCount)
    {
    case 2:
	wigCart->graphColumn = atoi(words[1]);
	/*	protect against nonsense values	*/
	if ( (wigCart->graphColumn < 2) ||
	    (wigCart->graphColumn > 100) )
	    wigCart->graphColumn = 5; /* default score column */

	break;
    default:
	wigCart->graphColumn = 5; /* default score column */
	break;
    } 

track->minRange = wigCart->minY;
track->maxRange = wigCart->maxY;
track->graphUpperLimit = wigEncodeStartingUpperLimit;
track->graphLowerLimit = wigEncodeStartingLowerLimit;

track->loadItems = bedGraphLoadItems;
track->freeItems = bedGraphFreeItems;
track->drawItems = bedGraphDrawItems;
track->itemName = bedGraphName;
track->mapItemName = bedGraphName;
track->totalHeight = wigTotalHeight;
track->itemHeight = tgFixedItemHeight;

track->itemStart = tgItemNoStart;
track->itemEnd = tgItemNoEnd;
track->mapsSelf = TRUE;
track->extraUiData = (void *) wigCart;
track->colorShades = shadesOfGray;
track->drawLeftLabels = wigLeftLabels;
track->loadPreDraw = bedGraphLoadPreDraw;
/*	the lfSubSample type makes the image map function correctly */
track->subType = lfSubSample;     /*make subType be "sample" (=2)*/

}	/*	bedGraphMethods()	*/
