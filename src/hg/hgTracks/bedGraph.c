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

static char const rcsid[] = "$Id: bedGraph.c,v 1.3 2005/02/09 19:52:16 hiram Exp $";

/*	bedGraphCartOptions structure - to carry cart options from
 *	bedGraphMethods to all the other methods via the
 *	track->extraUiData pointer
 */
struct bedGraphCartOptions
    {
    enum wiggleGridOptEnum horizontalGrid;	/*  grid lines, ON/OFF */
    enum wiggleGraphOptEnum lineBar;		/*  Line or Bar chart */
    enum wiggleScaleOptEnum autoScale;		/*  autoScale on */
    enum wiggleWindowingEnum windowingFunction;	/*  max,mean,min */
    enum wiggleSmoothingEnum smoothingWindow;	/*  N: [1:15] */
    enum wiggleYLineMarkEnum yLineOnOff;	/*  OFF/ON	*/
    double minY;	/*	from trackDb.ra words, the absolute minimum */
    double maxY;	/*	from trackDb.ra words, the absolute maximum */
    int maxHeight;	/*	maximum pixels height from trackDb	*/
    int defaultHeight;	/*	requested height from cart	*/
    int minHeight;	/*	minimum pixels height from trackDb	*/
    double yLineMark;	/*	user requested line at y = */
    char *colorTrack;   /*	Track to use for coloring the graph. */
    int graphColumn;	/*	column to be graphing	*/
    };

struct preDrawElement
    {
    double	max;	/*	maximum value seen for this point	*/
    double	min;	/*	minimum value seen for this point	*/
    unsigned long long	count;	/* number of datum at this point */
    double	sumData;	/*	sum of all values at this point	*/
    double  sumSquares;	/* sum of (values squared) at this point */
    double  plotValue;	/*	raw data to plot	*/
    double  smooth;	/*	smooth data values	*/
    };

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


static void bedGraphFillInColorLfArray(struct track *bedGraphTrack, Color *colArray, int colSize,
				  struct track *colorTrack)
/* Fill in a color array with the linkedFeatures based colorTrack's
   color where it would normally have an exon. */
{
struct linkedFeatures *lf = NULL, *lfList = colorTrack->items;
struct simpleFeature *sf = NULL;
double scale = scaleForPixels(colSize);
int x1 = 0, x2 = 0;
int i = 0;
for(lf = lfList; lf != NULL; lf = lf->next)
    {
    for (sf = lf->components; sf != NULL; sf = sf->next)
	{
	x1 = round((double)((int)sf->start-winStart)*scale);
	x2 = round((double)((int)sf->end-winStart)*scale);
	if(x1 < 0)
	    x1 = 0;
	if(x2 > colSize)
	    x2 = colSize;
	if(x1 == x2)
	    x2++;
	for(i = x1; i < x2; i++)
	    colArray[i] = colorTrack->ixAltColor;
	}
    }
}

static void bedGraphFillInColorBedArray(struct track *bedGraphTrack, Color *colArray, int colSize,
				  struct track *colorTrack, struct vGfx *vg)
/* Fill in a color array with the simple bed based colorTrack's
   color where it would normally have an block. */
{
struct bed *bed = NULL, *bedList = colorTrack->items;
double scale = scaleForPixels(colSize);
int x1 = 0, x2 = 0;
int i = 0;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    x1 = round((double)((int)bed->chromStart-winStart)*scale);
    x2 = round((double)((int)bed->chromEnd-winStart)*scale);
    if(x1 < 0)
	x1 = 0;
    if(x2 > colSize)
	x2 = colSize;
    if(x1 == x2)
	x2++;
    for(i = x1; i < x2; i++)
	colArray[i] = colorTrack->itemColor(colorTrack, bed, vg);
    }
}

void bedGraphFillInColorArray(struct track *bedGraphTrack, struct vGfx *vg, 
			 Color *colorArray, int colSize, struct track *colorTrack)
