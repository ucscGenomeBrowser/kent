/* wigTrack - stuff to handle loading and display of
 * wig type tracks in browser. Wigs are arbitrary data graphs
 */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "wiggle.h"
#include "scoredRef.h"

static char const rcsid[] = "$Id: wigTrack.c,v 1.21 2003/11/10 20:39:35 hiram Exp $";

/*	wigCartOptions structure - to carry cart options from wigMethods
 *	to all the other methods via the track->extraUiData pointer
 */
struct wigCartOptions
    {
    enum wiggleOptEnum wiggleType;
    boolean zoomCompression;	/*  true - do max() averaging over the bin
    				 *  false - simple pick one of the
				 *  points in the bin.
				 */
    enum wiggleGridOptEnum horizontalGrid;	/*  grid lines, ON/OFF */
    enum wiggleGraphOptEnum lineBar;		/*  Line or Bar chart */
    enum wiggleScaleOptEnum autoScale;		/*  autoScale on */
    double minY;	/*	from trackDb.ra words, the absolute minimum */
    double maxY;	/*	from trackDb.ra words, the absolute maximum */
    int heightFromCart;	/*	requested height from cart	*/
    };

struct preDrawElement
    {
	double	max;	/*	maximum value seen for this point	*/
	double	min;	/*	minimum value seen for this point	*/
	unsigned long long	count;	/* number of datum at this point */
	double	sumData;	/*	sum of all values at this point	*/
	double  sumSquares;	/* sum of (values squared) at this point */
    };

struct wigItem
/* A wig track item. */
    {
    struct wigItem *next;
    int start, end;	/* Start/end in chrom (aka browser) coordinates. */
    int grayIx;         /* Level of gray usually. */
    char *name;		/* Common name */
    char *db;		/* Database */
    int orientation;	/* Orientation */
    int ix;		/* Position in list. */
    int height;		/* Pixel height of item. */
    unsigned Max;      /* Maximum score in this block [0:127] */
    unsigned Min;      /* Minimum score in this block [0:126] */
    unsigned Span;      /* each value spans this many bases */
    unsigned Count;     /* number of values to use */
    unsigned Offset;    /* offset in File to fetch data */
    char *File; /* path name to data file, one byte per value */
    double lowerLimit;  /* lowest data value in this block */
    double dataRange;   /* lowerLimit + dataRange = upperLimit */
    unsigned validCount;        /* number of valid data values in this block */
    double sumData;     /* sum of the data points, for average and stddev calc */
    double sumSquares;      /* sum of data points squared, for stddev calc */
    };

static void wigItemFree(struct wigItem **pEl)
    /* Free up a wigItem. */
{
struct wigItem *el = *pEl;
if (el != NULL)
    {
    freeMem(el->name);
    freeMem(el->db);
    freeMem(el->File);
    freez(pEl);
    }
}

void wigItemFreeList(struct wigItem **pList)
    /* Free a list of dynamically allocated wigItem's */
{
struct wigItem *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    wigItemFree(&el);
    }
*pList = NULL;
}

/*	trackSpans - hash of hashes, first hash is via trackName
 *	the element for trackName is a hash itself where each element
 *	is a Span found in the data (==zoom level indication)
 */
static struct hash *trackSpans = NULL;	/* hash of hashes */

#if ! defined(DEBUG)
/****           some simple debug output during development	*/
static char dbgFile[] = "/tmp/wig.dbg";
static boolean debugOpened = FALSE;
static FILE * dF;

static void wigDebugOpen(char * name) {
if (debugOpened) return;
dF = fopen( dbgFile, "w");
fprintf( dF, "opened by %s\n", name);
chmod(dbgFile, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP  | S_IROTH | S_IWOTH | S_IXOTH);
debugOpened = TRUE;
}

#define DBGMSGSZ	1023
char dbgMsg[DBGMSGSZ+1];
void wigDebugPrint(char * name) {
wigDebugOpen(name);
if (debugOpened)
    {
    if (dbgMsg[0])
	fprintf( dF, "%s: %s\n", name, dbgMsg);
    else
	fprintf( dF, "%s:\n", name);
    }
    dbgMsg[0] = (char) NULL;
    fflush(dF);
}
#endif

