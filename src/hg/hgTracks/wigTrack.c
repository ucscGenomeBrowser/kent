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

static char const rcsid[] = "$Id: wigTrack.c,v 1.13 2003/10/04 02:18:28 hiram Exp $";

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
    double minY;	/*	from trackDb.ra words, the absolute minimum */
    double maxY;	/*	from trackDb.ra words, the absolute maximum */
    };

struct wigItem
/* A wig track item. */
    {
    struct wigItem *next;
    int start, end;	/* Start/end in browser coordinates. */
    int grayIx;         /* Level of gray usually. */
    char *name;		/* Common name */
    char *db;		/* Database */
    int orientation;	/* Orientation */
    int ix;		/* Position in list. */
    int height;		/* Pixel height of item. */
    unsigned Span;      /* each value spans this many bases */
    unsigned Count;     /* number of values to use */
    unsigned Offset;    /* offset in File to fetch data */
    char *File; /* path name to data file, one byte per value */
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

#ifdef DEBUG
/****           some simple debug output during development	*/
static char dbgFile[] = "/tmp/wig.dbg";
static boolean debugOpened = FALSE;
static FILE * dF;

static void wigDebugOpen(char * name) {
if( debugOpened ) return;
dF = fopen( dbgFile, "w" );
fprintf( dF, "opened by %s\n", name );
chmod(dbgFile, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP  | S_IROTH | S_IWOTH | S_IXOTH );
debugOpened = TRUE;
}

#define DBGMSGSZ	1023
char dbgMsg[DBGMSGSZ+1];
void wigDebugPrint(char * name) {
wigDebugOpen(name);
if( debugOpened )
    {
    if( dbgMsg[0] )
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
if( different )
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
char o1[128];
int itemCount = 1;

/*	heightPer was already set in wigMethods	since it may have come
 *	from the cart and all cart stuff is there.  A track is just one
 *	item, so there is nothing to do here, either it is the tvFull
 *	height as chosen by the user from TrackUi, or it is the dense
 *	mode.  All of this was already taken care of in wigMethods
 */
if( vis == tvDense )
    tg->lineHeight = tl.fontHeight+1;
else if( vis == tvFull )
    tg->lineHeight = max(tl.fontHeight + 1, tg->heightPer);

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

if( basesPerPixel >= 1024 ) {
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
if( itemsLoaded )
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
if( ! trackSpans )
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

	if( ! fileExists(File) )
	    errAbort("wigLoadItems: file '%s' missing", File );
	wi->File = cloneString(File);
	freeMem(File);

	wi->Span = wiggle->Span;
	wi->Count = wiggle->Count;
	wi->Offset = wiggle->Offset;

	el = hashLookup(trackSpans, wi->name);
	if ( el == NULL )
	    	hashAdd(trackSpans, wi->name, spans);
	snprintf(spanName, sizeof(spanName), "%d", wi->Span );
	el = hashLookup(spans, spanName);
	if ( el == NULL )
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
#ifdef DEBUG
snprintf(dbgMsg, DBGMSGSZ, "I haven't seen wigFreeItems ever called ?" );
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
Color shadesOfPrimary[EXPR_DATA_SHADES];
Color shadesOfAlt[EXPR_DATA_SHADES];
Color black = vgFindColorIx(vg, 0, 0, 0);
struct rgbColor blackColor = {0, 0, 0};
int totalLoopCount = 0;	/*	to measure loop performance */

vgMakeColorGradient(vg, &blackColor, &tg->color, EXPR_DATA_SHADES, shadesOfPrimary );
vgMakeColorGradient(vg, &blackColor, &tg->altColor, EXPR_DATA_SHADES, shadesOfAlt );

wigCart = (struct wigCartOptions *) tg->extraUiData;
wiggleType = wigCart->wiggleType;
horizontalGrid = wigCart->horizontalGrid;
lineBar = wigCart->lineBar;

if( pixelsPerBase > 0.0 )
    basesPerPixel = 1.0 / pixelsPerBase;

/*	width - width of drawing window in pixels
 *	pixelsPerBase - pixels per base
 *	basesPerPixel - calculated as 1.0/pixelsPerBase
 */
itemCount = 0;

for (wi = tg->items; wi != NULL; wi = wi->next)
    {
    int usingDataSpan = 1;	/* will become larger if possible */
    unsigned char *ReadData;	/* the bytes read in from the file */
    int pixelToDraw = 0;
    int dV;			/* loop counter over data view	*/
    unsigned char *dataStarts;	/* pointer into ReadData	*/
    int dataViewStarts;	/*	chrom coords	*/
    int dataViewEnds;	/*	chrom coords	*/
    int dataOffsetStart;	/*	offset into data file	*/
    int dataOffsetEnd;		/*	offset into data file	*/
    int dataSpan;	/*	chrom coords	*/
    double dataPointsInView;	/*	number of data points to use */
    double dataValuesPerPixel;	/*  values in the data file per pixel */
    int pixelsPerDataValue;	/*  to specify drawing box width */
    int x1 = 0;
    int w,h,x2,y2;
    int loopCount = 0;	/*	to catch a runaway drawing loop */
    int dataOffset = 0;		/*	within data block during drawing */
    int prevDataOffset = 0;	/*	previous data block item used  */
    int OffsetIncrement = 1;	/*	for loop control	*/
    int minimalSpan = 1000000;	/*	a lower limit safety check */

    h = tg->lineHeight;
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
	if( (Span < basesPerPixel) && (Span > usingDataSpan) )
	    usingDataSpan = Span;
	if( Span < minimalSpan )
	    minimalSpan = Span;
	}
    hashElFreeList(&elList);

    /*	There may not be a span of 1, use whatever is lowest	*/
    if( minimalSpan > usingDataSpan )
	usingDataSpan = minimalSpan;

    /*	Now that we know what Span to draw, see if this item should be
     *	drawn at all.
     */
    if( usingDataSpan == wi->Span )
	{
	/*	Check our data file, see if we need to open a new one */
	if ( currentFile )
	    {
	    if( differentString(currentFile,wi->File) )
		{
		if( f != (FILE *) NULL )
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
	++itemCount;
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

	dataViewStarts = max(wi->start,seqStart);
	dataViewEnds = min(wi->end,seqEnd);
	dataOffsetStart = (dataViewStarts - wi->start)/ usingDataSpan;
	dataOffsetEnd = wi->Count - ((wi->end - dataViewEnds) / usingDataSpan);
	dataSpan = dataViewEnds - dataViewStarts;

/* If this data block doesn't span any data (for some unknown reason)
 *	then move on to the next block of data.
 */
	if( dataSpan < 1 ) continue;

	dataPointsInView = (double) dataSpan / (double) usingDataSpan;
	w = (int)round((double)(dataViewEnds - dataViewStarts) * pixelsPerBase);
	if( w < 1 ) w = 1;
	dataValuesPerPixel = dataPointsInView / (double) w;
	pixelsPerDataValue = (int) round(1.0 / dataValuesPerPixel);
	if( pixelsPerDataValue < 1 ) pixelsPerDataValue = 1;
    	dataStarts = ReadData + dataOffsetStart;

	loopCount = 0;	/*	A safety timeout for the loop below	*/
	x1 = 0;
	prevDataOffset = -1;	/* -1 will cause the first one to be used */
	OffsetIncrement = 1;

	/*	Examine every point in this data block	*/
	for( dataOffset = 0;
		(dataOffset < dataPointsInView) && (loopCount < 1000000);
		dataOffset += OffsetIncrement )
	    {
	    int validData = 0;
	    int data = 0;
	    int dataValue = 0;
	    int skippedDataPoints = dataOffset - prevDataOffset;
	    int j;

	    ++loopCount;	/*	safety timeout	*/

	    /*	Pixel coordinate for this data point	*/
	    x1 = xOff + ((dataViewStarts - seqStart) * pixelsPerBase) +
		(dataOffset*usingDataSpan)*pixelsPerBase;

	    /*	OffsetIncrement is calculated such that the next point
	     *	to check will be at least (x1 + 1) pixel coordinates,
	     *	thus a new point to graph.  It is the above equation
	     *	solved for OffsetIncrement with (x1 + 1) in place
	     *	of x1, and (dataOffset+OffsetIncrement) in place of
	     *	dataOffset.
	     */
	    OffsetIncrement =
	((x1 + 1 - xOff - ((dataViewStarts - seqStart) * pixelsPerBase)) /
	(usingDataSpan*pixelsPerBase)) - dataOffset;
	    /*	Must be at least one to be moving on	*/
	    if( OffsetIncrement < 1 ) OffsetIncrement = 1;

	    /*	Between the point previously checked and this one, look
	     *	at all the points in between and find the max.  This is
	     *	the zooming function.  Might want other functions here
	     *	other than max()
	     *	This loop will be at least one data point.
	     */
	    for( j = 0; j < skippedDataPoints; ++j )
		{
		data = *(dataStarts + prevDataOffset + 1 + j);
		if( data < WIG_NO_DATA )
		    {
		    dataValue = max(dataValue,data);
		    ++validData;
		    }
		}
	    prevDataOffset = dataOffset;
	    /*	did we end up with some valid data here ?
	     *	Yes, this is a double test here.  If validData is not
	     *	true it is not true because dataValue is < WIG_NO_DATA
	     *	But just in case something went wrong above, make sure
	     *	dataValue is OK.
	     */
	    if( validData && (dataValue < WIG_NO_DATA) )
		{
		double dY;
		double dataScale = (wigCart->maxY - wigCart->minY) /
			(double) MAX_WIG_VALUE;
		double dataPoint;
		int boxHeight;

/*	drawing region here is from upper left (x1,yOff)
 *	with width of pixelsPerDataValue
 *	and height of h  (! Y axis is 0 at top ! -> h is at bottom )
 *	So, within those constraints, lets figure out where we want to
 *	draw a box, given a 'dataValue' score in range [0:MAX_WIG_VALUE]
 *	and our data scale of range [tg->minRange:tg->maxRange]
 *	with upper data scale limit of wigCart->maxY
 *	and lower data scale limit of wigCart->minY
 */
		dY = (double) dataValue * dataScale;
		dataPoint = wigCart->minY + dY;
		if( vis == tvFull )
		    {
		    double viewRange = tg->maxRange - tg->minRange;
		    double dataZeroOffset;
		    double viewZero = 0.0;
		    int zeroLine = h;	/*	often at the bottom	*/
		    int boxTop = 0;	/*	will be adjusted below	*/

		    /*	Graph only if data point is in viewing window.
		     *	If the user has selected upper and lower
		     *	boundaries to view, these become filtering
		     *	devices.  The viewer can select a range of
		     *	values to see and only values in that range can
		     *	be seen.
		     */
		    if( (dataPoint >= (tg->minRange - (viewRange/1000.0))) &&
			    (dataPoint <= (tg->maxRange + (viewRange/1000.0))))
			{

			viewZero = 0.0;
			if( tg->minRange > 0.0 ) {
			    viewZero = tg->minRange;
			    zeroLine = h-1;	/*	under the bottom */
			} else if( tg->maxRange < 0.0 ) {
			    viewZero = tg->maxRange;
			    zeroLine = 0;	/*	over the top	*/
			} else {
			    zeroLine = (int) ((double) h *
				((tg->maxRange - 0.0) / viewRange) );
			}
			dataZeroOffset = fabs(dataPoint - viewZero);
			boxHeight = (int) ((double) h *
				(dataZeroOffset / viewRange));

		    /* we are going to draw a box of boxHeight starting
		     * at Y = yOff + boxTop
		     */
			if( zeroLine < 0 ) zeroLine = 0;
			if( zeroLine > h ) zeroLine = h;

		drawColor = tg->ixColor;
		    if( dataPoint < 0.0 )
			{
			boxTop = zeroLine;
			drawColor = tg->ixAltColor;
			}
		    else if( dataPoint > 0.0 )
			{
			boxTop = zeroLine - boxHeight;
			}
		    else
		        {
			boxHeight = 1;
			boxTop = zeroLine;
		        }

		    if( boxTop > h-1 ) boxTop = h - 1;
		    if( boxTop < 0 ) boxTop = 0;

		    if( boxHeight + boxTop > h ) boxHeight = h - boxTop;
		    if( boxHeight < 1 ) boxHeight = 1;

		    if( lineBar == wiggleGraphBar )
			{
			vgBox(vg, x1, yOff+boxTop,
				pixelsPerDataValue, boxHeight, drawColor );
			}
		    else
			{
			int y1 = yOff+boxTop+boxHeight;
			if( dataPoint > viewZero ) y1 = yOff+boxTop;
			vgBox(vg, x1, y1-1, pixelsPerDataValue, 3, black );
			}
			}	/*	if data point in view	*/
		    }	/*	vis == tvFull	*/
		else if( vis == tvDense )
		    {
		    if( tg->colorShades )
			drawColor =
tg->colorShades[grayInRange(dataValue, tg->maxRange, tg->minRange)];

		    boxHeight = tg->lineHeight;
		    vgBox(vg, x1, yOff, pixelsPerDataValue,
			    boxHeight, drawColor );
		    }	/*	vis == tvDense	*/
		}	/*	we have valid data here	*/
	    }	/*	loop working through this data block	*/
    totalLoopCount += loopCount;
	freeMem(ReadData);
	}	/*	Draw if span is correct	*/
    }	/*	for ( each item )	*/

    /*	Do we need to draw a horizontalGrid ?	*/
    if( (vis == tvFull) && (horizontalGrid == wiggleHorizontalGridOn) )
	{
	double yRange;
	int x1, x2, y1, y2;
	int gridLines = 2;
	int drewLines = 0;

	yRange = tg->maxRange - tg->minRange;
	x1 = xOff;
	x2 = x1 + width;
	if( tg->lineHeight > 64 )
	    gridLines = 4;
	else if( tg->lineHeight > 32 )
	    gridLines = 3;
	for( y1 = yOff; y1 <= tg->lineHeight+yOff;
		y1 += (int)((double)(tg->lineHeight-1)/(double)gridLines))
	    {
	    y2 = y1;
	    ++drewLines;
	    vgLine(vg,x1,y1,x2,y2,black);
	    }
	}	/*	drawing horizontalGrid	*/

if( f != (FILE *) NULL )
    {
    fclose(f);
    freeMem(currentFile);
    }
}	/*	wigDrawItems()	*/

/* Make track group for wig multiple alignment. */
void wigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
{
char o1[128];	/*	Option 1 - track pixel height:  .heightPer	*/
char o2[128];	/*	Option 2 - interpolate or samples only	*/
char o4[128];	/*	Option 4 - minimum Y axis value: .minY	*/
char o5[128];	/*	Option 5 - maximum Y axis value: .minY	*/
char o7[128];	/*	Option 7 - horizontal grid lines: horizGrid */
char o8[128];	/*	Option 8 - type of graph, lineBar */
char *interpolate = NULL;	/*	samples only, or interpolate */
char *horizontalGrid = NULL;	/*	Grid lines, ON/OFF - off default */
char *lineBar = NULL;	/*	Line or Bar chart - Bar by default */
char *minY_str = NULL;	/*	string from cart	*/
char *maxY_str = NULL;	/*	string from cart	*/
char *heightPer = NULL;	/*	string from cart	*/
int heightFromCart;	/*	truncated by limits	*/
double minYc;	/*	from cart */
double maxYc;	/*	from cart */
enum wiggleOptEnum wiggleType;
enum wiggleGridOptEnum wiggleHorizGrid;
enum wiggleGraphOptEnum wiggleLineBar;
double minY;	/*	from trackDb.ra words, the absolute minimum */
double maxY;	/*	from trackDb.ra words, the absolute maximum */
char cartStr[64];	/*	to set cart strings	*/
struct wigCartOptions *wigCart;


AllocVar(wigCart);

/*	Start with an arbitrary min,max when not given in the .ra file	*/
minY = DEFAULT_MIN_Yv;
maxY = DEFAULT_MAX_Yv;

/*	Possibly fetch values from the trackDb.ra file	*/
if( wordCount > 1 ) minY = atof(words[1]);
if( wordCount > 2 ) maxY = atof(words[2]);
/*	Let's ensure their order is correct	*/
if( maxY < minY )
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
heightPer = cartOptionalString(cart, o1);
interpolate = cartOptionalString(cart, o2);
minY_str = cartOptionalString(cart, o4);
maxY_str = cartOptionalString(cart, o5);
horizontalGrid = cartOptionalString(cart, o7);
lineBar = cartOptionalString(cart, o8);

if( minY_str ) minYc = atof(minY_str);
else minYc = minY;

if( maxY_str ) maxYc = atof(maxY_str);
else maxYc = maxY;

/*	Clip the cart value to range [MIN_HEIGHT_PER:DEFAULT_HEIGHT_PER] */
if( heightPer ) heightFromCart = min( DEFAULT_HEIGHT_PER, atoi(heightPer) );
else heightFromCart = DEFAULT_HEIGHT_PER;

if( track->visibility == tvDense ) {
    track->heightPer = tl.fontHeight + 1;
} else {
    track->heightPer = max( MIN_HEIGHT_PER, heightFromCart );
}

/*	The values from trackDb.ra are the clipping boundaries, do
 *	not let cart settings go outside that range, and keep them
 *	in proper order.
 */
track->minRange = max( minY, minYc );
track->maxRange = min( maxY, maxYc );
if( track->maxRange < track->minRange )
    {
	double d;
	d = track->maxRange;
	track->maxRange = track->minRange;
	track->minRange = d;
    }

/*	If interpolate is a string, it came from the cart, otherwise set
 *	the default for this option and stuff it into the cart
 */
if( interpolate )
    wiggleType = wiggleStringToEnum(interpolate);
else 
    {
    wiggleType = wiggleStringToEnum("Linear Interpolation");
    snprintf( cartStr, sizeof(cartStr), "%s", "Linear Interpolation" );
    cartSetString( cart, o2, cartStr );
    }
wigCart->wiggleType = wiggleType;

/*	If horizontalGrid is a string, it came from the cart, otherwise set
 *	the default for this option and stuff it into the cart
 */
if( horizontalGrid )
    wiggleHorizGrid = wiggleGridStringToEnum(horizontalGrid);
else 
    {
    wiggleHorizGrid = wiggleGridStringToEnum("OFF");
    snprintf( cartStr, sizeof(cartStr), "%s", "OFF" );
    cartSetString( cart, o7, cartStr );
    }
wigCart->horizontalGrid = wiggleHorizGrid;

/*	If lineBar is a string, it came from the cart, otherwise set
 *	the default for this option and stuff it into the cart
 */
if( lineBar )
    wiggleLineBar = wiggleGraphStringToEnum(lineBar);
else 
    {
    wiggleLineBar = wiggleGraphStringToEnum("Bar");
    snprintf( cartStr, sizeof(cartStr), "%s", "Bar" );
    cartSetString( cart, o8, cartStr );
    }
wigCart->lineBar = wiggleLineBar;

/*	And set the other values back into the cart for hgTrackUi	*/
if( track->visibility == tvFull )
    {		/*	no need to change this in the cart when dense */
    snprintf( cartStr, sizeof(cartStr), "%d", track->heightPer );
    cartSetString( cart, o1, cartStr );
    }
snprintf( cartStr, sizeof(cartStr), "%g", track->minRange );
cartSetString( cart, o4, cartStr );
snprintf( cartStr, sizeof(cartStr), "%g", track->maxRange );
cartSetString( cart, o5, cartStr );

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