/* Fill in a color array with the colorTrack's color where
   it would normally have an exon. */
{
boolean trackLoaded = FALSE;
/* If the track is hidden currently, load the items. */
if(colorTrack->visibility == tvHide)
    {
    trackLoaded = TRUE;
    colorTrack->loadItems(colorTrack);
    colorTrack->ixColor = vgFindRgb(vg, &colorTrack->color);
    colorTrack->ixAltColor = vgFindRgb(vg, &colorTrack->altColor);
    }

if(colorTrack->drawItemAt == linkedFeaturesDrawAt)
    bedGraphFillInColorLfArray(bedGraphTrack, colorArray, colSize, colorTrack);
else if(colorTrack->drawItemAt == bedDrawSimpleAt)
    bedGraphFillInColorBedArray(bedGraphTrack, colorArray, colSize, colorTrack, vg);

if(trackLoaded && colorTrack->freeItems != NULL)
    colorTrack->freeItems(colorTrack);
}

void bedGraphSetCart(struct track *track, char *dataID, void *dataValue)
    /*	set one of the variables in the bedGraphCart	*/
{
struct bedGraphCartOptions *bedGraphCart;
bedGraphCart = (struct bedGraphCartOptions *) track->extraUiData;

if (sameWord(dataID, MIN_Y))
    bedGraphCart->minY = *((double *)dataValue);
else if (sameWord(dataID, MAX_Y))
    bedGraphCart->maxY = *((double *)dataValue);
}

/*	these two routines unused at this time	*/
#if defined(NOT)
static void bedGraphItemFree(struct bedGraphItem **pEl)
    /* Free up a bedGraphItem. */
{
struct bedGraphItem *el = *pEl;
if (el != NULL)
    {
    /* freeMem(el->name);	DO NOT - this belongs to tg->mapName */
    freeMem(el->db);
    freeMem(el->file);
    freez(pEl);
    }
}

static void bedGraphItemFreeList(struct bedGraphItem **pList)
    /* Free a list of dynamically allocated bedGraphItem's */
{
struct bedGraphItem *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    bedGraphItemFree(&el);
    }
*pList = NULL;
}
#endif

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

int bedGraphTotalHeight(struct track *tg, enum trackVisibility vis)
/* Wiggle track will use this to figure out the height they use
   as defined in the cart */
{
struct bedGraphCartOptions *bedGraphCart;

bedGraphCart = (struct bedGraphCartOptions *) tg->extraUiData;

/*
 *	A track is just one
 *	item, so there is nothing to do here, either it is the tvFull
 *	height as chosen by the user from TrackUi, or it is the dense
 *	mode.
 */
/*	Wiggle tracks depend upon clipping.  They are reporting
 *	totalHeight artifically high by 1 so this will leave a
 *	blank area one pixel high below the track.  hgTracks will set
 *	our clipping rectangle one less than what we report here to get
 *	this accomplished.  In the meantime our actual drawing height is
 *	recorded properly in lineHeight, heightPer and height
 */
if (vis == tvDense)
    tg->lineHeight = tl.fontHeight+1;
else if (vis == tvFull)
    tg->lineHeight = max(bedGraphCart->minHeight, bedGraphCart->defaultHeight);

tg->heightPer = tg->lineHeight;
tg->height = tg->lineHeight;

return tg->height + 1;
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
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = (struct sqlResult *) NULL;
char **row = (char **)NULL;
int rowOffset = 0;
struct bedGraphItem *bgList = NULL;
int itemsLoaded = 0;
int colCount = 0;
struct bedGraphCartOptions *bedGraphCart = (struct bedGraphCartOptions *) NULL;
int graphColumn = 5;

bedGraphCart = (struct bedGraphCartOptions *) tg->extraUiData;
graphColumn = bedGraphCart->graphColumn;
fprintf(stderr, "graphColumn: %d\n", graphColumn);

/*	Verify this is NOT a custom track	*/
if (tg->customPt != (void *)NULL)
    errAbort("bedGraphLoadItems: encountered a custom track: %s in the wrong place", tg->mapName);

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL,
	&rowOffset);