/*	The item names have been massaged during the Load.  An
 *	individual item may have been read in on multiple table rows and
 *	had an extension on it to make it unique from the others.  Also,
 *	each different zoom level had a different extension name.
 *	All these names were condensed into the root of the name with
 *	the extensions removed.
 */
static char *wigName(struct track *tg, void *item)
/* Return name of wig level track. */
{
struct wigItem *wi = item;
return wi->name;
}

/*	This is practically identical to sampleUpdateY in sampleTracks.c
 *	In fact is is functionally identical except jkLib functions are
 *	used instead of the actual string functions.  I will consult
 *	with Ryan to see if one of these copies can be removed.
 */
boolean sameWigGroup(char *name, char *nextName, int lineHeight)
/* Only increment height when name root (extension removed)
 * is different from previous one.  Assumes entries are sorted by name.
 */
{
int different = 0;
char *s0;
char *s1;
s0 = cloneString(name);
s1 = cloneString(nextName);
chopSuffix(s0);
chopSuffix(s1);
different = differentString(s0,s1);
freeMem(s0);
freeMem(s1);
if (different)
    return lineHeight;
else
    return 0;
}

static int wigTotalHeight(struct track *tg, enum trackVisibility vis)
/* Wiggle track will use this to figure out the height they use
   as defined in the cart */
{
struct wigItem *item;
char *heightPer;
int heightFromCart;
struct wigCartOptions *wigCart;
char o1[128];
int itemCount = 1;

wigCart = (struct wigCartOptions *) tg->extraUiData;

/*
 *	A track is just one
 *	item, so there is nothing to do here, either it is the tvFull
 *	height as chosen by the user from TrackUi, or it is the dense
 *	mode.
 */
if (vis == tvDense)
    tg->lineHeight = tl.fontHeight+1;
else if (vis == tvFull)
    tg->lineHeight = max(tl.fontHeight + 1, wigCart->heightFromCart);

tg->heightPer = tg->lineHeight;
tg->height = tg->lineHeight;

return tg->height;
}

/*	wigLoadItems - read the table rows that hRangeQuery returns
 *	With appropriate adjustment to help hRangeQuery limit its
 *	result to specific "Span" based on the basesPerPixel.
 *	From the rows returned, turn each one into a wigItem, add it to
 *	the growing wiList, and return that wiList as the tg->items.
 *
 *	To help DrawItems, we are going to make up a list of Spans that
 *	were read in for this track.  DrawItems can use that list to
 *	limit itself to the appropriate span.  (Even though we tried to
 *	use Span to limit the hRangeQuery, there may be other Spans in
 *	the result that are not needed during the drawing.)
 *	This list of spans is actually a hash of hashes.
 *	The first level is a hash of track names from each call to
 *	wigLoadItems.  The second level chosen from the track name is
 *	the actual hash of Spans for this particular track.
 *
 *	With 1K zoom Spans available, no more than approximately 1024
 *	rows will need to be loaded at any one time.
 */
static void wigLoadItems(struct track *tg) {
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct wiggle *wiggle;
struct wigItem *wiList = NULL;
char *whereNULL = NULL;
int itemsLoaded = 0;
char spanName[128];
struct hashEl *el, *elList;
struct hashEl *el2, *elList2;
struct hash *spans = NULL;	/* Spans encountered during load */
/*	Check our scale from the global variables that exist in
 *	hgTracks.c - This can give us a guide about which rows to load.
 *	If the scale is more than 1K bases per pixel, we can try loading
 *	only those rows with Span == 1024 to see if an appropriate zoom
 *	level exists.
 */
int basesPerPixel = (int)((double)(winEnd - winStart)/(double)insideWidth);
char *span1K = "Span = 1024 limit 1";
char *spanOver1K = "Span >= 1024";

if (basesPerPixel >= 1024) {
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
	span1K, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
	++itemsLoaded;
}
/*	If that worked, excellent, then we have at least another zoom level
 *	So, for our actual load, use spanOver1K to fetch not only the 1K
 *	zooms, but potentially others that may be useful.  This will
 *	save us a huge amount in loaded rows.  On a 250 Mbase chromosome
 *	there would be 256,000 rows at the 1 base level and only
 *	256 rows at the 1K zoom level.  Otherwise, we go back to the
 *	regular query which will give us all rows.
 */
if (itemsLoaded)
    {
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
	spanOver1K, &rowOffset);
    }
