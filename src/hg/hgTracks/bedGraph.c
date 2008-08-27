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

static char const rcsid[] = "$Id: bedGraph.c,v 1.13 2008/08/27 23:06:37 kate Exp $";

struct bedGraphItem
/* A bedGraph track item. */
    {
    struct bedGraphItem *next;
    int start, end;	/* Start/end in chrom coordinates. */
    char *name;		/* Common name */
    float dataValue;	/* data value from bed table graphColumn	*/
    double graphUpperLimit;	/* filled in by DrawItems	*/
    double graphLowerLimit;	/* filled in by DrawItems	*/
    };

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
if (tg->customPt == (void *)NULL)
    errAbort("ctBedGraphLoadItems: did not find a custom wiggle track: %s", tg->mapName);

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
struct wigCartOptions *wigCart = (struct wigCartOptions *) NULL;
int graphColumn = 5;
char *tableName;

wigCart = (struct wigCartOptions *) tg->extraUiData;
graphColumn = wigCart->graphColumn;


#ifndef GBROWSE
if (tg->customPt)
    {
    struct customTrack *ct = (struct customTrack *) tg->customPt;
    tableName = ct->dbTableName;
    conn = sqlCtConn(TRUE);
    }
else 
#endif /* GBROWSE */
    {
    tableName = tg->mapName;
    conn = hAllocConn();
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
    bg->graphUpperLimit = -1.0e+300; /* filled in by DrawItems	*/
    bg->graphLowerLimit = 1.0e+300; /* filled in by DrawItems	*/
    slAddHead(&bgList, bg);
    bedFree(&bed);
    }

sqlFreeResult(&sr);
hFreeOrDisconnect(&conn);

slReverse(&bgList);
tg->items = bgList;
}	/*	bedGraphLoadItems()	*/

static void bedGraphFreeItems(struct track *tg) {
#if defined(DEBUG)
snprintf(dbgMsg, DBGMSGSZ, "I haven't seen bedGraphFreeItems ever called ?");
wigDebugPrint("bedGraphFreeItems");
#endif
}

static void bedGraphDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
struct bedGraphItem *wi;
double pixelsPerBase = scaleForPixels(width);
double basesPerPixel = 1.0;
int itemCount = 0;
struct wigCartOptions *wigCart;
enum wiggleYLineMarkEnum yLineOnOff;
double yLineMark;
struct preDrawElement *preDraw;	/* to accumulate everything in prep for draw */
int preDrawZero;		/* location in preDraw where screen starts */
int preDrawSize;		/* size of preDraw array */
int i;				/* an integer loop counter	*/
double overallUpperLimit = -1.0e+300;	/*	determined from data	*/
double overallLowerLimit = 1.0e+300;	/*	determined from data	*/
double overallRange;		/*	determined from data	*/
double graphUpperLimit;		/*	scaling choice will set these	*/
double graphLowerLimit;		/*	scaling choice will set these	*/
double graphRange;		/*	scaling choice will set these	*/
double epsilon;			/*	range of data in one pixel	*/
int x1 = 0;			/*	screen coordinates	*/
int x2 = 0;			/*	screen coordinates	*/
Color *colorArray = NULL;       /*	Array of pixels to be drawn.	*/
wigCart = (struct wigCartOptions *) tg->extraUiData;
if(sameString(tg->mapName, "affyTranscription"))
    wigCart->colorTrack = "affyTransfrags";

yLineOnOff = wigCart->yLineOnOff;
yLineMark = wigCart->yLineMark;

if (pixelsPerBase > 0.0)
    basesPerPixel = 1.0 / pixelsPerBase;

/*	width - width of drawing window in pixels
 *	pixelsPerBase - pixels per base
 *	basesPerPixel - calculated as 1.0/pixelsPerBase
 */
itemCount = 0;

preDraw = initPreDraw(width, &preDrawSize, &preDrawZero);