colCount = sqlCountColumns(sr) - rowOffset;
/*	Must have at least four good columns	*/
if (colCount < 4)
    errAbort("bedGraphLoadItems: table %s only has %d data columns, must be at least 4", tg->mapName, colCount);

if (colCount < graphColumn)
    errAbort("bedGraphLoadItems: table %s only has %d data columns, specified graph column %d does not exist",
	tg->mapName, colCount, graphColumn);

/*	before loop, determine actual row[graphColumn] index */
graphColumn += (rowOffset - 1);
fprintf(stderr, "graphColumn: %d, colCount: %d\n", graphColumn, colCount);

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
fprintf(stderr, "load Items: %d: %f\n", itemsLoaded, bg->dataValue);
    bg->graphUpperLimit = -1.0e+300; /* filled in by DrawItems	*/
    bg->graphLowerLimit = 1.0e+300; /* filled in by DrawItems	*/
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

/*	DrawItems has grown too large.  It has several distinct sections
 *	in it that should now be broken out into their own routines.
 */
static void bedGraphDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct vGfx *vg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
struct bedGraphItem *wi;
double pixelsPerBase = scaleForPixels(width);
double basesPerPixel = 1.0;
Color drawColor = vgFindColorIx(vg, 0, 0, 0);
int itemCount = 0;
struct bedGraphCartOptions *bedGraphCart;
enum wiggleGridOptEnum horizontalGrid;
enum wiggleGraphOptEnum lineBar;
enum wiggleScaleOptEnum autoScale;
enum wiggleWindowingEnum windowingFunction;
enum wiggleYLineMarkEnum yLineOnOff;
double yLineMark;
Color black = vgFindColorIx(vg, 0, 0, 0);
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
bedGraphCart = (struct bedGraphCartOptions *) tg->extraUiData;
if(sameString(tg->mapName, "affyTranscription"))
    bedGraphCart->colorTrack = "affyTransfrags";

horizontalGrid = bedGraphCart->horizontalGrid;
lineBar = bedGraphCart->lineBar;
autoScale = bedGraphCart->autoScale;
windowingFunction = bedGraphCart->windowingFunction;
yLineOnOff = bedGraphCart->yLineOnOff;
yLineMark = bedGraphCart->yLineMark;

if (pixelsPerBase > 0.0)
    basesPerPixel = 1.0 / pixelsPerBase;

/*	width - width of drawing window in pixels
 *	pixelsPerBase - pixels per base
 *	basesPerPixel - calculated as 1.0/pixelsPerBase
 */
itemCount = 0;
/*	we are going to keep an array that is three times the size of
 *	the screen to allow a screen full on either side of the visible
 *	region.  Those side screens can be used in the smoothing
 *	operation so there won't be any discontinuity at the visible screen
 *	boundaries.
 */
preDrawSize = width * 3;
preDraw = (struct preDrawElement *) needMem ((size_t)
		preDrawSize * sizeof(struct preDrawElement));
preDrawZero = width / 3;
for (i = 0; i < preDrawSize; ++i) {
	preDraw[i].count = 0;
	preDraw[i].max = -1.0e+300;
	preDraw[i].min = 1.0e+300;
}

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

/*	Determine the raw plotting value	*/
for (i = 0; i < preDrawSize; ++i)
    {
    double dataValue;
    if (preDraw[i].count)
	{
    switch (windowingFunction)
	{
	case (wiggleWindowingMin):
		if (fabs(preDraw[i].min)
				< fabs(preDraw[i].max))
		    dataValue = preDraw[i].min;
		else 
		    dataValue = preDraw[i].max;
		break;
	case (wiggleWindowingMean):
		dataValue =
		    preDraw[i].sumData / preDraw[i].count;
		break;
	default:
	case (wiggleWindowingMax):
		if (fabs(preDraw[i].min)
			> fabs(preDraw[i].max))
		    dataValue = preDraw[i].min;
		else 
		    dataValue = preDraw[i].max;
		break;
	}
	preDraw[i].plotValue = dataValue;
	preDraw[i].smooth = dataValue;
	}
    }