else
    {
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
	whereNULL, &rowOffset);
    }

/*	Allocate trackSpans one time only	*/
if (! trackSpans)
    trackSpans = newHash(0);
/*	Each instance of this LoadItems will create a new spans hash
 *	It will be the value included in the trackSpans hash
 */
spans = newHash(0);
/*	Each row read will be turned into an instance of a wigItem
 *	A growing list of wigItems will be the items list to return
 */
itemsLoaded = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
	struct wigItem *wi;
	size_t fileNameSize = 0;
	char *File = (char *) NULL;

	++itemsLoaded;
	wiggle = wiggleLoad(row + rowOffset);
	AllocVar(wi);
	wi->start = wiggle->chromStart;
	wi->end = wiggle->chromEnd;
	wi->grayIx = grayInRange(wiggle->score, 0, 127);
	wi->name = cloneString(tg->mapName);
	wi->orientation = orientFromChar(wiggle->strand[0]);
	fileNameSize = strlen("/gbdb//wib/") + strlen(database)
	    + strlen(wiggle->File) + 1;
	File = (char *) needMem((size_t)fileNameSize);
	snprintf(File, fileNameSize, "/gbdb/%s/wib/%s", database, wiggle->File);

	if (! fileExists(File))
	    errAbort("wigLoadItems: file '%s' missing", File);
	wi->File = cloneString(File);
	freeMem(File);

	wi->Max = wiggle->score;
	wi->Min = wiggle->Min;
	wi->Span = wiggle->Span;
	wi->Count = wiggle->Count;
	wi->Offset = wiggle->Offset;
	wi->lowerLimit = wiggle->lowerLimit;
	wi->dataRange = wiggle->dataRange;
	wi->validCount = wiggle->validCount;
	wi->sumData = wiggle->sumData;
	wi->sumSquares = wiggle->sumSquares;

	el = hashLookup(trackSpans, wi->name);
	if ( el == NULL)
	    	hashAdd(trackSpans, wi->name, spans);
	snprintf(spanName, sizeof(spanName), "%d", wi->Span);
	el = hashLookup(spans, spanName);
	if ( el == NULL)
	    	hashAddInt(spans, spanName, wi->Span);
	slAddHead(&wiList, wi);
	wiggleFree(&wiggle);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);

slReverse(&wiList);
tg->items = wiList;
}	/*	wigLoadItems()	*/

static void wigFreeItems(struct track *tg) {
#if defined(DEBUG)
snprintf(dbgMsg, DBGMSGSZ, "I haven't seen wigFreeItems ever called ?");
wigDebugPrint("wigFreeItems");
#endif
}

static void wigDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct vGfx *vg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
struct wigItem *wi;
double pixelsPerBase = scaleForPixels(width);
double basesPerPixel = 1.0;
struct rgbColor *normal = &(tg->color);
Color drawColor = vgFindColorIx(vg, 0, 0, 0);
int itemCount = 0;
char *currentFile = (char *) NULL;	/*	the binary file name */
FILE *f = (FILE *) NULL;		/*	file handle to binary file */
struct hashEl *el, *elList;
char cartStr[64];	/*	to set cart strings	*/
struct wigCartOptions *wigCart;
enum wiggleOptEnum wiggleType;
enum wiggleGridOptEnum horizontalGrid;
enum wiggleGraphOptEnum lineBar;
enum wiggleScaleOptEnum autoScale;
Color shadesOfPrimary[EXPR_DATA_SHADES];
Color shadesOfAlt[EXPR_DATA_SHADES];
Color black = vgFindColorIx(vg, 0, 0, 0);
struct rgbColor blackColor = {0, 0, 0};
struct rgbColor whiteColor = {255, 255, 255};
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
int x1 = 0;			/*	screen coordinates	*/
int x2 = 0;			/*	screen coordinates	*/

