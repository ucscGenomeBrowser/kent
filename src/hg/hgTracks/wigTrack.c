/* wigTrack - stuff to handle loading and display of
 * wig type tracks in browser. Wigs are arbitrary data graphs
 */

#include "common.h"
#include "obscure.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "wiggle.h"
#include "scoredRef.h"
#ifndef GBROWSE
#include "customTrack.h"
#endif /* GBROWSE */
#include "wigCommon.h"

static char const rcsid[] = "$Id: wigTrack.c,v 1.77 2008/05/23 22:14:58 angie Exp $";

struct wigItem
/* A wig track item. */
    {
    struct wigItem *next;
    int start, end;	/* Start/end in chrom (aka browser) coordinates. */
    char *db;		/* Database */
    int ix;		/* Position in list. */
    int height;		/* Pixel height of item. */
    unsigned span;      /* each value spans this many bases */
    unsigned count;     /* number of values to use */
    unsigned offset;    /* offset in File to fetch data */
    char *file; /* path name to data file, one byte per value */
    double lowerLimit;  /* lowest data value in this block */
    double dataRange;   /* lowerLimit + dataRange = upperLimit */
    unsigned validCount;        /* number of valid data values in this block */
    double sumData;     /* sum of the data points, for average and stddev calc */
    double sumSquares;      /* sum of data points squared, for stddev calc */
    double graphUpperLimit;	/* filled in by DrawItems	*/
    double graphLowerLimit;	/* filled in by DrawItems	*/
    };

static void wigFillInColorLfArray(struct track *wigTrack, Color *colArray, int colSize,
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

static void wigFillInColorBedArray(struct track *wigTrack, Color *colArray, int colSize,
				  struct track *colorTrack, struct hvGfx *hvg)
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
    for(i = x1; i < x2 && i < colSize; i++)
	{
	if(colorTrack->itemColor != NULL)
	    colArray[i] = colorTrack->itemColor(colorTrack, bed, hvg);
	else
	    colArray[i] = colorTrack->ixColor;
	}
    }
}

void wigFillInColorArray(struct track *wigTrack, struct hvGfx *hvg, 
			 Color *colorArray, int colSize, struct track *colorTrack)
/* Fill in a color array with the colorTrack's color where
   it would normally have an exon. */
{
boolean trackLoaded = FALSE;
/* If the track is hidden currently, load the items. */
if(colorTrack->limitedVis == tvHide)
    {
    trackLoaded = TRUE;
    colorTrack->loadItems(colorTrack);
    colorTrack->ixColor = hvGfxFindRgb(hvg, &colorTrack->color);
    colorTrack->ixAltColor = hvGfxFindRgb(hvg, &colorTrack->altColor);
    }

if(colorTrack->drawItemAt == linkedFeaturesDrawAt)
    wigFillInColorLfArray(wigTrack, colorArray, colSize, colorTrack);
else if(colorTrack->drawItemAt == bedDrawSimpleAt)
    wigFillInColorBedArray(wigTrack, colorArray, colSize, colorTrack, hvg);

if(trackLoaded && colorTrack->freeItems != NULL)
    colorTrack->freeItems(colorTrack);
}

void wigSetCart(struct track *track, char *dataID, void *dataValue)
    /*	set one of the variables in the wigCart	*/
{
struct wigCartOptions *wigCart;
wigCart = (struct wigCartOptions *) track->extraUiData;

if (sameWord(dataID, MIN_Y))
    wigCart->minY = *((double *)dataValue);
else if (sameWord(dataID, MAX_Y))
    wigCart->maxY = *((double *)dataValue);
}

/*	these two routines unused at this time	*/
#if defined(NOT)
static void wigItemFree(struct wigItem **pEl)
    /* Free up a wigItem. */
{
struct wigItem *el = *pEl;
if (el != NULL)
    {
    /* freeMem(el->name);	DO NOT - this belongs to tg->mapName */
    freeMem(el->db);
    freeMem(el->file);
    freez(pEl);
    }
}

static void wigItemFreeList(struct wigItem **pList)
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
#endif

/*	trackSpans - hash of hashes, first hash is keyed via trackName
 *	the value for key trackName is a hash itself where each element
 *	is a Span found in the data (==zoom level indication)
 *	The key for the second hash is the ascii string for the span
 *	value, and the value at that key is the binary equivalent for
 *	that number.
 */
static struct hash *trackSpans = NULL;	/* hash of hashes */

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
return tg->mapName;
}

/*	NOT used at this time, maybe later	*/
#if defined(NOT)
/*	This is practically identical to sampleUpdateY in sampleTracks.c
 *	In fact is is functionally identical except jkLib functions are
 *	used instead of the actual string functions.  I will consult
 *	with Ryan to see if one of these copies can be removed.
 */
static boolean sameWigGroup(char *name, char *nextName, int lineHeight)
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
#endif