/*	Are we perhaps doing smoothing ?  smoothingWindow is 1 off due
 *	to enum funny business in inc/hui.h and lib/hui.c 	*/
if (bedGraphCart->smoothingWindow > 0)
    {
    int winSize = bedGraphCart->smoothingWindow + 1; /* enum funny business */
    int winBegin = 0;
    int winMiddle = -(winSize/2);
    int winEnd = -winSize;
    double sum = 0.0;
    unsigned long long points = 0LL;

    for (winBegin = 0; winBegin < preDrawSize; ++winBegin)
	{
	if (winEnd >=0)
	    {
	    if (preDraw[winEnd].count)
		{
		points -= preDraw[winEnd].count;
		sum -= preDraw[winEnd].plotValue * preDraw[winEnd].count;
		}
	    }
	if (preDraw[winBegin].count)
	    {
	    points += preDraw[winBegin].count;
	    sum += preDraw[winBegin].plotValue * preDraw[winBegin].count;
	    }
	if ((winMiddle >= 0) && points && preDraw[winMiddle].count)
		preDraw[winMiddle].smooth = sum / points;
	++winEnd;
	++winMiddle;
	}
    }

for (i = preDrawZero; i < preDrawZero+width; ++i)
    {
    /*	count is non-zero meaning valid data exists here	*/
    if (preDraw[i].count)
	{
	if (preDraw[i].max > overallUpperLimit)
	    overallUpperLimit = preDraw[i].max;
	if (preDraw[i].min < overallLowerLimit)
	    overallLowerLimit = preDraw[i].min;
	}
    }
overallRange = overallUpperLimit - overallLowerLimit;

if (autoScale == wiggleScaleAuto)
    {
    overallUpperLimit = -1.0e+300;	/* reset limits for auto scale */
    overallLowerLimit = 1.0e+300;
    for (i = preDrawZero; i < preDrawZero+width; ++i)
	{
	/*	count is non-zero meaning valid data exists here	*/
	if (preDraw[i].count)
	    {
	    if (preDraw[i].smooth > overallUpperLimit)
		overallUpperLimit = preDraw[i].smooth;
	    if (preDraw[i].smooth < overallLowerLimit)
		overallLowerLimit = preDraw[i].smooth;
	    }
	}
    overallRange = overallUpperLimit - overallLowerLimit;
    if (overallRange == 0.0)
	{
	if (overallUpperLimit > 0.0)
	    {
	    graphUpperLimit = overallUpperLimit;
	    graphLowerLimit = 0.0;
	    } else if (overallUpperLimit < 0.0) {
	    graphUpperLimit = 0.0;
	    graphLowerLimit = overallUpperLimit;
	    } else {
	    graphUpperLimit = 1.0;
	    graphLowerLimit = -1.0;
	    }
	    graphRange = graphUpperLimit - graphLowerLimit;
	} else {
	graphUpperLimit = overallUpperLimit;
	graphLowerLimit = overallLowerLimit;
	}
    } else {
	graphUpperLimit = bedGraphCart->maxY;
	graphLowerLimit = bedGraphCart->minY;
    }
graphRange = graphUpperLimit - graphLowerLimit;
epsilon = graphRange / tg->lineHeight;

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
/*	Set up the color by array. Determine color of each pixel
 *	based initially on the sign of the data point. If a colorTrack
 *	is specified also fill in the color array with that. 
 */