/*
vgMakeColorGradient(vg, &blackColor, &tg->color, 4, shadesOfPrimary);
vgMakeColorGradient(vg, &blackColor, &tg->altColor, 4, shadesOfAlt);
*/
vgMakeColorGradient(vg, &whiteColor, &blackColor, 12, shadesOfPrimary);
vgMakeColorGradient(vg, &whiteColor, &blackColor, 12, shadesOfAlt);

wigCart = (struct wigCartOptions *) tg->extraUiData;
wiggleType = wigCart->wiggleType;
horizontalGrid = wigCart->horizontalGrid;
lineBar = wigCart->lineBar;
autoScale = wigCart->autoScale;

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
		preDrawSize * sizeof(struct preDrawElement) );
preDrawZero = width / 3;
for (i = 0; i < preDrawSize; ++i) {
	preDraw[i].count = 0;
	preDraw[i].max = -1.0e+300;
	preDraw[i].min = 1.0e+300;
}

snprintf(dbgMsg, DBGMSGSZ, "going into pre-draw prep");
wigDebugPrint("wigDrawItems");

/*	walk through all the data and prepare the preDraw array	*/
for (wi = tg->items; wi != NULL; wi = wi->next)
    {
    unsigned char *ReadData;	/* the bytes read in from the file */
    int dataOffset = 0;		/*	within data block during drawing */
    int usingDataSpan = 1;		/* will become larger if possible */
    int minimalSpan = 100000000;	/*	a lower limit safety check */

    ++itemCount;
    /*	Take a look through the potential spans, and given what we have
     *	here for basesPerPixel, pick the largest usingDataSpan that is
     *	not greater than the basesPerPixel
     */
    el = hashLookup(trackSpans, wi->name);	/*  What Spans do we have */
    elList = hashElListHash(el->val);		/* Our pointer to spans hash */
    for (el = elList; el != NULL; el = el->next)
	{
	int Span;
	Span = (int) el->val;
	if ((Span < basesPerPixel) && (Span > usingDataSpan))
	    usingDataSpan = Span;
	if (Span < minimalSpan)
	    minimalSpan = Span;
	}
    hashElFreeList(&elList);

    /*	There may not be a span of 1, use whatever is lowest	*/
    if (minimalSpan > usingDataSpan)
	usingDataSpan = minimalSpan;

    /*	Now that we know what Span to draw, see if this item should be
     *	drawn at all.
     */
    if (usingDataSpan == wi->Span)
	{
	/*	Check our data file, see if we need to open a new one */
	if (currentFile)
	    {
	    if (differentString(currentFile,wi->File))
		{
		if (f != (FILE *) NULL)
		    {
		    fclose(f);
		    freeMem(currentFile);
		    }
		currentFile = cloneString(wi->File);
		f = mustOpen(currentFile, "r");
		}
	    }
	else
	    {
	    currentFile = cloneString(wi->File);
	    f = mustOpen(currentFile, "r");
	    }
/*	Ready to draw, what do we know:
 *	the feature being processed:
 *	chrom coords:  [wi->start : wi-end)
 *
 *	The data to be drawn: to be read from file f at offset wi->Offset
 *	data points available: wi->Count, representing wi->Span bases
 *	for each data point
 *
 *	The drawing window, in pixels:
 *	xOff = left margin, yOff = top margin, h = height of drawing window
 *	drawing window in chrom coords: seqStart, seqEnd
 *	'basesPerPixel' is known, 'pixelsPerBase' is known
 */
	fseek(f, wi->Offset, SEEK_SET);
	ReadData = (unsigned char *) needMem((size_t) (wi->Count + 1));
	fread(ReadData, (size_t) wi->Count, (size_t) sizeof(unsigned char), f);

	/*	let's check end point screen coordinates.  If they are
 	 *	the same, then this entire data block lands on one pixel,
 	 *	no need to walk through it, just use the block's specified
 	 *	max/min.  It is OK if these end up + or -, we do want to
 	 *	keep track of pixels before and after the screen for
 	 *	later smoothing operations
	 */
	x1 = (wi->start - seqStart) * pixelsPerBase;
	x2 = ((wi->start+(wi->Count * usingDataSpan))-seqStart) * pixelsPerBase;

	if (x2 > x1) {
	    /*	walk through all the data in this block	*/
	    for (dataOffset = 0; dataOffset < wi->Count; ++dataOffset)
		{
		unsigned char datum = ReadData[dataOffset];
		if (datum != WIG_NO_DATA)
		    {
    x1 = ((wi->start-seqStart) + (dataOffset * usingDataSpan)) * pixelsPerBase;
		    x2 = x1 + (usingDataSpan * pixelsPerBase);
		    for (i = x1; i <= x2; ++i)
			{
			int xCoord = preDrawZero + i;
			if ((xCoord >= 0) && (xCoord < preDrawSize) )
			    {
			double dataValue =
	wi->lowerLimit+(((double)datum/(double)MAX_WIG_VALUE)*wi->dataRange);
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
		}
	} else {	/*	only one pixel for this block of data */
	    int xCoord = preDrawZero + x1;
	    /*	if the point falls within our array, record it.
	     *	the (wi->validCount > 0) is a safety check.  It
	     *	should always be true unless the data was
	     *	prepared incorrectly.
	     */
	    if ((wi->validCount > 0) && (xCoord >= 0) && (xCoord < preDrawSize) )
		{
		double upperLimit;
		preDraw[xCoord].count += wi->validCount;
		upperLimit = wi->lowerLimit + wi->dataRange;
		if (upperLimit > preDraw[xCoord].max)
		    preDraw[xCoord].max = upperLimit;
		if (wi->lowerLimit < preDraw[xCoord].min)
		    preDraw[xCoord].min = wi->lowerLimit;
		preDraw[xCoord].sumData += wi->sumData;
		preDraw[xCoord].sumSquares += wi->sumSquares;
snprintf(dbgMsg, DBGMSGSZ, "one whole block at item %d, with %d points", itemCount, wi->Count);
wigDebugPrint("wigDrawItems");
		}
	}
	freeMem(ReadData);
	}	/*	Draw if span is correct	*/
    }	/*	for (wi = tg->items; wi != NULL; wi = wi->next)	*/
if (f != (FILE *) NULL)
    {
    fclose(f);
    }
if (currentFile)
    freeMem(currentFile);

for (i = preDrawZero; i < preDrawZero+width; ++i)
    {
    if (preDraw[i].max > overallUpperLimit)
	overallUpperLimit = preDraw[i].max;
    if (preDraw[i].min < overallLowerLimit)
	overallLowerLimit = preDraw[i].min;
    }
overallRange = overallUpperLimit - overallLowerLimit;

snprintf(dbgMsg, DBGMSGSZ, "done preDraw, %d items, range: [%g:%g]", itemCount, overallLowerLimit, overallUpperLimit );
wigDebugPrint("wigDrawItems");

/*	now we are ready to draw.  Each element in the preDraw[] array
 *	cooresponds to a single pixel on the screen
 */

if (autoScale == wiggleScaleAuto)
    {
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
snprintf(dbgMsg, DBGMSGSZ, "autoScale requested");
    } else {
	graphUpperLimit = wigCart->maxY;
	graphLowerLimit = wigCart->minY;
snprintf(dbgMsg, DBGMSGSZ, "manualScale requested");
    }
graphRange = graphUpperLimit - graphLowerLimit;
wigDebugPrint("wigDrawItems");
snprintf(dbgMsg, DBGMSGSZ, "Scale: [%g:%g] range: %g", graphLowerLimit, graphUpperLimit, graphRange);
wigDebugPrint("wigDrawItems");

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
	int y1;			/*	special y coordinate for points draw */
	double dataValue;	/*	the data value in data space	*/

	/*	here is the auto-scaling in operation.  The size of the viewing
	 *	data window is (overallRange * 1.2) - 10% margin on top and
	 *	bottom.  The bottom view limit is
	 *	(overallLowerLimit - (overallRange * 0.1))
	 *	is from (overallLowerLimit - (overallRange * 0.1)) to
	 *	The upper limit, not needed here, would be
	 *	(overallLowerLimit + (overallRange * 1.2))
	 */
	/*	See if the zero line is between the limits
	 *	If it is, we need to decide which point to draw.
	 *	We want to draw the one that is farther from zero
	 *	(we could draw them both if there is more than one data
	 *	value here, show the high and the low)
	 */
	/*	THIS IS NOT DONE YET - IT WILL NOT DO negative values
 	 *	CORRECTLY !
	 */
	/*	Assume .max is the data value to draw,  if the .min is
 	 *	farther away from zero, then it is the one to draw
	 */

	dataValue = preDraw[preDrawIndex].max;
	if (fabs(0.0 - preDraw[preDrawIndex].min) >
		fabs(0.0 - preDraw[preDrawIndex].max) ) {
	    dataValue = preDraw[preDrawIndex].min;
	}
	drawColor = tg->ixColor;
	if (dataValue < 0.0) drawColor = tg->ixAltColor;
	boxHeight = h * ((dataValue - graphLowerLimit) / graphRange);
	if (boxHeight > h) boxHeight = h;
	if (boxHeight < 1) boxHeight = 1;
	boxTop = h - boxHeight;
	y1 = yOff+boxTop;
	if ( (0.0 > graphLowerLimit) && (0.0 < graphUpperLimit) )
	    {
		boxHeight = h * (fabs(dataValue) / graphRange);
		if (boxHeight > h) boxHeight = h;
		if (boxHeight < 1) boxHeight = 1;
		if (dataValue > 0.0) {
			boxTop =
	(h * (graphUpperLimit/ graphRange));
			boxTop -= boxHeight;
			y1 = yOff+boxTop;
		} else {
			boxTop = h -
			(h * (((0.0-graphLowerLimit) /
				graphRange)));
			y1 = yOff+boxTop + boxHeight;
		}
	    } else if (graphUpperLimit < 0.0)
	    {
		boxHeight= h * ((graphUpperLimit-dataValue) / graphRange);
		if (boxHeight > h) boxHeight = h;
		if (boxHeight < 1) boxHeight = 1;
		boxTop = 0;
		y1 = yOff+boxTop + boxHeight;
	    }
	    if (boxTop > h-1) boxTop = h - 1;
	    if (boxTop < 0) boxTop = 0;

/*	drawing region here is from upper left (x1,yOff)
 *	with width of pixelsPerDataValue
 *	and height of h  (! Y axis is 0 at top ! -> h is at bottom)
 *	So, within those constraints, lets figure out where we want to
 *	draw a box, given a 'dataValue' score in range [0:MAX_WIG_VALUE]
 *	and our data scale of range [tg->minRange:tg->maxRange]
 *	with upper data scale limit of wigCart->maxY
 *	and lower data scale limit of wigCart->minY
 *
		dY = (double) dataValue * dataScale;
		dataPoint = wigCart->minY + dY;

vgBox(v,x,y,width,height,color)

 */
	if (vis == tvFull)
	    {
	    if (lineBar == wiggleGraphBar)
		{
		vgBox(vg, x1+xOff, yOff+boxTop,
				1, boxHeight, drawColor);
		}
	    else
		{
		vgBox(vg, x1+xOff, y1-1, 1, 3, drawColor);
		}

	    }	/*	vis == tvFull	*/
	else if (vis == tvDense)
	    {
	    double dataValue;
	    int grayIndex;
	    dataValue = preDraw[preDrawIndex].max - graphLowerLimit;
	    grayIndex = (dataValue/graphRange) * MAX_WIG_VALUE;
			
	    drawColor =
shadesOfPrimary[grayInRange(grayIndex, 0, MAX_WIG_VALUE)];

	    boxHeight = tg->lineHeight;
	    vgBox(vg, x1+xOff, yOff, 1,
			    boxHeight, drawColor);
	    }	/*	vis == tvDense	*/
	}	/*	if (preDraw[].count)	*/
    }	/*	for (x1 = 0; x1 < width; ++x1)	*/

/*	Do we need to draw a horizontalGrid ?	*/
if ((vis == tvFull) && (horizontalGrid == wiggleHorizontalGridOn))
    {
    double yRange;
    int x1, x2, y1, y2;
    int gridLines = 2;
    int drewLines = 0;

    yRange = tg->maxRange - tg->minRange;
    x1 = xOff;
    x2 = x1 + width;
    if (tg->lineHeight > 64)
	gridLines = 4;
    else if (tg->lineHeight > 32)
	gridLines = 3;
    for (y1 = yOff; y1 <= tg->lineHeight+yOff;
		y1 += (int)((double)(tg->lineHeight-1)/(double)gridLines))
	{
	y2 = y1;
	++drewLines;
	vgLine(vg,x1,y1,x2,y2,black);
	}
    }	/*	drawing horizontalGrid	*/

freeMem(preDraw);
}	/*	wigDrawItems()	*/