/*	walk through all the data and prepare the preDraw array	*/
for (wi = tg->items; wi != NULL; wi = wi->next)
    {
    double dataValue = wi->dataValue;	/* the data value to graph */

    ++itemCount;

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
    x1 = (wi->start - seqStart) * pixelsPerBase;
    x2 = (wi->end - seqStart) * pixelsPerBase;

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

/*	now we are ready to draw.  Each element in the preDraw[] array
 *	cooresponds to a single pixel on the screen
 */

preDrawWindowFunction(preDraw, preDrawSize, wigCart->windowingFunction);
preDrawSmoothing(preDraw, preDrawSize, wigCart->smoothingWindow);
overallRange = preDrawLimits(preDraw, preDrawZero, width,
    &overallUpperLimit, &overallLowerLimit);
graphRange = preDrawAutoScale(preDraw, preDrawZero, width,
    wigCart->autoScale,
    &overallUpperLimit, &overallLowerLimit,
    &graphUpperLimit, &graphLowerLimit,
    &overallRange, &epsilon, tg->lineHeight,
    wigCart->maxY, wigCart->minY);

/*
 *	We need to put the graphing limits back into the items
 *	so the LeftLabels routine can find these numbers.
 *	This may seem like overkill to put it into each item but
 *	this will become necessary when these graphs stack up upon
 *	each other in a multiple item display, each one will have its
 *	own graph.
 */
for (wi = tg->items; wi != NULL; wi = wi->next)
    {
	wi->graphUpperLimit = graphUpperLimit;
	wi->graphLowerLimit = graphLowerLimit;
    }

colorArray = allocColorArray(preDraw, width, preDrawZero,
    wigCart->colorTrack, tg, hvg);

graphPreDraw(preDraw, preDrawZero, width,
    tg, hvg, xOff, yOff, graphUpperLimit, graphLowerLimit, graphRange,
    epsilon, colorArray, vis, wigCart->lineBar);

drawZeroLine(vis, wigCart->horizontalGrid,
    graphUpperLimit, graphLowerLimit,
    hvg, xOff, yOff, width, tg->lineHeight);

drawArbitraryYLine(vis, wigCart->yLineOnOff,
    graphUpperLimit, graphLowerLimit,
    hvg, xOff, yOff, width, tg->lineHeight, wigCart->yLineMark, graphRange,
    wigCart->yLineOnOff);

wigMapSelf(tg, hvg, seqStart, seqEnd, xOff, yOff, width);

freez(&colorArray);
freeMem(preDraw);
}	/*	bedGraphDrawItems()	*/

void wigBedGraphFindItemLimits(void *items,
    double *graphUpperLimit, double *graphLowerLimit)
/*	find upper and lower limits of graphed items (bedGraphItem)	*/
{
struct bedGraphItem *wi;
*graphUpperLimit = -1.0e+300;
*graphLowerLimit = 1.0e+300;

for (wi = items; wi != NULL; wi = wi->next)
    {
    if (wi->graphUpperLimit > *graphUpperLimit)
	*graphUpperLimit = wi->graphUpperLimit;
    if (wi->graphLowerLimit < *graphLowerLimit)
	*graphLowerLimit = wi->graphLowerLimit;
    }
}	/*	wigBedGraphFindItemLimits	*/

/*
 *	WARNING ! - track->visibility is merely the default value
 *	from the trackDb entry at this time.  It will be set after this
 *	 by hgTracks from its cart UI setting.  When called in
 *	 TotalHeight it will then be the requested visibility.
 */
void bedGraphMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
{
int defaultHeight = atoi(DEFAULT_HEIGHT_PER);  /* truncated by limits	*/
double minY = 0.0;	/*	from trackDb or cart, requested minimum */
double maxY = 1000.0;	/*	from trackDb or cart, requested maximum */
double yLineMark;	/*	from trackDb or cart */
struct wigCartOptions *wigCart;
int maxHeight = atoi(DEFAULT_HEIGHT_PER);
int minHeight = MIN_HEIGHT_PER;

AllocVar(wigCart);
char *name = compositeViewControlNameFromTdb(tdb);

/*	These Fetch functions look for variables in the cart bounded by
 *	limits specified in trackDb or returning defaults
 */
wigCart->lineBar = wigFetchGraphTypeWithCart(cart, tdb, name, (char **) NULL);
wigCart->horizontalGrid = wigFetchHorizontalGridWithCart(cart, tdb, name, (char **) NULL);

wigCart->autoScale = wigFetchAutoScaleWithCart(cart, tdb, name,(char **) NULL);
wigCart->windowingFunction = wigFetchWindowingFunctionWithCart(cart, tdb, name,(char **) NULL);
wigCart->smoothingWindow = wigFetchSmoothingWindowWithCart(cart, tdb, name,(char **) NULL);

wigFetchMinMaxPixelsWithCart(cart, tdb, name,&minHeight, &maxHeight, &defaultHeight);
wigFetchYLineMarkValueWithCart(cart, tdb, name,&yLineMark);
wigCart->yLineMark = yLineMark;
wigCart->yLineOnOff = wigFetchYLineMarkWithCart(cart, tdb, name,(char **) NULL);

wigCart->maxHeight = maxHeight;
wigCart->defaultHeight = defaultHeight;
wigCart->minHeight = minHeight;

wigFetchMinMaxLimitsWithCart(cart, tdb, name,&minY, &maxY, (double *)NULL, (double *)NULL);

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

track->minRange = minY;
track->maxRange = maxY;

wigCart->minY = track->minRange;
wigCart->maxY = track->maxRange;
wigCart->bedGraph = TRUE;	/*	signal to left labels	*/

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
/*	the lfSubSample type makes the image map function correctly */
track->subType = lfSubSample;     /*make subType be "sample" (=2)*/

}	/*	bedGraphMethods()	*/