AllocArray(colorArray, width);
for(x1 = 0; x1 < width; ++x1)
    {
    int preDrawIndex = x1 + preDrawZero;
    if (preDraw[preDrawIndex].count)
	{
	double dataValue;	/*	the data value in data space	*/
	dataValue = preDraw[preDrawIndex].smooth;
	/*	negative data is the alternate color	*/
	if (dataValue < 0.0)
	    colorArray[x1] = tg->ixAltColor;
	else
	    colorArray[x1] = tg->ixColor;
	}
    }

/* Fill in colors from alternate track if necessary. */
if(bedGraphCart->colorTrack != NULL) 
    {
    struct track *cTrack = hashMustFindVal(trackHash, bedGraphCart->colorTrack);
    bedGraphFillInColorArray(tg, vg, colorArray, width, cTrack);
    }

/*	right now this is a simple pixel by pixel loop.  Future
 *	enhancements will smooth this data and draw boxes where pixels
 *	are all the same height in a run.
 */
for (x1 = 0; x1 < width; ++x1)
    {
    int preDrawIndex = x1 + preDrawZero;
    /*	count is non-zero meaning valid data exists here	*/
    if (preDraw[preDrawIndex].count)
	{
	int h = tg->lineHeight;	/*	the height of our drawing window */
	int boxHeight;		/*	the size of our box to draw	*/
	int boxTop;		/*	box top starts here	*/
	int y1;			/*	y coordinate of data point */
	int y0;			/*	y coordinate of data = 0.0 */
	int yPointGraph;	/*	y coordinate of data for point style */
	double dataValue;	/*	the data value in data space	*/

	/*	The graphing coordinate conversion situation is:
	 *	graph coordinate y = 0 is graphUpperLimit data space
	 *	and total graph height is h which is graphRange in data space
	 *	The Y axis is positive down, negative up.
	 *
	 *	Taking a simple coordinate conversion from data space
	 *	to the graphing space, the data value is at:
	 *	h * ((graphUpperLimit - dataValue)/graphRange)
	 *	and a data value zero line is at:
	 *	h * (graphUpperLimit/graphRange)
	 *	These may end up to be negative meaning they are above
	 *	the upper graphing limit, or be very large, meaning they
	 *	are below the lower graphing limit.  This is OK, the
	 *	clipping will be taken care of by the vgBox() function.
	 */

	/*	data value has been picked by previous scanning.
	 *	Could be smoothed, maybe not.
	 */
	dataValue = preDraw[preDrawIndex].smooth;

	y1 = h * ((graphUpperLimit - dataValue)/graphRange);
	yPointGraph = yOff + y1 - 1;
	y0 = h * ((graphUpperLimit)/graphRange);
	boxHeight = abs(y1 - y0);
	boxTop = min(y1,y0);
	/*	special case where dataValue is on the zero line, it
 	 *	needs to have a boxHeight of 1, otherwise it disappears into
 	 *	zero nothingness
	 */
	if (fabs(dataValue) < epsilon)
	    {
	    boxHeight = 1;
	    }
	/*	Last pixel (bottom) is a special case of a closed interval */
	if ((boxTop == h) && (boxHeight == 0))
	    {
	    boxTop = h - 1;
	    boxHeight = 1;
	    }
	/*	Special case data value on upper limit line	*/
	if ((boxTop+boxHeight) == 0)
	    boxHeight += 1;
	/*	Special case data value is below the lower view limit,
 	 *	should still show a 1 pixel wide line at the bottom */
	if ((graphLowerLimit >= 0.0) && (dataValue < graphLowerLimit))
	    {
	    boxTop = h - 1;
	    boxHeight = 1;
	    }
	/*	Special case data value is above the upper view limit,
 	 *	should still show a 1 pixel wide line at the top */
	if ((graphUpperLimit <= 0.0) && (dataValue > graphUpperLimit))
	    {
	    boxTop = 0;
	    boxHeight = 1;
	    }


	drawColor = colorArray[x1];
/* 	/\*	negative data is the alternate color	*\/ */
/* 	if (dataValue < 0.0) */
/* 	    drawColor = tg->ixAltColor; */
/* 	else */
/* 	    drawColor = tg->ixColor; */

	/*	vgBox will take care of clipping.  No need to worry
	 *	about coordinates or height of line to draw.
	 *	We are actually drawing single pixel wide lines here.
	 */
	if (vis == tvFull)
	    {
	    if (lineBar == wiggleGraphBar)
		{
		vgBox(vg, x1+xOff, yOff+boxTop, 1, boxHeight, drawColor);
		}
	    else
		{	/*	draw a 3 pixel height box	*/
		vgBox(vg, x1+xOff, yPointGraph, 1, 3, drawColor);
		}
	    }	/*	vis == tvFull	*/
	else if (vis == tvDense)
	    {
	    double dataValue;
	    int grayIndex;
	    dataValue = preDraw[preDrawIndex].smooth - graphLowerLimit;
	    grayIndex = (dataValue/graphRange) * MAX_WIG_VALUE;

	    drawColor =
		tg->colorShades[grayInRange(grayIndex, 0, MAX_WIG_VALUE)];

	    boxHeight = tg->lineHeight;
	    vgBox(vg, x1+xOff, yOff, 1,
		boxHeight, drawColor);
	    }	/*	vis == tvDense	*/
	}	/*	if (preDraw[].count)	*/
    }	/*	for (x1 = 0; x1 < width; ++x1)	*/

