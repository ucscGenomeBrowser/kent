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

static char const rcsid[] = "$Id: wigTrack.c,v 1.5 2003/09/25 07:11:37 hiram Exp $";

struct wigItem
/* A wig track item. */
    {
    struct wigItem *next;
    char *name;		/* Common name */
    char *db;		/* Database */
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

/****           some simple debug output during development	*/
static char dbgFile[] = "/tmp/wig.dbg";
static boolean debugOpened = FALSE;
static FILE * dF;

static void debugOpen(char * name) {
if( debugOpened ) return;
dF = fopen( dbgFile, "w" );
fprintf( dF, "opened by %s\n", name );
chmod(dbgFile, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP  | S_IROTH | S_IWOTH | S_IXOTH );
debugOpened = TRUE;
}

#define DBGMSGSZ	1023
char dbgMsg[DBGMSGSZ+1];
static void debugPrint(char * name) {
debugOpen(name);
if( debugOpened ) {
    if( dbgMsg[0] )
    fprintf( dF, "%s: %s\n", name, dbgMsg);
    else
    fprintf( dF, "%s:\n", name);
    }
    dbgMsg[0] = (char) NULL;
    fflush(dF);
}

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
struct linkedFeatures *lf = item;
return lf->name;
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

/*	heightPer was already set in wigMethods	*/
/*tg->heightPer = tl.fontHeight;*/
tg->lineHeight = max(tl.fontHeight + 1, tg->heightPer);
tg->height = tg->lineHeight;

/*	To do this calculation we need to walk through the items and
 *	note the ones that belong together.  A single item can have
 *	multiple rows (==bins of data), they all belong together.
 */

if( tg->visibility == tvFull )
    {
	itemCount = 1;
    for (item = tg->items; item != NULL; item = item->next)
	{
	    if( item->next != NULL )
		if( sameWigGroup( tg->itemName(tg, item),
			    tg->itemName(tg, item->next), 1 ))
		    ++itemCount;
	}
    tg->height = itemCount * tg->lineHeight;
    }
if( tg->visibility == tvDense )
    tg->height = tg->lineHeight;

return tg->height;
}


/*	Given the data read in from a table row, construct a single
 *	instance of a linked feature.  Expand the wiggle binary file
 *	name to include a full /gbdb/<db>/wib/<File> path specification.
 *	Include the extra fields from the table row in a wigItem
 *	structure accessible via lf->extra
 */
struct linkedFeatures *lfFromWiggle(struct wiggle *wiggle)
    /* Return a linked feature from a full wiggle track. */
{
struct wigItem *wi;
struct linkedFeatures *lf;
struct simpleFeature *sf, *sfList = NULL;
int grayIx = grayInRange(wiggle->score, 0, 127);
unsigned Span = wiggle->Span;	/* bases spanned per data value */
unsigned Count = wiggle->Count;	/* number of values in this row */
unsigned Offset = wiggle->Offset;	/* start of data in the data file */
char *File = (char *) NULL;
int i;
size_t FileNameSize = 0;

assert(Span > 0 && Count > 0 && wiggle->File != NULL);

AllocVar(lf);
AllocVar(wi);
lf->grayIx = grayIx;
strncpy(lf->name, wiggle->name, sizeof(lf->name));
chopSuffix(lf->name);
lf->orientation = orientFromChar(wiggle->strand[0]);

FileNameSize = strlen("/gbdb//wib/") + strlen(database) + strlen(wiggle->File) + 1;
File = (char *) needMem((size_t)FileNameSize);
snprintf(File, FileNameSize, "/gbdb/%s/wib/%s", database, wiggle->File);

if( ! fileExists(File) )
    errAbort("wiggle load: file '%s' missing", File );

AllocVar(sf);
sf->start = wiggle->chromStart;
sf->end = sf->start + (Span * Count);
sf->grayIx = grayIx;
slAddHead(&sfList, sf);

lf->components = sfList;
wi->Span = wiggle->Span;
wi->Count = wiggle->Count;
wi->Offset = wiggle->Offset;
wi->File = cloneString(File);
lf->extra = wi;	/* does anyone need to free this ? */
linkedFeaturesBoundsAndGrays(lf);
lf->start = wiggle->chromStart;
lf->end = wiggle->chromEnd;

return lf;
}	/*	lfFromWiggle()	*/