int wigTotalHeight(struct track *tg, enum trackVisibility vis)
/* Wiggle track will use this to figure out the height they use
   as defined in the cart */
{
struct wigCartOptions *wigCart;

wigCart = (struct wigCartOptions *) tg->extraUiData;

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
    tg->lineHeight = max(wigCart->minHeight, wigCart->defaultHeight);

tg->heightPer = tg->lineHeight;
tg->height = tg->lineHeight;

return tg->height + 1;
}

static void wigSetItemData(struct track *tg, struct wigItem *wi,
    struct wiggle *wiggle, struct hash *spans)
/* copy values from *wiggle to *wi, maintain trackSpans hash	*/
{
static char *previousFileName = (char *)NULL;
char spanName[128];
struct hashEl *el;
char *trackName = tg->mapName;

/*	Allocate trackSpans one time only, for all tracks	*/
if (! trackSpans)
    trackSpans = newHash(4);

wi->start = wiggle->chromStart;
wi->end = wiggle->chromEnd;

if ((previousFileName == (char *)NULL) ||
	differentString(previousFileName,wiggle->file))
    {
    if (! fileExists(wiggle->file))
	errAbort("wigSetItemData: can't open file '%s' (%s)",
                         wiggle->file, strerror(errno));
    freez(&previousFileName);
    previousFileName = cloneString(wiggle->file);
    }
wi->file = cloneString(wiggle->file);

wi->span = wiggle->span;
wi->count = wiggle->count;
wi->offset = wiggle->offset;
wi->lowerLimit = wiggle->lowerLimit;
wi->dataRange = wiggle->dataRange;
wi->validCount = wiggle->validCount;
wi->sumData = wiggle->sumData;
wi->sumSquares = wiggle->sumSquares;

/*	see if we have a spans hash for this track already */
el = hashLookup(trackSpans, trackName);
/*	no, then let's start one	*/
if ( el == NULL)
	hashAdd(trackSpans, trackName, spans);
/*	see if this span is already in our hash for this track */
safef(spanName, sizeof(spanName), "%d", wi->span);
el = hashLookup(spans, spanName);
/*	no, then add this span to the spans list for this track */
if ( el == NULL)
	hashAddInt(spans, spanName, wi->span);
}

#ifndef GBROWSE
void ctWigLoadItems(struct track *tg) 
/*	load custom wiggle track data	*/
{
struct customTrack *ct;
char *row[13];
struct lineFile *lf = NULL;
struct wiggle wiggle; 
struct wigItem *wiList = NULL;
int itemsLoaded = 0;
struct hash *spans = NULL;	/* Spans encountered during load */

ct = tg->customPt;

/*	Verify this is a custom track	*/
if (ct == (void *)NULL)
    errAbort("ctWigLoadItems: did not find a custom wiggle track: %s", tg->mapName);

/*	and should *not* be here for a database custom track	*/
if (ct->dbTrack)
    errAbort("ctWigLoadItems: this custom wiggle track is in database: %s", tg->mapName);

/*	Each instance of this LoadItems will create a new spans hash
 *	It will be the value included in the trackSpans hash
 */
spans = newHash(3);
/*	Each row read will be turned into an instance of a wigItem
 *	A growing list of wigItems will be the items list to return
 */
itemsLoaded = 0;

tg->items = wiList;

lf = lineFileOpen(ct->wigFile, TRUE);
while (lineFileChopNextTab(lf, row, ArraySize(row)))
    {

    wiggleStaticLoad(row, &wiggle); 
    /*	we have to do hRangeQuery's job here since we are reading a
     *	file.  We need to be on the correct chromosome, and the data
     *	needs to be in the current view.
     */
    if (sameWord(chromName,wiggle.chrom))
	{
	if ((winStart < wiggle.chromEnd) && (winEnd > wiggle.chromStart))
	    {
	    struct wigItem *wi;
	    ++itemsLoaded;
	    AllocVar(wi);
	    wigSetItemData(tg, wi, &wiggle, spans);
	    slAddHead(&wiList, wi);
	    }	/*	if in viewing window	*/
	}	/*	if in same chromosome	*/
    }	/*	while reading lines	*/

slReverse(&wiList);
tg->items = wiList;
tg->mapsSelf = TRUE;

lineFileClose(&lf);
}
#endif /* GBROWSE */

void wigLoadItems(struct track *tg) 
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
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct wiggle wiggle;
struct wigItem *wiList = NULL;
char *whereNULL = NULL;
int itemsLoaded = 0;
char *previousFileName;
struct hash *spans = NULL;	/* Spans encountered during load */
/*	Check our scale from the global variables that exist in
 *	hgTracks.c - This can give us a guide about which rows to load.
 *	If the scale is more than 1000 bases per pixel, we can try loading
 *	only those rows with Span >= 1000 to see if an appropriate zoom
 *	level exists.
 */