/* Make track group for wig multiple alignment.
 *	WARNING ! - track->visibility is merely the default value
 *	from the trackDb entry at this time.  It will be set after this
 *	 by hgTracks from its cart UI setting.  When called in
 *	 TotalHeight it will then be the requested visibility.
 */
void wigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
{
char o1[128];	/*	Option 1 - track pixel height:  .heightPer	*/
char o2[128];	/*	Option 2 - interpolate or samples only	*/
char o4[128];	/*	Option 4 - minimum Y axis value: .minY	*/
char o5[128];	/*	Option 5 - maximum Y axis value: .minY	*/
char o7[128];	/*	Option 7 - horizontal grid lines: horizGrid */
char o8[128];	/*	Option 8 - type of graph, lineBar */
char o9[128];	/*	Option 9 - type of graph, autoScale */
char *interpolate = NULL;	/*	samples only, or interpolate */
char *horizontalGrid = NULL;	/*	Grid lines, ON/OFF - off default */
char *lineBar = NULL;	/*	Line or Bar chart - Bar by default */
char *autoScale = NULL;	/*	autoScale on/off - On by default */
char *minY_str = NULL;	/*	string from cart	*/
char *maxY_str = NULL;	/*	string from cart	*/
char *heightPer = NULL;	/*	string from cart	*/
int heightFromCart;	/*	truncated by limits	*/
double minYc;	/*	from cart */
double maxYc;	/*	from cart */
enum wiggleOptEnum wiggleType;
enum wiggleGridOptEnum wiggleHorizGrid;
enum wiggleGraphOptEnum wiggleLineBar;
enum wiggleScaleOptEnum wiggleAutoScale;
double minY;	/*	from trackDb.ra words, the absolute minimum */
double maxY;	/*	from trackDb.ra words, the absolute maximum */
char cartStr[64];	/*	to set cart strings	*/
struct wigCartOptions *wigCart;


AllocVar(wigCart);

/*	Start with an arbitrary min,max when not given in the .ra file	*/
minY = DEFAULT_MIN_Yv;
maxY = DEFAULT_MAX_Yv;

/*	Possibly fetch values from the trackDb.ra file	*/
if (wordCount > 1) minY = atof(words[1]);
if (wordCount > 2) maxY = atof(words[2]);
/*	Let's ensure their order is correct	*/
if (maxY < minY)
    {
    double d;
    d = maxY;
    maxY = minY;
    minY = d;
    }
/*	Allow DrawItems to see these from wigCart in addition
 *	to the track minRange,maxRange which is set below
 */

wigCart->minY = minY;
wigCart->maxY = maxY;

/*	Possibly fetch values from the cart	*/
snprintf( o1, sizeof(o1), "%s.heightPer", track->mapName);
snprintf( o2, sizeof(o2), "%s.linear.interp", track->mapName);
snprintf( o4, sizeof(o4), "%s.minY", track->mapName);
snprintf( o5, sizeof(o5), "%s.maxY", track->mapName);
snprintf( o7, sizeof(o7), "%s.horizGrid", track->mapName);
snprintf( o8, sizeof(o8), "%s.lineBar", track->mapName);
snprintf( o9, sizeof(o9), "%s.autoScale", track->mapName);
heightPer = cartOptionalString(cart, o1);
interpolate = cartOptionalString(cart, o2);
minY_str = cartOptionalString(cart, o4);
maxY_str = cartOptionalString(cart, o5);
horizontalGrid = cartOptionalString(cart, o7);
lineBar = cartOptionalString(cart, o8);
autoScale = cartOptionalString(cart, o9);

if (minY_str) minYc = atof(minY_str);
else minYc = minY;

if (maxY_str) maxYc = atof(maxY_str);
else maxYc = maxY;

/*	Clip the cart value to range [tl.fontHeight + 1:DEFAULT_HEIGHT_PER] */
if (heightPer) heightFromCart = min( DEFAULT_HEIGHT_PER, atoi(heightPer));
else heightFromCart = DEFAULT_HEIGHT_PER;
heightFromCart = max(tl.fontHeight + 1, heightFromCart);

wigCart->heightFromCart = heightFromCart;

/*	The values from trackDb.ra are the clipping boundaries, do
 *	not let cart settings go outside that range, and keep them
 *	in proper order.
 */
track->minRange = max( minY, minYc);
track->maxRange = min( maxY, maxYc);
wigCart->minY = track->minRange;
wigCart->maxY = track->maxRange;

if (track->maxRange < track->minRange)
    {
	double d;
	d = track->maxRange;
	track->maxRange = track->minRange;
	track->minRange = d;
    }

/*	If interpolate is a string, it came from the cart, otherwise set
 *	the default for this option and stuff it into the cart
 */
if (interpolate)
    wiggleType = wiggleStringToEnum(interpolate);
else 
    {
    wiggleType = wiggleStringToEnum("Linear Interpolation");
    snprintf( cartStr, sizeof(cartStr), "%s", "Linear Interpolation");
    cartSetString( cart, o2, cartStr);
    }
wigCart->wiggleType = wiggleType;

/*	If horizontalGrid is a string, it came from the cart, otherwise set
 *	the default for this option and stuff it into the cart
 */
if (horizontalGrid)
    wiggleHorizGrid = wiggleGridStringToEnum(horizontalGrid);
else 
    {
    wiggleHorizGrid = wiggleGridStringToEnum("OFF");
    snprintf( cartStr, sizeof(cartStr), "%s", "OFF");
    cartSetString( cart, o7, cartStr);
    }
wigCart->horizontalGrid = wiggleHorizGrid;

/*	If lineBar is a string, it came from the cart, otherwise set
 *	the default for this option and stuff it into the cart
 */
if (lineBar)
    wiggleLineBar = wiggleGraphStringToEnum(lineBar);
else 
    {
    wiggleLineBar = wiggleGraphStringToEnum("Bar");
    snprintf( cartStr, sizeof(cartStr), "%s", "Bar");
    cartSetString( cart, o8, cartStr);
    }
wigCart->lineBar = wiggleLineBar;

/*	If autoScale is a string, it came from the cart, otherwise set
 *	the default for this option and stuff it into the cart
 */
if (autoScale)
    wiggleAutoScale = wiggleScaleStringToEnum(autoScale);
else 
    {
    wiggleAutoScale = wiggleScaleStringToEnum("Auto-Scale to data view");
    snprintf( cartStr, sizeof(cartStr), "%s", "Auto-Scale to data view");
    cartSetString( cart, o9, cartStr);
    }
wigCart->autoScale = wiggleAutoScale;

/*	And set the other values back into the cart for hgTrackUi	*/
if( track->visibility == tvFull )
    {		/*	no need to change this in the cart when dense */
    snprintf( cartStr, sizeof(cartStr), "%d", heightFromCart );
    cartSetString( cart, o1, cartStr );	/* possibly clipped from above */
    }
snprintf( cartStr, sizeof(cartStr), "%g", track->minRange);
cartSetString( cart, o4, cartStr);
snprintf( cartStr, sizeof(cartStr), "%g", track->maxRange);
cartSetString( cart, o5, cartStr);

track->loadItems = wigLoadItems;
track->freeItems = wigFreeItems;
track->drawItems = wigDrawItems;
track->itemName = wigName;
track->mapItemName = wigName;
track->totalHeight = wigTotalHeight;
track->itemHeight = tgFixedItemHeight;
track->itemStart = tgItemNoStart;
track->itemEnd = tgItemNoEnd;
/*	using subType in an attempt to piggyback on the sample tracks
 *	this will cause the scale to be printed in the left column
 *	Although it has a lower limit of 0, which is arbitrary.
 */
track->subType = lfSubSample;     /*make subType be "sample" (=2)*/
track->mapsSelf = TRUE;
track->extraUiData = (void *) wigCart;
track->colorShades = shadesOfGray;
}	/*	wigMethods()	*/