/*	This is a simple shell that reads the database table rows.
 *	The real work actually takes place in the lfFromWiggle()
 *	This does make up the hash of spans to be used during Draw
 *	The trackSpans has is interesting.  It will be a hash of hashes
 *	The first level has will be the track name
 *	Inside that has will be a has of Spans for that track
 */
static void wigLoadItems(struct track *tg) {
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct wiggle *wiggle;
struct linkedFeatures *lfList = NULL;
struct linkedFeatures *lf;
char *where = NULL;
int rowsLoaded = 0;
char spanName[128];
struct hashEl *el, *elList;
struct hashEl *el2, *elList2;
struct hash *spans = NULL;	/* Spans encountered during load */

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, where, &rowOffset);

/*	Allocate trackSpans one time only	*/
if( ! trackSpans )
    trackSpans = newHash(0);
/*	Each instance of this LoadItems will create a new spans hash
 *	It will be the value included in the trackSpans hash
 */
spans = newHash(0);
while ((row = sqlNextRow(sr)) != NULL)
    {
	struct wigItem *wi;
	++rowsLoaded;
	wiggle = wiggleLoad(row + rowOffset);
	lf = lfFromWiggle(wiggle);
	el = hashLookup(trackSpans, lf->name);
	if ( el == NULL )
	    	hashAdd(trackSpans, lf->name, spans);
	wi = lf->extra;
	snprintf(spanName, sizeof(spanName), "%d", wi->Span );
	el = hashLookup(spans, spanName);
	if ( el == NULL )
	    	hashAddInt(spans, spanName, wi->Span);
	slAddHead(&lfList, lf);
	wiggleFree(&wiggle);
    }

if(where != NULL)
        freez(&where);
sqlFreeResult(&sr);
hFreeConn(&conn);

slReverse(&lfList);
tg->items = lfList;
}	/*	wigLoadItems()	*/

static void wigFreeItems(struct track *tg) {
debugPrint("wigFreeItems");
}

static void wigDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct vGfx *vg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
struct linkedFeatures *lf;
double pixelsPerBase = scaleForPixels(width);
double basesPerPixel = 1.0;
struct rgbColor *normal = &(tg->color);
int black;
int itemCount = 0;
char *currentFile = (char *) NULL;	/*	the binary file name */
FILE *f = (FILE *) NULL;		/*	file handle to binary file */
struct hashEl *el, *elList;
char cartStr[64];	/*	to set cart strings	*/
char *interpolate = NULL;	/*	samples only, or interpolate */
char o2[128];	/*	Option 2 - interpolate or samples only	*/
enum wiggleOptEnum wiggleType;

snprintf( o2, sizeof(o2), "%s.linear.interp", tg->mapName);
interpolate = cartOptionalString(cart, o2);
if( interpolate )
    wiggleType = wiggleStringToEnum(interpolate);
    else 
    {
    wiggleType = wiggleStringToEnum("Linear Interpolation");
    /*	And set that value back into the cart for hgTrackUi	*/
    snprintf( cartStr, sizeof(cartStr), "%s", "Linear Interpolation" );
    cartSetString( cart, o2, cartStr );
    }

if( pixelsPerBase > 0.0 )
    basesPerPixel = 1.0 / pixelsPerBase;

for (lf = tg->items; lf != NULL; lf = lf->next)
    ++itemCount;

black = vgFindColorIx(vg, 0, 0, 0);

/*	width - width of drawing window in pixels
 *	pixelsPerBase - pixels per base
 *	basesPerPixel - calculated as 1.0/pixelsPerBase
 */
snprintf(dbgMsg, DBGMSGSZ, "========================  Next Item  ============\nwidth: %d, height: %d, heightPer: %d, pixelsPerBase: %.4f", width, tg->lineHeight, tg->heightPer, pixelsPerBase );
debugPrint("wigDrawItems");
snprintf(dbgMsg, DBGMSGSZ, "seqStart: %d, seqEnd: %d, xOff: %d, yOff: %d, black: %d", seqStart, seqEnd, xOff, yOff, black );
debugPrint("wigDrawItems");
snprintf(dbgMsg, DBGMSGSZ, "itemCount: %d, Y range: %.1f - %.1f, basesPerPixel: %.4f", itemCount, tg->minRange, tg->maxRange, basesPerPixel );
debugPrint("wigDrawItems");