int basesPerPixel = (int)((double)(winEnd - winStart)/(double)insideWidth);
char *span1K = "Span >= 1000 limit 1";
char *spanOver1K = "Span >= 1000";
char whereSpan[64];
int spanMinimum = 1;
char *dbTableName = NULL;
struct trackDb *tdb = NULL; 
int loadStart = winStart, loadEnd = winEnd;

#ifndef GBROWSE
struct customTrack *ct = NULL;
/*	custom tracks have different database	*/
if (tg->customPt != (void *)NULL)
    {
    hFreeConn(&conn);
    conn = sqlCtConn(TRUE);
    ct = tg->customPt;
    dbTableName = ct->dbTableName;
    tdb = ct->tdb;
    }
else
#endif /* GBROWSE */
    {
    dbTableName = tg->mapName;
    tdb = tg->tdb;
    }

/*	Allocate trackSpans one time only	*/
if (! trackSpans)
    trackSpans = newHash(0);

/*	find the minimum span to see if there are actually any data
 *	points in this area at that span.  If there are not, then there
 *	is no data here even if a zoomed view covers this section.
 *	protect against less than 1 with the max(1,minSpan());
 *	This business will fix the problem mentioned in RT #1186
 */

spanMinimum = max(1,
	minSpan(conn, dbTableName, chromName, winStart, winEnd, cart, tdb));

itemsLoaded = 0;
safef(whereSpan, sizeof(whereSpan), "span=%d limit 1", spanMinimum);

sr = hRangeQuery(conn, dbTableName, chromName, loadStart, loadEnd,
    whereSpan, &rowOffset);

while ((row = sqlNextRow(sr)) != NULL)
    ++itemsLoaded;
sqlFreeResult(&sr);

/*	if nothing here, bail out	*/
if (itemsLoaded < 1)
    {
    tg->items = (struct wigItem *)NULL;
    hFreeOrDisconnect(&conn);
    return;
    }

itemsLoaded = 0;
if (basesPerPixel >= 1000)
    {
    sr = hRangeQuery(conn, dbTableName, chromName, loadStart, loadEnd,
	span1K, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	    ++itemsLoaded;
    sqlFreeResult(&sr);
    }
/*	If that worked, excellent, then we have at least another zoom level
 *	So, for our actual load, use spanOver1K to fetch not only the 1K
 *	zooms, but potentially others that may be useful.  This will
 *	save us a huge amount in loaded rows.  On a 250 Mbase chromosome
 *	there would be 256,000 rows at the 1 base level and only
 *	256 rows at the 1K zoom level.  Otherwise, we go back to the
 *	regular query which will give us all rows.
 */
/* JK - Can't we figure out here one, and only one span to load?  This
 * would simplify drawing logic. */
if (itemsLoaded)
    {
    sr = hRangeQuery(conn, dbTableName, chromName, loadStart, loadEnd,
	     spanOver1K, &rowOffset);
    }
else
    {
    sr = hRangeQuery(conn, dbTableName, chromName, loadStart, loadEnd,
	whereNULL, &rowOffset);
    }

/*	Allocate trackSpans one time only	*/
if (! trackSpans)
    trackSpans = newHash(4);
/*	Each instance of this LoadItems will create a new spans hash
 *	It will be the value included in the trackSpans hash
 */
spans = newHash(4);
/*	Each row read will be turned into an instance of a wigItem
 *	A growing list of wigItems will be the items list to return
 */
previousFileName = "";
itemsLoaded = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct wigItem *wi;
    ++itemsLoaded;
    wiggleStaticLoad(row + rowOffset, &wiggle);
    AllocVar(wi);
    wigSetItemData(tg, wi, &wiggle, spans);
    slAddHead(&wiList, wi);
    }

sqlFreeResult(&sr);
hFreeOrDisconnect(&conn);

slReverse(&wiList);
tg->items = wiList;
}	/*	wigLoadItems()	*/

static void wigFreeItems(struct track *tg) {
#if defined(DEBUG)
safef(dbgMsg, DBGMSGSZ, "I haven't seen wigFreeItems ever called ?");
wigDebugPrint("wigFreeItems");
#endif
}

struct preDrawElement * initPreDraw(int width, int *preDrawSize,
	int *preDrawZero)
/*	initialize a preDraw array of size width	*/
{
struct preDrawElement *preDraw = (struct preDrawElement *)NULL;
int i = 0;

/*	we are going to keep an array that is three times the size of
 *	the screen to allow a screen full on either side of the visible
 *	region.  Those side screens can be used in the smoothing
 *	operation so there won't be any discontinuity at the visible screen
 *	boundaries.
 */
*preDrawSize = width * 3;
*preDrawZero = width / 3;
preDraw = (struct preDrawElement *) needMem ((size_t)
		*preDrawSize * sizeof(struct preDrawElement));
for (i = 0; i < *preDrawSize; ++i)
    {
    preDraw[i].count = 0;
    preDraw[i].max = -1.0e+300;
    preDraw[i].min = 1.0e+300;
    }
return preDraw;
}