/*	Do we need to draw a zero line ?
 *	This is to be generalized in the future to allow horizontal grid
 *	lines, perhaps user specified to indicate thresholds.
 */
if ((vis == tvFull) && (horizontalGrid == wiggleHorizontalGridOn))
    {
    int x1, x2, y1, y2;

    x1 = xOff;
    x2 = x1 + width;

    /*	Let's see if the zero line can be drawn	*/
    if ((0.0 <= graphUpperLimit) && (0.0 >= graphLowerLimit))
	{
	int zeroOffset;
	drawColor = vgFindColorIx(vg, 0, 0, 0);
	zeroOffset = (int)((graphUpperLimit * tg->lineHeight) /
			(graphUpperLimit - graphLowerLimit));
	y1 = yOff + zeroOffset;
	if (y1 >= (yOff + tg->lineHeight)) y1 = yOff + tg->lineHeight - 1;
	y2 = y1;
	vgLine(vg,x1,y1,x2,y2,black);
	}

    }	/*	drawing horizontalGrid	*/

/*	Optionally, a user requested Y marker line at some value */
if ((vis == tvFull) && (yLineOnOff == wiggleYLineMarkOn))
    {
    int x1, x2, y1, y2;

    x1 = xOff;
    x2 = x1 + width;

    /*	Let's see if this marker line can be drawn	*/
    if ((yLineMark <= graphUpperLimit) && (yLineMark >= graphLowerLimit))
	{
	int Offset;
	drawColor = vgFindColorIx(vg, 0, 0, 0);
	Offset = tg->lineHeight * ((graphUpperLimit - yLineMark)/graphRange);
	y1 = yOff + Offset;
	if (y1 >= (yOff + tg->lineHeight)) y1 = yOff + tg->lineHeight - 1;
	y2 = y1;
	vgLine(vg,x1,y1,x2,y2,black);
	}

    }	/*	drawing y= line marker	*/

/*	Map this wiggle area if we are self mapping	*/
if (tg->mapsSelf)
    {
    char *itemName;
    if (tg->customPt)
	{
	struct customTrack *ct = tg->customPt;
	itemName = (char *)needMem(128 * sizeof(char));
	safef(itemName, 128, "%s %s", ct->wigFile, tg->mapName);
	}
    else
	itemName = cloneString(tg->mapName);

    mapBoxHc(seqStart, seqEnd, xOff, yOff, width, tg->height, tg->mapName, 
            itemName, NULL);
    freeMem(itemName);
    }

freez(&colorArray);
freeMem(preDraw);
}	/*	bedGraphDrawItems()	*/