itemCount = 0;
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    struct simpleFeature *sf = lf->components;
    struct wigItem *wi = lf->extra;
    int usingDataSpan = 1;
    unsigned char *ReadData;
    int pixelsToDraw = 0;
    unsigned char *dataPtr;
    int dataStarts;	/*	chrom coords	*/
    int dataEnds;	/*	chrom coords	*/
    int dataSpan;	/*	chrom coords	*/
    int dataPointsInView;	/*	number of data points to use */
    double dataValuesPerPixel;	/*  values in the data file per pixel */
    int pixelsPerDataValue;	/*  to specify drawing box width */
    int x1 = 0;
    int y1,w,h,x2,y2;

    h = tg->lineHeight;
    /*	Take a look through the potential spans, and given what we have
     *	here for basesPerPixel, pick the largest usingDataSpan that is
     *	not greater than the basesPerPixel
     */
    el = hashLookup(trackSpans, lf->name);	/*  What Spans do we have */
    elList = hashElListHash(el->val);		/* Our pointer to spans hash */
    for (el = elList; el != NULL; el = el->next)
	{
	int Span;
	Span = (int) el->val;
	if( (Span < basesPerPixel) && (Span > usingDataSpan) )
	    usingDataSpan = Span;
	}
    hashElFreeList(&elList);

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
 *	chrom coords:  [sf->start : sf-end)
 *
 *	The data to be drawn: to be read from file f at offset wi->Offset
 *	data points available: wi->Count, representing wi->Span bases
 *	for each data point
 *
 *	The drawing window, in pixels:
 *	xOff = left margin, y1 = top margin, h = height
 *	drawing window in chrom coords: seqStart, seqEnd
 *	basesPerPixel is known, 'pixelsPerBase' is known
 */
	fseek(f, wi->Offset, SEEK_SET);
	ReadData = (unsigned char *) needMem((size_t) (wi->Count + 1));
	fread(ReadData, (size_t) wi->Count, (size_t) sizeof(unsigned char), f);

	dataStarts = max(sf->start,seqStart);
	dataEnds = min(sf->end,seqEnd);
	dataSpan = dataEnds - dataStarts;
	dataPointsInView = dataSpan / usingDataSpan;
	if( dataPointsInView < 1 ) dataPointsInView = 1;
	w = (dataEnds - dataStarts) * pixelsPerBase;
	dataValuesPerPixel = (double) dataPointsInView / (double) w;
	pixelsPerDataValue = 1.0 / dataValuesPerPixel;
	if( pixelsPerDataValue < 1 ) pixelsPerDataValue = 1;
	if( w < 0 ) w = 0;