void preDrawWindowFunction(struct preDrawElement *preDraw, int preDrawSize,
	enum wiggleWindowingEnum windowingFunction)
/*	apply windowing function to the values in preDraw array	*/
{
int i;

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
}

void preDrawSmoothing(struct preDrawElement *preDraw, int preDrawSize,
    enum wiggleSmoothingEnum smoothingWindow)
/*	apply smoothing function to preDraw array	*/
{
/*	Are we perhaps doing smoothing ?  smoothingWindow is 1 off due
 *	to enum funny business in inc/hui.h and lib/hui.c 	*/
if (smoothingWindow > 0)
    {
    int winSize = smoothingWindow + 1; /* enum funny business */
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
}

double preDrawLimits(struct preDrawElement *preDraw, int preDrawZero,
    int width, double *overallUpperLimit, double *overallLowerLimit)
/*	scan preDraw array and determine graph limits */
{
int i;

/*	Just in case they haven't been initialized before	*/
*overallUpperLimit = -1.0e+300;
*overallLowerLimit = 1.0e+300;
for (i = preDrawZero; i < preDrawZero+width; ++i)
    {
    /*	count is non-zero meaning valid data exists here	*/
    if (preDraw[i].count)
	{
	if (preDraw[i].max > *overallUpperLimit)
	    *overallUpperLimit = preDraw[i].max;
	if (preDraw[i].min < *overallLowerLimit)
	    *overallLowerLimit = preDraw[i].min;
	}
    }
return (overallUpperLimit - overallLowerLimit);
}

double preDrawAutoScale(struct preDrawElement *preDraw, int preDrawZero,
    int width, enum wiggleScaleOptEnum autoScale,
    double *overallUpperLimit, double *overallLowerLimit,
    double *graphUpperLimit, double *graphLowerLimit,
    double *overallRange, double *epsilon, int lineHeight,
    double maxY, double minY)
/*	if autoScaling, scan preDraw array and determine limits */
{
double graphRange;

if (autoScale == wiggleScaleAuto)
    {
    int i;

    *overallUpperLimit = -1.0e+300;	/* reset limits for auto scale */
    *overallLowerLimit = 1.0e+300;
    for (i = preDrawZero; i < preDrawZero+width; ++i)
	{
	/*	count is non-zero meaning valid data exists here	*/
	if (preDraw[i].count)
	    {
	    if (preDraw[i].smooth > *overallUpperLimit)
		*overallUpperLimit = preDraw[i].smooth;
	    if (preDraw[i].smooth < *overallLowerLimit)
		*overallLowerLimit = preDraw[i].smooth;
	    }
	}
    *overallRange = *overallUpperLimit - *overallLowerLimit;
    if (*overallRange == 0.0)
	{
	if (*overallUpperLimit > 0.0)
	    {
	    *graphUpperLimit = *overallUpperLimit;
	    *graphLowerLimit = 0.0;
	    } else if (*overallUpperLimit < 0.0) {
	    *graphUpperLimit = 0.0;
	    *graphLowerLimit = *overallUpperLimit;
	    } else {
	    *graphUpperLimit = 1.0;
	    *graphLowerLimit = -1.0;
	    }
	    graphRange = *graphUpperLimit - *graphLowerLimit;
	} else {
	*graphUpperLimit = *overallUpperLimit;
	*graphLowerLimit = *overallLowerLimit;
	}
    } else {
	*graphUpperLimit = maxY;
	*graphLowerLimit = minY;
    }
graphRange = *graphUpperLimit - *graphLowerLimit;
*epsilon = graphRange / lineHeight;
return(graphRange);
}

Color * allocColorArray(struct preDrawElement *preDraw, int width,
    int preDrawZero, char *colorTrack, struct track *tg, struct hvGfx *hvg)
/*	allocate and fill in a coloring array based on another track */
{
int x1;
Color *colorArray = NULL;       /*	Array of pixels to be drawn.	*/

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
if(colorTrack != NULL) 
    {
    struct track *cTrack = hashMustFindVal(trackHash, colorTrack);

    wigFillInColorArray(tg, hvg, colorArray, width, cTrack);
    }

return colorArray;
}

void graphPreDraw(struct preDrawElement *preDraw, int preDrawZero, int width,
    struct track *tg, struct hvGfx *hvg, int xOff, int yOff,
    double graphUpperLimit, double graphLowerLimit, double graphRange,
    double epsilon, Color *colorArray, enum trackVisibility vis,
    enum wiggleGraphOptEnum lineBar)
/*	graph the preDraw array */
{
int x1;
int lastRealX = -1;
int lastRealY = -1;

/*	right now this is a simple pixel by pixel loop.  Future
 *	enhancements could draw boxes where pixels
 *	are all the same height in a run.
 */
for (x1 = 0; x1 < width; ++x1)
    {
    Color drawColor = hvGfxFindColorIx(hvg, 0, 0, 0);
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
		hvGfxBox(hvg, x1+xOff, yOff+boxTop, 1, boxHeight, drawColor);
		}
	    else
		{	/*	draw a 3 pixel height box	*/
		hvGfxBox(hvg, x1+xOff, yPointGraph, 1, 3, drawColor);
		}
	    }	/*	vis == tvFull	*/
	else if (vis == tvDense)
	    {
	    double grayValue;
	    int grayIndex;
	    /* honor the viewLimits, data below is white, data above is black */
	    grayValue = max(dataValue,graphLowerLimit);
	    grayValue = min(grayValue,graphUpperLimit);
	    grayIndex = ((grayValue-graphLowerLimit)/graphRange)*MAX_WIG_VALUE;

	    drawColor =
		tg->colorShades[grayInRange(grayIndex, 0, MAX_WIG_VALUE)];

	    boxHeight = tg->lineHeight;
	    hvGfxBox(hvg, x1+xOff, yOff, 1,
		boxHeight, drawColor);
	    }	/*	vis == tvDense	*/
	lastRealX = xOff + x1;
	lastRealY = yOff + y1;
	}	/*	if (preDraw[].count)	*/
    }	/*	for (x1 = 0; x1 < width; ++x1)	*/
}	/*	graphPreDraw()	*/