static void bedGraphLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct vGfx *vg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
{
struct bedGraphItem *wi;
int fontHeight = tl.fontHeight+1;
int centerOffset = 0;
double lines[2];	/*	lines to label	*/
int numberOfLines = 1;	/*	at least one: 0.0	*/
int i;			/*	loop counter	*/
struct bedGraphCartOptions *bedGraphCart;

bedGraphCart = (struct bedGraphCartOptions *) tg->extraUiData;
lines[0] = 0.0;
lines[1] = bedGraphCart->yLineMark;
if (bedGraphCart->yLineOnOff == wiggleYLineMarkOn)
    ++numberOfLines;

if (withCenterLabels)
	centerOffset = fontHeight;

/*	We only do Dense and Full	*/
if (tg->visibility == tvDense)
    {
    vgTextRight(vg, xOff, yOff+centerOffset, width - 1, height-centerOffset,
	tg->ixColor, font, tg->shortLabel);
    }
else if (tg->visibility == tvFull)
    {
    int centerLabel = (height/2)-(fontHeight/2);
    int labelWidth = 0;

    /* track label is centered in the whole region */
    vgText(vg, xOff, yOff+centerLabel, tg->ixColor, font, tg->shortLabel);
    labelWidth = mgFontStringWidth(font,tg->shortLabel);
    /*	Is there room left to draw the min, max ?	*/
    if (height >= (3 * fontHeight))
	{
	boolean zeroOK = TRUE;
	double graphUpperLimit = -1.0e+300;
	double graphLowerLimit = 1.0e+300;
	char upper[128];
	char lower[128];
	char upperTic = '-';	/* as close as we can get with ASCII */
			/* the ideal here would be to draw tic marks in
 			 * exactly the correct location.
			 */
	Color drawColor;
	if (withCenterLabels)
	    {
	    centerOffset = fontHeight;
	    upperTic = '_';	/*	this is correct	*/
	    }
	for (wi = tg->items; wi != NULL; wi = wi->next)
	    {
	    if (wi->graphUpperLimit > graphUpperLimit)
		graphUpperLimit = wi->graphUpperLimit;
	    if (wi->graphLowerLimit < graphLowerLimit)
		graphLowerLimit = wi->graphLowerLimit;
	    }
	/*  In areas where there is no data, these limits do not change */
	if (graphUpperLimit < graphLowerLimit)
	    {
	    double d = graphLowerLimit;
	    graphLowerLimit = graphUpperLimit;
	    graphUpperLimit = d;
	    snprintf(upper, 128, "No data %c", upperTic);
	    snprintf(lower, 128, "No data _");
	    zeroOK = FALSE;
	    }
	else
	    {
	    snprintf(upper, 128, "%g %c", graphUpperLimit, upperTic);
	    snprintf(lower, 128, "%g _", graphLowerLimit);
	    }
	drawColor = tg->ixColor;
	if (graphUpperLimit < 0.0) drawColor = tg->ixAltColor;
	vgTextRight(vg, xOff, yOff, width - 1, fontHeight, drawColor,
	    font, upper);
	drawColor = tg->ixColor;
	if (graphLowerLimit < 0.0) drawColor = tg->ixAltColor;
	vgTextRight(vg, xOff, yOff+height-fontHeight, width - 1, fontHeight,
	    drawColor, font, lower);

	for (i = 0; i < numberOfLines; ++i )
	    {
	    double lineValue = lines[i];
	    /*	Maybe zero can be displayed */
	    /*	It may overwrite the track label ...	*/
	    if (zeroOK && (lineValue < graphUpperLimit) &&
		(lineValue > graphLowerLimit))
		{
		int offset;
		int Width;

		drawColor = vgFindColorIx(vg, 0, 0, 0);
		offset = centerOffset +
		    (int)(((graphUpperLimit - lineValue) *
				(height - centerOffset)) /
			(graphUpperLimit - graphLowerLimit));
		/*	reusing the lower string here	*/
		if (i == 0)
		    snprintf(lower, 128, "0 -");
		else
		    snprintf(lower, 128, "%g -", lineValue);
		/*	only draw if it is far enough away from the
		 *	upper and lower labels, and it won't overlap with
		 *	the center label.
		 */
		Width = mgFontStringWidth(font,lower);
		if ( !( (offset < centerLabel+fontHeight) &&
		    (offset > centerLabel-(fontHeight/2)) &&
		    (Width+labelWidth >= width) ) &&
		    (offset > (fontHeight*2)) &&
		    (offset < height-(fontHeight*2)) )
		    {
		    vgTextRight(vg, xOff, yOff+offset-(fontHeight/2),
			width - 1, fontHeight, drawColor, font, lower);
		    }
		}	/*	drawing a zero label	*/
	    }	/*	drawing 0.0 and perhaps yLineMark	*/
	}	/* if (height >= (3 * fontHeight))	*/
    }	/*	if (tg->visibility == tvFull)	*/
}	/* bedGraphLeftLabels */

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
struct bedGraphCartOptions *bedGraphCart;
int maxHeight = atoi(DEFAULT_HEIGHT_PER);
int minHeight = MIN_HEIGHT_PER;