snprintf(dbgMsg, DBGMSGSZ, "seek to: %d, read: %d bytes, dataStarts: %d, dataEnds: %d, pixels: %d", wi->Offset, wi->Count, dataStarts, dataEnds, w );
debugPrint("wigDrawItems");
snprintf(dbgMsg, DBGMSGSZ, "pixelsPerBase %.4f / dataPointsInView: %d = dataValuesPerPixel: %.4f", pixelsPerBase, dataPointsInView, dataValuesPerPixel );
debugPrint("wigDrawItems");
snprintf(dbgMsg, DBGMSGSZ, "pixelsPerDataValue %d", pixelsPerDataValue );
debugPrint("wigDrawItems");
    	dataPtr = ReadData + (dataStarts - sf->start);;
	if( usingDataSpan == 1 ) {
		x1 = xOff + (dataStarts - seqStart)*pixelsPerBase;
snprintf(dbgMsg, DBGMSGSZ, "Data Points: %d, Width: %d, start at x1: %d", wi->Count, w, x1 );
debugPrint("wigDrawItems");
	for ( pixelsToDraw = 0; pixelsToDraw < w; ++pixelsToDraw )
	    {
		int boxHeight;
		int dataValue;
    		int skipDataPoints;
    		skipDataPoints = dataValuesPerPixel * pixelsToDraw;
		dataValue = *(dataPtr + pixelsToDraw + skipDataPoints);
		if( ((int) dataValuesPerPixel > 1) &&
			    (wiggleType == wiggleLinearInterpolation) )
		    {		/* skipping data points, find maximum */
			int j;	/*	in this area skipped	*/
			int validData = 0;
			unsigned char data;
			dataValue = 0;
			for( j = 0; j < (int) dataValuesPerPixel; ++j )
			    {
				data = *(dataPtr + pixelsToDraw + j);
				if( data < WIG_NO_DATA )
				    {
				    dataValue = max(dataValue,data);
				    ++validData;
				    }
			    }
			/*	Is there actually any valid data here */
			if( ! validData ) dataValue = WIG_NO_DATA;
		    }
		/*	Check for the NO_DATA situation	*/
		if( dataValue < WIG_NO_DATA )
		    {
		    boxHeight = (h * dataValue) / 128;
		    if( boxHeight < 1 ) boxHeight = 1;
		    x1 = pixelsToDraw + xOff +
			(dataStarts - seqStart)*pixelsPerBase;
		    y1 = yOff - boxHeight + h;
		    vgBox(vg, x1, y1, pixelsPerDataValue, boxHeight, black);
		    }
		if( pixelsToDraw == w ) {
snprintf(dbgMsg, DBGMSGSZ, "finished at x1: %d, skipDataPoints: %d", x1, skipDataPoints );
debugPrint("wigDrawItems");
		}
	    }
	} else {
	y1 = yOff;
	x1 = xOff + (sf->start - seqStart)*pixelsPerBase;
	vgBox(vg, x1, y1, w, h, black);
	}

snprintf(dbgMsg, DBGMSGSZ, "itemCount: %d, start: %d, end: %d, X: %d->%d, File: %s", itemCount, sf->start, sf->end, x1, x1+w, wi->File );
debugPrint("wigDrawItems");

	freeMem(ReadData);
	}	/*	Draw if span is correct	*/
    }	/*	for ( each item )	*/
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
char o4[128];	/*	Option 4 - minimum Y axis value: .minY	*/
char o5[128];	/*	Option 5 - maximum Y axis value: .minY	*/
char *minY_str;	/*	string from cart	*/
char *maxY_str;	/*	string from cart	*/
char *heightPer;	/*	string from cart	*/
int heightFromCart;	/*	truncated by limits	*/
double minYc;	/*	from cart */
double maxYc;	/*	from cart */
double minY;	/*	from trackDb.ra words, the absolute minimum */
double maxY;	/*	from trackDb.ra words, the absolute maximum */
char cartStr[64];	/*	to set cart strings	*/


/*	Start with an arbitrary min,max when not given in the .ra file	*/
minY = DEFAULT_MIN_Yv;
maxY = DEFAULT_MAX_Yv;

/*	Possibly fetch values from the trackDb.ra file	*/
if( wordCount > 1 ) minY = atof(words[1]);
if( wordCount > 2 ) maxY = atof(words[2]);

/*	Possibly fetch values from the cart	*/
snprintf( o1, sizeof(o1), "%s.heightPer", track->mapName);
snprintf( o4, sizeof(o4), "%s.minY", track->mapName);
snprintf( o5, sizeof(o5), "%s.maxY", track->mapName);
heightPer = cartOptionalString(cart, o1);
minY_str = cartOptionalString(cart, o4);
maxY_str = cartOptionalString(cart, o5);

if( minY_str ) minYc = atof(minY_str);
else minYc = minY;

if( maxY_str ) maxYc = atof(maxY_str);
else maxYc = maxY;

/*	Clip the cart value to range [MIN_HEIGHT_PER:DEFAULT_HEIGHT_PER] */
if( heightPer ) heightFromCart = min( DEFAULT_HEIGHT_PER, atoi(heightPer) );
else heightFromCart = DEFAULT_HEIGHT_PER;

track->heightPer = max( MIN_HEIGHT_PER, heightFromCart );

/*	The values from trackDb.ra are the clipping boundaries, do
 *	not let cart settings go outside that range
 */
track->minRange = max( minY, minYc );
track->maxRange = min( maxY, maxYc );

/*	And set those values back into the cart for hgTrackUi	*/
snprintf( cartStr, sizeof(cartStr), "%d", track->heightPer );
cartSetString( cart, o1, cartStr );
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
}