void drawZeroLine(enum trackVisibility vis,
    enum wiggleGridOptEnum horizontalGrid,
    double graphUpperLimit, double graphLowerLimit,
    struct hvGfx *hvg, int xOff, int yOff, int width, int lineHeight)
/*	draw a line at y=0 on the graph	*/
{
/*	Do we need to draw a zero line ?
 *	This is to be generalized in the future to allow horizontal grid
 *	lines, perhaps user specified to indicate thresholds.
 */
if ((vis == tvFull) && (horizontalGrid == wiggleHorizontalGridOn))
    {
    Color black = hvGfxFindColorIx(hvg, 0, 0, 0);
    int x1, x2, y1, y2;

    x1 = xOff;
    x2 = x1 + width;

    /*	Let's see if the zero line can be drawn	*/
    if ((0.0 <= graphUpperLimit) && (0.0 >= graphLowerLimit))
	{
	int zeroOffset;

	zeroOffset = (int)((graphUpperLimit * lineHeight) /
			(graphUpperLimit - graphLowerLimit));
	y1 = yOff + zeroOffset;
	if (y1 >= (yOff + lineHeight)) y1 = yOff + lineHeight - 1;
	y2 = y1;
	hvGfxLine(hvg,x1,y1,x2,y2,black);
	}

    }	/*	drawing horizontalGrid	*/
}	/*	drawZeroLine()	*/

void drawArbitraryYLine(enum trackVisibility vis,
    enum wiggleGridOptEnum horizontalGrid,
    double graphUpperLimit, double graphLowerLimit,
    struct hvGfx *hvg, int xOff, int yOff, int width, int lineHeight,
    double yLineMark, double graphRange, enum wiggleYLineMarkEnum yLineOnOff)
/*	draw a line at y=yLineMark on the graph	*/
{
/*	Optionally, a user requested Y marker line at some value */
if ((vis == tvFull) && (yLineOnOff == wiggleYLineMarkOn))
    {
    int x1, x2, y1, y2;
    Color black = hvGfxFindColorIx(hvg, 0, 0, 0);

    x1 = xOff;
    x2 = x1 + width;

    /*	Let's see if this marker line can be drawn	*/
    if ((yLineMark <= graphUpperLimit) && (yLineMark >= graphLowerLimit))
	{
	int Offset;

	Offset = lineHeight * ((graphUpperLimit - yLineMark)/graphRange);
	y1 = yOff + Offset;
	if (y1 >= (yOff + lineHeight)) y1 = yOff + lineHeight - 1;
	y2 = y1;
	hvGfxLine(hvg,x1,y1,x2,y2,black);
	}

    }	/*	drawing y= line marker	*/
}	/*	drawArbitraryYLine()	*/

void wigMapSelf(struct track *tg, struct hvGfx *hvg, int seqStart, int seqEnd,
    int xOff, int yOff, int width)
/*	if self mapping, create the mapping box	*/
{
/*	Map this wiggle area if we are self mapping	*/
if (tg->mapsSelf)
    {
    char *itemName;
#ifndef GBROWSE
    if (tg->customPt)
	{
	struct customTrack *ct = tg->customPt;
	itemName = (char *)needMem(128 * sizeof(char));
	safef(itemName, 128, "%s %s", ct->wigFile, tg->mapName);
	}
    else
#endif /* GBROWSE */
	itemName = cloneString(tg->mapName);

    mapBoxHc(hvg, seqStart, seqEnd, xOff, yOff, width, tg->height, tg->mapName, 
            itemName, NULL);
    freeMem(itemName);
    }
}