AllocVar(bedGraphCart);

/*	These Fetch functions look for variables in the cart bounded by
 *	limits specified in trackDb or returning defaults
 */
bedGraphCart->lineBar = wigFetchGraphType(tdb, (char **) NULL);
bedGraphCart->horizontalGrid = wigFetchHorizontalGrid(tdb, (char **) NULL);

bedGraphCart->autoScale = wigFetchAutoScale(tdb, (char **) NULL);
bedGraphCart->windowingFunction = wigFetchWindowingFunction(tdb, (char **) NULL);
bedGraphCart->smoothingWindow = wigFetchSmoothingWindow(tdb, (char **) NULL);

wigFetchMinMaxPixels(tdb, &minHeight, &maxHeight, &defaultHeight);
wigFetchYLineMarkValue(tdb, &yLineMark);
bedGraphCart->yLineMark = yLineMark;
bedGraphCart->yLineOnOff = wigFetchYLineMark(tdb, (char **) NULL);

bedGraphCart->maxHeight = maxHeight;
bedGraphCart->defaultHeight = defaultHeight;
bedGraphCart->minHeight = minHeight;

/*wigFetchMinMaxY(tdb, &minY, &maxY, &tDbMinY, &tDbMaxY, wordCount, words);*/
wigFetchMinMaxLimits(tdb, &minY, &maxY, (double *)NULL, (double *)NULL);

switch (wordCount)
    {
	case 2:
	    bedGraphCart->graphColumn = atoi(words[1]);
	    /*	protect against nonsense values	*/
	    if ( (bedGraphCart->graphColumn < 2) ||
		(bedGraphCart->graphColumn > 100) )
		bedGraphCart->graphColumn = 5; /* default score column */

	    break;
	default:
	    bedGraphCart->graphColumn = 5; /* default score column */
	    break;
    } 

track->minRange = minY;
track->maxRange = maxY;

bedGraphCart->minY = track->minRange;
bedGraphCart->maxY = track->maxRange;

track->loadItems = bedGraphLoadItems;
track->freeItems = bedGraphFreeItems;
track->drawItems = bedGraphDrawItems;
track->itemName = bedGraphName;
track->mapItemName = bedGraphName;
track->totalHeight = bedGraphTotalHeight;
track->itemHeight = tgFixedItemHeight;

track->itemStart = tgItemNoStart;
track->itemEnd = tgItemNoEnd;
track->mapsSelf = TRUE;
track->extraUiData = (void *) bedGraphCart;
track->colorShades = shadesOfGray;
track->drawLeftLabels = bedGraphLeftLabels;
/*	the lfSubSample type makes the image map function correctly */
track->subType = lfSubSample;     /*make subType be "sample" (=2)*/

}	/*	bedGraphMethods()	*/