int wigFindSpan(struct track *tg, double basesPerPixel)
/* Return span to use at this scale */
{
int usingDataSpan = 1;
int minimalSpan = 100000000;	/*	a lower limit safety check */
struct hashEl *el, *elList;

/*	Take a look through the potential spans, and given what we have
 *	here for basesPerPixel, pick the largest usingDataSpan that is
 *	not greater than the basesPerPixel
 */
el = hashLookup(trackSpans, tg->mapName);	/*  What Spans do we have */
elList = hashElListHash(el->val);		/* Our pointer to spans hash */
for (el = elList; el != NULL; el = el->next)
    {
    int Span;
    Span = ptToInt(el->val);
    if ((Span < basesPerPixel) && (Span > usingDataSpan))
	usingDataSpan = Span;
    if (Span < minimalSpan)
	minimalSpan = Span;
    }
hashElFreeList(&elList);

/*	There may not be a span of 1, use whatever is lowest	*/
if (minimalSpan > usingDataSpan)
    usingDataSpan = minimalSpan;

return usingDataSpan;
}


static void wigDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw wiggle items that resolve to doing a box for each pixel. */
{
struct wigItem *wi;
double pixelsPerBase = scaleForPixels(width);
double basesPerPixel = 1.0;
int itemCount = 0;
char currentFile[PATH_LEN];
int wibFH = 0;		/*	file handle to binary file */
struct wigCartOptions *wigCart = tg->extraUiData;
enum wiggleGraphOptEnum lineBar = wigCart->lineBar;
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
int usingDataSpan = 1;		/* will become larger if possible */

if (tg->items == NULL)
    return;

currentFile[0] = '\0';

if (pixelsPerBase > 0.0)
    basesPerPixel = 1.0 / pixelsPerBase;

/*	width - width of drawing window in pixels
 *	pixelsPerBase - pixels per base
 *	basesPerPixel - calculated as 1.0/pixelsPerBase
 */
itemCount = 0;

preDraw = initPreDraw(width, &preDrawSize, &preDrawZero);
usingDataSpan = wigFindSpan(tg, basesPerPixel);

/*	walk through all the data and prepare the preDraw array	*/
for (wi = tg->items; wi != NULL; wi = wi->next)
    {
    size_t bytesRead;		/* to check fread being OK */
    int dataOffset = 0;		/*	within data block during drawing */

    ++itemCount;


    /*	Now that we know what Span to draw, see if this item should be
     *	drawn at all.
     */
    if (usingDataSpan == wi->span)
	{
	/*	Check our data file, see if we need to open a new one */
	if (differentString(currentFile,""))
	    {
	    if (differentString(currentFile,wi->file))
		{
		if (wibFH > 0)
		    {
		    close(wibFH);
		    freeMem(currentFile);
		    }
		strncpy(currentFile, wi->file, PATH_LEN);
		currentFile[PATH_LEN-1] = '\0';
		wibFH = open(currentFile, O_RDONLY);
		if (-1 == wibFH)
		    errAbort("openWibFile: failed to open %s", currentFile);
		}
	    }
	else
	    {
	    strncpy(currentFile, wi->file, PATH_LEN-1);
	    currentFile[PATH_LEN-1] = '\0';
	    wibFH = open(currentFile, O_RDONLY);
	    if (-1 == wibFH)
		errAbort("openWibFile: failed to open %s", currentFile);
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
	/*	let's check end point screen coordinates.  If they are
 	 *	the same, then this entire data block lands on one pixel,
 	 *	no need to walk through it, just use the block's specified
 	 *	max/min.  It is OK if these end up + or -, we do want to
 	 *	keep track of pixels before and after the screen for
 	 *	later smoothing operations
	 */
	x1 = (wi->start - seqStart) * pixelsPerBase;
	x2 = ((wi->start+(wi->count * usingDataSpan))-seqStart) * pixelsPerBase;

	if (x2 > x1) 
	    {
	    unsigned char *readData;	/* the bytes read in from the file */
	    lseek(wibFH, wi->offset, SEEK_SET);
	    readData = (unsigned char *) needMem((size_t) (wi->count + 1));
	    bytesRead = read(wibFH, readData,
		(size_t) wi->count * (size_t) sizeof(unsigned char));
	    /*	walk through all the data in this block	*/
	    for (dataOffset = 0; dataOffset < wi->count; ++dataOffset)
		{
		unsigned char datum = readData[dataOffset];
		if (datum != WIG_NO_DATA)
		    {
		    x1 = ((wi->start-seqStart) + (dataOffset * usingDataSpan)) * pixelsPerBase;
		    x2 = x1 + (usingDataSpan * pixelsPerBase);
		    for (i = x1; i <= x2; ++i)
			{
			int xCoord = preDrawZero + i;
			if ((xCoord >= 0) && (xCoord < preDrawSize))
			    {
			    double dataValue =
			       BIN_TO_VALUE(datum,wi->lowerLimit,wi->dataRange);

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
	    freeMem(readData);
	    } 
	else 
	    {	/*	only one pixel for this block of data */
	    int xCoord = preDrawZero + x1;
	    /*	if the point falls within our array, record it.
	     *	the (wi->validCount > 0) is a safety check.  It
	     *	should always be true unless the data was
	     *	prepared incorrectly.
	     */
	    if ((wi->validCount > 0) && (xCoord >= 0) && (xCoord < preDrawSize))
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
		}
	    }
	}	/*	Draw if span is correct	*/
    }	/*	for (wi = tg->items; wi != NULL; wi = wi->next)	*/
if (wibFH > 0)
    {
    close(wibFH);
    wibFH = 0;
    }

/*	now we are ready to draw.  Each element in the preDraw[] array
 *	cooresponds to a single pixel on the screen
 */

preDrawWindowFunction(preDraw, preDrawSize, wigCart->windowingFunction);
preDrawSmoothing(preDraw, preDrawSize, wigCart->smoothingWindow);
overallRange = preDrawLimits(preDraw, preDrawZero, width,
    &overallUpperLimit, &overallLowerLimit);
graphRange = preDrawAutoScale(preDraw, preDrawZero, width, wigCart->autoScale,
    &overallUpperLimit, &overallLowerLimit,
    &graphUpperLimit, &graphLowerLimit,
    &overallRange, &epsilon, tg->lineHeight, wigCart->maxY, wigCart->minY);

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
    epsilon, colorArray, vis, lineBar);

drawZeroLine(vis, wigCart->horizontalGrid, graphUpperLimit, graphLowerLimit,
    hvg, xOff, yOff, width, tg->lineHeight);

drawArbitraryYLine(vis, wigCart->yLineOnOff, graphUpperLimit, graphLowerLimit,
    hvg, xOff, yOff, width, tg->lineHeight, wigCart->yLineMark, graphRange,
    wigCart->yLineOnOff);

wigMapSelf(tg, hvg, seqStart, seqEnd, xOff, yOff, width);

freez(&colorArray);
freeMem(preDraw);
}	/*	wigDrawItems()	*/

void wigFindItemLimits(void *items,
    double *graphUpperLimit, double *graphLowerLimit)
/*	find upper and lower limits of graphed items (wigItem)	*/
{
struct wigItem *wi;
*graphUpperLimit = -1.0e+300;
*graphLowerLimit = 1.0e+300;

for (wi = items; wi != NULL; wi = wi->next)
    {
    if (wi->graphUpperLimit > *graphUpperLimit)
	*graphUpperLimit = wi->graphUpperLimit;
    if (wi->graphLowerLimit < *graphLowerLimit)
	*graphLowerLimit = wi->graphLowerLimit;
    }
}

void wigLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
/*	drawing left labels	*/
{
int fontHeight = tl.fontHeight+1;
int centerOffset = 0;
double lines[2];	/*	lines to label	*/
int numberOfLines = 1;	/*	at least one: 0.0	*/
int i;			/*	loop counter	*/
struct wigCartOptions *wigCart;

wigCart = (struct wigCartOptions *) tg->extraUiData;
lines[0] = 0.0;
lines[1] = wigCart->yLineMark;
if (wigCart->yLineOnOff == wiggleYLineMarkOn)
    ++numberOfLines;

if (withCenterLabels)
	centerOffset = fontHeight;

/*	We only do Dense and Full	*/
if (tg->visibility == tvDense)
    {
    hvGfxTextRight(hvg, xOff, yOff+centerOffset, width - 1, height-centerOffset,
	tg->ixColor, font, tg->shortLabel);
    }
else if (tg->visibility == tvFull)
    {
    int centerLabel = (height/2)-(fontHeight/2);
    int labelWidth = 0;

    /* track label is centered in the whole region */
    hvGfxText(hvg, xOff, yOff+centerLabel, tg->ixColor, font, tg->shortLabel);
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
	if (wigCart->bedGraph)
	    wigBedGraphFindItemLimits(tg->items,
		&graphUpperLimit, &graphLowerLimit);
	else
	    wigFindItemLimits(tg->items,
		&graphUpperLimit, &graphLowerLimit);

	/*  In areas where there is no data, these limits do not change */
	if (graphUpperLimit < graphLowerLimit)
	    {
	    double d = graphLowerLimit;
	    graphLowerLimit = graphUpperLimit;
	    graphUpperLimit = d;
            if (hvg->rc)
                {
                safef(upper, sizeof(upper), " %c No data", upperTic);
                safef(lower, sizeof(lower), "_ No data");
                }
            else
                {
                safef(upper, sizeof(upper), "No data %c", upperTic);
                safef(lower, sizeof(lower), "No data _");
                }
	    zeroOK = FALSE;
	    }
	else
	    {
            if (hvg->rc)
                {
                safef(upper, sizeof(upper), "%c %g", upperTic, graphUpperLimit);
                safef(lower, sizeof(lower), "_ %g", graphLowerLimit);
                }
            else
                {
                safef(upper, sizeof(upper), "%g %c", graphUpperLimit, upperTic);
                safef(lower, sizeof(lower), "%g _", graphLowerLimit);
                }
	    }
	drawColor = tg->ixColor;
	if (graphUpperLimit < 0.0) drawColor = tg->ixAltColor;
	hvGfxTextRight(hvg, xOff, yOff, width - 1, fontHeight, drawColor,
	    font, upper);
	drawColor = tg->ixColor;
	if (graphLowerLimit < 0.0) drawColor = tg->ixAltColor;
	hvGfxTextRight(hvg, xOff, yOff+height-fontHeight, width - 1, fontHeight,
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

		drawColor = hvGfxFindColorIx(hvg, 0, 0, 0);
		offset = centerOffset +
		    (int)(((graphUpperLimit - lineValue) *
				(height - centerOffset)) /
			(graphUpperLimit - graphLowerLimit));
		/*	reusing the lower string here	*/
                if (hvg->rc)
                    safef(lower, sizeof(lower), "- %g", ((i == 0) ? 0.0 : lineValue));
                else
                    safef(lower, sizeof(lower), "%g -", ((i == 0) ? 0.0 : lineValue));
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
		    hvGfxTextRight(hvg, xOff, yOff+offset-(fontHeight/2),
			width - 1, fontHeight, drawColor, font, lower);
		    }
		}	/*	drawing a zero label	*/
	    }	/*	drawing 0.0 and perhaps yLineMark	*/
	}	/* if (height >= (3 * fontHeight))	*/
    }	/*	if (tg->visibility == tvFull)	*/
}	/* wigLeftLabels */

/* Make track group for wig multiple alignment.
 *	WARNING ! - track->visibility is merely the default value
 *	from the trackDb entry at this time.  It will be set after this
 *	 by hgTracks from its cart UI setting.  When called in
 *	 TotalHeight it will then be the requested visibility.
 */
void wigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
{
int defaultHeight;	/*	truncated by limits	*/
double minY;	/*	from trackDb or cart, requested minimum */
double maxY;	/*	from trackDb or cart, requested maximum */
double tDbMinY;	/*	from trackDb type line, the absolute minimum */
double tDbMaxY;	/*	from trackDb type line, the absolute maximum */
double yLineMark;	/*	from trackDb or cart */
struct wigCartOptions *wigCart;
int maxHeight = atoi(DEFAULT_HEIGHT_PER);
int minHeight = MIN_HEIGHT_PER;

AllocVar(wigCart);

/*	These Fetch functions look for variables in the cart bounded by
 *	limits specified in trackDb or returning defaults
 */
wigCart->lineBar = wigFetchGraphType(tdb, (char **) NULL);
wigCart->horizontalGrid = wigFetchHorizontalGrid(tdb, (char **) NULL);

wigCart->autoScale = wigFetchAutoScale(tdb, (char **) NULL);
wigCart->windowingFunction = wigFetchWindowingFunction(tdb, (char **) NULL);
wigCart->smoothingWindow = wigFetchSmoothingWindow(tdb, (char **) NULL);

wigFetchMinMaxPixels(tdb, &minHeight, &maxHeight, &defaultHeight);
wigFetchYLineMarkValue(tdb, &yLineMark);
wigCart->yLineMark = yLineMark;
wigCart->yLineOnOff = wigFetchYLineMark(tdb, (char **) NULL);

wigCart->maxHeight = maxHeight;
wigCart->defaultHeight = defaultHeight;
wigCart->minHeight = minHeight;

if(trackDbSetting(tdb, "wigColorBy") != NULL)
    wigCart->colorTrack = trackDbSetting(tdb, "wigColorBy");

wigFetchMinMaxY(tdb, &minY, &maxY, &tDbMinY, &tDbMaxY, wordCount, words);
track->minRange = minY;
track->maxRange = maxY;

wigCart->minY = track->minRange;
wigCart->maxY = track->maxRange;
wigCart->bedGraph = FALSE;	/*	signal to left labels	*/

track->loadItems = wigLoadItems;
track->freeItems = wigFreeItems;
track->drawItems = wigDrawItems;
track->itemName = wigName;
track->mapItemName = wigName;
track->totalHeight = wigTotalHeight;
track->itemHeight = tgFixedItemHeight;

track->itemStart = tgItemNoStart;
track->itemEnd = tgItemNoEnd;
/*	the wigMaf parent will turn mapsSelf off	*/
track->mapsSelf = TRUE;
track->extraUiData = (void *) wigCart;
track->colorShades = shadesOfGray;
track->drawLeftLabels = wigLeftLabels;
/*	the lfSubSample type makes the image map function correctly */
track->subType = lfSubSample;     /*make subType be "sample" (=2)*/

}	/*	wigMethods()	*/
