/* sampleTracks - Ryan Weber's wiggle tracks. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "sample.h"

int sampleUpdateY( char *name, char *nextName, int lineHeight )
/* only increment height when name root (minus the period if
 * there is one) is different from previous one.
  *This assumes that the entries are sorted by name as they would
  *be if loaded by hgLoadSample*/
{
int ret;
char *tempstr = NULL;
char *tempstr2 = NULL;

tempstr = cloneString( name );
if( strstr( name, "." ) != NULL )
   strtok( tempstr, "." );

tempstr2 = cloneString( nextName );
if( strstr( nextName, "." ) != NULL )
   strtok( tempstr2, "." );

if( !sameString( tempstr, tempstr2 )) 
   ret = lineHeight; 
else 
   ret = 0;

freeMem(tempstr);
freeMem(tempstr2);

return(ret);
}

void samplePrintYAxisLabel( struct hvGfx *hvg, int y, struct track *track, char *labelString,
        double min0, double max0 )
/*print a label for a horizontal y-axis line*/
{
double tmp;
int fontHeight = tl.fontHeight;
double ymin = y - (track->heightPer / 2) + fontHeight;
int itemHeight0 = track->itemHeight(track, track->items);
int inWid = insideX-gfxBorder*3;

tmp = -whichSampleBin( atof(labelString), min0, max0, 1000 );
tmp = (int)((double)ymin+((double)tmp)*(double)track->heightPer/1000.0+(double)track->heightPer)-fontHeight/2.0;
if( !withCenterLabels ) tmp -= fontHeight;
hvGfxTextRight(hvg, gfxBorder, tmp, inWid-1, itemHeight0, track->ixColor, tl.font, labelString );
}


static int sampleTotalHeight(struct track *tg, 
	enum trackVisibility vis)
/* Wiggle track will use this to figure out the height they use
as defined in the cart */
{
struct slList *item;
int lines = 0;

int heightFromCart;
char o1[128];

safef( o1, 128, "%s.heightPer", tg->mapName);

heightFromCart = atoi(cartUsualString(cart, o1, "50"));

tg->lineHeight = max(tl.fontHeight+1, heightFromCart);
tg->heightPer = tg->lineHeight - 1;
switch (vis)
    {
    case tvFull:
	lines = 1;
	for (item = tg->items; item != NULL; item = item->next)
	    {
	    if( item->next != NULL )
		if( sampleUpdateY( tg->itemName(tg, item), tg->itemName(tg, item->next), 1 ))
		    lines++;
	    }
	tg->height = lines * tg->lineHeight;
	break;
    case tvPack:
    case tvSquish:
        errAbort("Sorry can't handle pack in sampleTotalHeight (%s)", tg->mapName);
	break;
    case tvDense:
	tg->height = tg->lineHeight;
	break;
    default:
        break;
    }
return tg->height;
}

int consTotalHeight(struct track *tg, enum trackVisibility vis)
/* Human/mouse conservation total height. */
{
if( vis == tvDense )
    return tg->height = tg->lineHeight = 10;
else
    return sampleTotalHeight(tg, vis); 
}

struct linkedFeatures *lfFromSample(struct sample *sample)
/* Return a linked feature from a full sample (wiggle) track. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf, *sfList = NULL;
int grayIx = grayInRange(sample->score, 0, 1000);
int start, i;
unsigned *X = sample->samplePosition;
int *Y = sample->sampleHeight;
unsigned sampleCount = sample->sampleCount;

assert(X != NULL && Y != NULL && sampleCount > 0);
AllocVar(lf);
lf->grayIx = grayIx;
strncpy(lf->name, sample->name, sizeof(lf->name));
lf->orientation = orientFromChar(sample->strand[0]);

for (i=0; i<sampleCount; ++i)
    {
    AllocVar(sf);
    start = X[i] + sample->chromStart;
    sf->start = start;

    if( Y[i] < 0 )      /*hack for negative values not loading correctly*/
        sf->end = start;
    else if( Y[i] == 0 )
        sf->end = start + 1;
    else
        sf->end = start + Y[i];

    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
lf->components = sfList;
linkedFeaturesBoundsAndGrays(lf);
lf->start = sample->chromStart;
lf->end = sample->chromEnd;
return lf;
}


int whichSampleBin( double num, double thisMin, double thisMax, 
	double binCount )
/* Get bin value from num. */
{
return (num - thisMin) * binCount / (thisMax - thisMin);
}

double whichSampleNum( double bin, double thisMin, double thisMax, 
	double binCount )
/* gets range nums. from bin values*/
{
return( (thisMax - thisMin) / binCount * bin + thisMin );
}


int basePositionToXAxis( int base, int seqStart, int seqEnd, int
                width, int xOff  ) 
{
double scale = scaleForPixels(width);
double x1 = round((double)((int)base-seqStart)*scale) + xOff; 
return(x1);
}

int humMusZoomLevel( void )
{
int zoom1 = 80000, zoom2 = 5000, zoom3 = 300; /* bp per data point */      
int pixPerBase = (winEnd - winStart)/ tl.picWidth;
if(pixPerBase >= zoom1)
    return(1);
else if( pixPerBase >= zoom2 ) 
    return(2);
else if(pixPerBase >= zoom3)
    return(3);
else    
    return(0);
}


static void drawWiggleHorizontalLine( struct hvGfx *hvg, 
	double where, double min0, double max0, 
	int binCount, int y, double hFactor, int heightPer, 
	Color lineColor )
/* Draws a blue horizontal line on a wiggle track at a specified
 * location based on the range and number of bins*/
{
int bin;
double y1;
bin = -whichSampleBin( where, min0, max0, binCount);
y1 = (int)((double)y+((double)bin)*hFactor+(double)heightPer);
hvGfxBox(hvg, 0, y1, hvg->width, 1, lineColor);
}

static void wiggleLinkedFeaturesDraw(struct track *tg, 
    int seqStart, int seqEnd,
    struct hvGfx *hvg, int xOff, int yOff, int width, 
    MgFont *font, Color color, enum trackVisibility vis)
/* Currently this routine is adapted from Terry's 
 * linkedFeatureSeriesDraw() routine.
 * It is called for 'sample' tracks as specified in the trackDb.ra.
 * and it looks at the cart to decide whether to interpolate, fill blocks,
 * and use anti-aliasing.*/
{
int i;
struct linkedFeatures *lf;
struct simpleFeature *sf;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2;
boolean isFull = (vis == tvFull);
Color bColor = tg->ixAltColor;
double scale = scaleForPixels(width);
int prevX = -1;
int gapPrevX = -1;
double prevY = -1;
double y1 = -1, y2;
int ybase;
int sampleX, sampleY; /* A sample in sample coordinates. 
                       * Sample X coordinate is chromosome coordinate.
		       * Sample Y coordinate is usually 0-1000 */
int binCount = 1.0/tg->scaleRange;   /* Maximum sample Y coordinate. */
int bin;	      /* Sample Y coordinates are first converted to
                       * bin coordinates, and then to pixels.  I'm not
		       * totally sure why.  */



int currentX, currentXEnd, currentWidth;

int leftSide, rightSide;

int noZoom = 1;
enum wiggleOptEnum wiggleType;
char *interpolate = NULL;
char *aa = NULL; 
boolean antiAlias = FALSE;
int fill; 
int lineGapSize;
double min0, max0;

char o1[128]; /* Option 1 - linear interp */
char o2[128]; /* Option 2 - anti alias */
char o3[128]; /* Option 3 - fill */
char o4[128]; /* Option 4 - minimum vertical range cutoff of plot */	
char o5[128]; /* Option 5 - maximum vertical range cutoff of plot */
char o6[128]; /* Option 6 - max gap where interpolation is still done */
char cartStr[64];
char *fillStr;

double hFactor;
double minRange, maxRange;
double minRangeCutoff, maxRangeCutoff;


Color gridColor = hvGfxFindRgb(hvg, &guidelineColor); /* for horizontal lines*/

lf=tg->items;    
if(lf==NULL) return;

//take care of cart options
safef( o1, 128,"%s.linear.interp", tg->mapName);
safef( o2, 128, "%s.anti.alias", tg->mapName);
safef( o3, 128,"%s.fill", tg->mapName);
safef( o4, 128,"%s.min.cutoff", tg->mapName);
safef( o5, 128,"%s.max.cutoff", tg->mapName);
safef( o6, 128,"%s.interp.gap", tg->mapName);

interpolate = cartUsualString(cart, o1, "Linear Interpolation");
wiggleType = wiggleStringToEnum(interpolate);
aa = cartUsualString(cart, o2, "on");
antiAlias = sameString(aa, "on");

//don't fill gcPercent track by default (but fill others)
if(sameString( tg->mapName, "pGC") && sameString(database,"zooHuman3"))
{
    fillStr = cartUsualString(cart, o3, "0");
}
else
{
    fillStr = cartUsualString(cart, o3, "1");
}
fill = atoi(fillStr);
cartSetString(cart, o3, fillStr );

//the 0.1 is so the label doesn't get truncated with integer valued user input min
//display range.
minRangeCutoff = max( atof(cartUsualString(cart,o4,"0.0"))-0.1, tg->minRange );
maxRangeCutoff = min( atof(cartUsualString(cart,o5,"1000.0"))+0.1, tg->maxRange);

lineGapSize = atoi(cartUsualString(cart, o6, "200"));

//update cart settings to reflect truncated range cutoff values
cartSetString( cart, "win", "F" );
safef( cartStr, 64, "%g", minRangeCutoff );
cartSetString( cart, o4, cartStr );
safef( cartStr, 64, "%g", maxRangeCutoff );
cartSetString( cart, o5, cartStr );

heightPer = tg->heightPer+1;
hFactor = (double)heightPer*tg->scaleRange;

//errAbort( "min=%g, max=%g\n", minRangeCutoff, maxRangeCutoff );


if( sameString( tg->mapName, "zoo" ) || sameString( tg->mapName, "zooNew" ) )
    binCount = binCount - 100;    //save some space at top, between each zoo species

minRange = whichSampleBin( minRangeCutoff, tg->minRange, tg->maxRange, binCount );
maxRange = whichSampleBin( maxRangeCutoff, tg->minRange, tg->maxRange, binCount );

//errAbort( "(%g,%g) cutoff=(%g,%g)\n", tg->minRange, tg->maxRange, minRangeCutoff, maxRangeCutoff );


if( sameString( tg->mapName, "zoo" ) || sameString( tg->mapName, "zooNew" ) )
    {
    /*Always interpolate zoo track (since gaps are explicitly defined*/
    lineGapSize = -1;
    }
else if( tg->minRange == 0 && tg->maxRange == 8 )    //range for all L-score tracks
    {
    if( isFull )
    {
        min0 = whichSampleNum( minRange, tg->minRange, tg->maxRange, binCount );
        max0 = whichSampleNum( maxRange, tg->minRange, tg->maxRange,  binCount );
        for( i=1; i<=6; i++ )
            drawWiggleHorizontalLine(hvg, (double)i, min0, max0,
	            binCount, y, hFactor, heightPer, gridColor );
        }
    }

for(lf = tg->items; lf != NULL; lf = lf->next) 
    {
    gapPrevX = -1;
    prevX = -1;
    ybase = (int)((double)y+hFactor+(double)heightPer);


    for (sf = lf->components; sf != NULL; sf = sf->next)
	{
	sampleX = sf->start;
	sampleY = sf->end - sampleX;	// Stange encoding but so it is. 
					// It is to deal with the fact that
					// for a BED: sf->end = sf->start + length
					// but in our case length = height (or y-value)
					// so to recover height we take
					// height = sf->end - sf->start.
					// Otherwise another sf variable would 
					// be needed.
 

	/*mapping or sequencing gap*/
	if (sampleY == 0)
	    {
	    bin = -whichSampleBin( (int)((maxRange - minRange)/5.0+minRange), 
	    	minRange, maxRange, binCount );
	    y1 = (int)((double)y+((double)bin)* hFactor+(double)heightPer);
	    if( gapPrevX >= 0 )
		drawScaledBox(hvg, sampleX, gapPrevX, scale, 
			xOff, (int)y1, (int)(.10*heightPer), shadesOfGray[2]);
	    gapPrevX = sampleX;
	    prevX = -1; /*connect next point with gray bar too*/
	    continue;
	    }
	if (sampleY > maxRange)
	    sampleY = maxRange;
	if (sampleY < minRange)
	    sampleY = minRange;
	bin = -whichSampleBin( sampleY, minRange, maxRange, binCount );


	x1 = round((double)(sampleX-winStart)*scale) + xOff;
	y1 = (int)((double)y+((double)bin)* hFactor+(double)heightPer);

	

	if (prevX > 0)
	    {
	    y2 = prevY;
	    x2 = round((double)(prevX-winStart)*scale) + xOff;
	    if( wiggleType == wiggleLinearInterpolation ) 
	    /*connect samples*/
		{
		if( lineGapSize < 0 || prevX - sampleX <= lineGapSize )   
		    /*don't interpolate over large gaps*/
		    {
		    if (fill)
			hvGfxFillUnder(hvg, x1,y1, x2,y2, ybase, bColor);
		    else
			hvGfxLine(hvg, x1,y1, x2,y2, color);
		    }
		}
	    }

	//if( x1 < 0 || x1 > tl.picWidth )
	//printf("x1 = %d, sampleX=%d, winStart = %d\n<br>", x1, sampleX, winStart );
	if( x1 >= 0 && x1 <= tl.picWidth )
	{
	/* Draw the points themselves*/
	drawScaledBox(hvg, sampleX, sampleX+1, scale, xOff, (int)y1-1, 3, color);
	if( fill )
		drawScaledBox(hvg, sampleX, sampleX+1, scale, xOff, (int)y1+2, 
			      ybase-y1-2, bColor);
	}

	prevX = gapPrevX = sampleX;
	prevY = y1;
	}

    leftSide = max( tg->itemStart(tg,lf), winStart );
    rightSide = min(  tg->itemEnd(tg,lf), winEnd );

    currentX =  round((double)((int)leftSide-winStart)*scale) + xOff;
    currentXEnd =  round((double)((int)rightSide-winStart)*scale) + xOff;
    currentWidth = currentXEnd - currentX;

    if( noZoom && isFull )
	{
	mapBoxHc(hvg, lf->start, lf->end, currentX ,y, currentWidth,
	    heightPer, tg->mapName, tg->mapItemName(tg, lf), tg->itemName(tg, lf));

	if( lf->next != NULL )
	    y += sampleUpdateY( lf->name, lf->next->name, lineHeight );
	else
	    y += lineHeight;
	}

    }
}

struct track *sortGroupList = NULL; /* Used temporarily for sample sorting. */

int lfNamePositionCmp(const void *va, const void *vb)
/* Compare based on name, then chromStart, used for
   sorting sample based tracks. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
int dif;
char *tgName = NULL;
if(sortGroupList != NULL)
    tgName = sortGroupList->shortLabel;
if(tgName != NULL)
    {
    if(sameString(a->name, tgName) && differentString(b->name, tgName))
	return -1;
    if(sameString(b->name, tgName) && differentString(a->name, tgName))
	return 1;
    }
dif = strcmp(a->name, b->name);
if (dif == 0)
    dif = a->start - b->start;
return dif;
}


void loadSampleIntoLinkedFeature(struct track *tg)
/* Convert sample info in window to linked feature. */
{
int maxWiggleTrackHeight = 2500;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
struct sample *sample;
struct linkedFeatures *lfList = NULL, *lf;
char *hasDense = NULL;
char *where = NULL;
char query[256];

/*see if we have a summary table*/
safef(query, sizeof(query), 
	"select name from %s where name = '%s' limit 1", 
	tg->mapName, tg->shortLabel);
//errAbort( "%s", query );
hasDense = sqlQuickQuery(conn, query, query, sizeof(query));

/* If we're in dense mode and have a summary table load it. */
if(tg->visibility == tvDense)
    {
    if(hasDense != NULL)
	{
	safef(query, sizeof(query), " name = '%s' ", tg->shortLabel);
	where = cloneString(query);
	}
    }

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, where, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sample = sampleLoad(row + rowOffset);
    lf = lfFromSample(sample);
    slAddHead(&lfList, lf);
    sampleFree(&sample);
    }
if(where != NULL)
    freez(&where);
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);

/* sort to bring items with common names to the same line
but only for tracks with a summary table (with name=shortLabel) in
dense mode*/

if( hasDense != NULL )
    {
    sortGroupList = tg; /* used to put track name at top of sorted list. */
    slSort(&lfList, lfNamePositionCmp);
    sortGroupList = NULL;
    }
tg->items = lfList;

/*turn off full mode if there are too many rows or each row is too
 * large. A total of maxWiggleTrackHeight is allowed for number of
 * rows times the rowHeight*/
if( tg->visibility == tvFull && sampleTotalHeight( tg, tvFull ) > maxWiggleTrackHeight  )
    {
    tg->limitedVisSet = TRUE;
    tg->limitedVis = tvDense;
    }
}



void sampleLinkedFeaturesMethods(struct track *tg)
/* Fill in track methods for 'sample' tracks. */
{
linkedFeaturesMethods(tg);
tg->drawItems = wiggleLinkedFeaturesDraw;
tg->totalHeight = sampleTotalHeight;
tg->itemHeight = tgFixedItemHeight;
}


void sampleMethods(struct track *track, struct trackDb *tdb, int wordCount, char *words[])
/* Load up methods for a generic sample type track. */
{
track->scaleRange = 0.001;
track->minRange = 0.0;
track->maxRange = 1000.0;
if (wordCount > 1)     /*sample minRange maxRange*/
    track->minRange = atof(words[1]);
if (wordCount > 2)
    track->maxRange = atof(words[2]);
if (wordCount > 3)
    track->scaleRange = atof(words[3]);
track->bedSize = 9;
track->subType = lfSubSample;     /*make subType be "sample" (=2)*/
sampleLinkedFeaturesMethods(track);
track->loadItems = loadSampleIntoLinkedFeature;
}

void loadHumMusL(struct track *tg)
/* Load humMusL track with 2 zoom levels and one normal level. 
 * Also used for loading the musHumL track (called Human Cons) 
 * on the mouse browser. It decides which of 4 tables to
 * load based on how large of a window the user is looking at*/
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
struct sample *sample;
struct linkedFeatures *lfList = NULL, *lf;
char *hasDense = NULL;
char *where = NULL;
char tableName[256];
int z;
float pixPerBase = 0;

if(tl.picWidth == 0)
    errAbort("hgTracks.c::loadHumMusL() - can't have pixel width of 0");
pixPerBase = (winEnd - winStart)/ tl.picWidth;


/* Determine zoom level. */
 if (!strstr(tg->mapName,"HMRConservation"))
   z = humMusZoomLevel();
 else z=0;


if(z == 1 )
    safef(tableName, sizeof(tableName), "%s_%s", "zoom1",
	    tg->mapName);
else if( z == 2)
    safef(tableName, sizeof(tableName), "%s_%s", "zoom50",
	    tg->mapName);
else if(z == 3)
    safef(tableName, sizeof(tableName), "%s_%s",
	    "zoom2500", tg->mapName);
else
    safef(tableName, sizeof(tableName), "%s", tg->mapName);

//printf("(%s)", tableName );

sr = hRangeQuery(conn, tableName, chromName, winStart, winEnd,
    where, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sample = sampleLoad(row+rowOffset);
    lf = lfFromSample(sample);
    slAddHead(&lfList, lf);
    sampleFree(&sample);
    }
if(where != NULL)
    freez(&where);
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);

/* sort to bring items with common names to the same line
   but only for tracks with a summary table (with name=shortLabel)
   in
   dense mode*/
if( hasDense != NULL )
    {
    sortGroupList = tg; /* used to put track name at
	                 * top of sorted list. */
    slSort(&lfList, lfNamePositionCmp);
    sortGroupList = NULL;
    }
tg->items = lfList;
}

void humMusLMethods( struct track *tg )
/* Overide the humMusL load function to look for zoomed out tracks. */
{
tg->loadItems = loadHumMusL;
tg->totalHeight = consTotalHeight;
}

struct hash *zooSpeciesHash = NULL;

void zooSpeciesHashInit()
/* Function to init list of zoo species */
{
char *name = NULL;
char *val = NULL;

if (zooSpeciesHash != NULL)
    return;
zooSpeciesHash = hashNew(6);

name = cloneString("Human");
val = cloneString("1");
hashAdd(zooSpeciesHash, name, val);
cartSetBoolean( cart, "zooSpecies.Human", TRUE );

name = cloneString("Chimpanzee");
val = cloneString("2");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Baboon");
val = cloneString("3");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Orangutan");
val = cloneString("4");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Macaque");
val = cloneString("5");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Vervet");
val = cloneString("6");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Lemur");
val = cloneString("7");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Cat");
val = cloneString("8");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Dog");
val = cloneString("9");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Cow");
val = cloneString("10");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Pig");
val = cloneString("11");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Horse");
val = cloneString("12");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Rabbit");
val = cloneString("13");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Hedgehog");
val = cloneString("14");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("ajBat");
val = cloneString("15");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("cpBat");
val = cloneString("16");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Rat");
val = cloneString("17");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Mouse");
val = cloneString("18");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Platypus");
val = cloneString("19");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Opossum");
val = cloneString("20");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Dunnart");
val = cloneString("21");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Chicken");
val = cloneString("22");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Fugu");
val = cloneString("23");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Tetraodon");
val = cloneString("24");
hashAdd(zooSpeciesHash, name, val);

name = cloneString("Zebrafish");
val = cloneString("25");
hashAdd(zooSpeciesHash, name, val);
}


int lfZooCmp(const void *va, const void *vb)
/* Compare based on name, then chromStart, used for
   sorting sample based tracks. */
{
struct linkedFeatures *a = *((struct linkedFeatures **)va);
struct linkedFeatures *b = *((struct linkedFeatures **)vb);
char *aVal =  hashFindVal(zooSpeciesHash, a->name);
char *bVal =  hashFindVal(zooSpeciesHash, b->name);
int aV = atoi(aVal);
int bV = atoi(bVal);
return aV - bV;
}

void loadSampleZoo(struct track *tg)
/* Convert sample info in window to linked feature. */
{
int maxWiggleTrackHeight = 2500;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
struct sample *sample;
struct linkedFeatures *lfList = NULL, *lf;
char *hasDense = NULL;
char *where = NULL;
char query[256];
char option[64];

zooSpeciesHashInit();

/*see if we have a summary table*/
safef(query, sizeof(query), "select name from %s where name = '%s' limit 1", tg->mapName, tg->shortLabel);
//errAbort( "%s", query );
hasDense = sqlQuickQuery(conn, query, query, sizeof(query));

/* If we're in dense mode and have a summary table load it. */
if(tg->visibility == tvDense)
    {
    if(hasDense != NULL)
	{
	safef(query, sizeof(query), " name = '%s' ", tg->shortLabel);
	where = cloneString(query);
	}
    }

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, where, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sample = sampleLoad(row + rowOffset);
    lf = lfFromSample(sample);
    safef( option, sizeof(option), "zooSpecies.%s", sample->name );
    if( cartUsualBoolean(cart, option, TRUE ))
    slAddHead(&lfList, lf);
    sampleFree(&sample);
    }
if(where != NULL)
    freez(&where);
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);

/* sort to bring items with common names to the same line
 * but only for tracks with a summary table 
 * (with name=shortLabel) in dense mode */
if( hasDense != NULL )
    {
    sortGroupList = tg; /* used to put track name at top of sorted list. */
    slSort(&lfList, lfNamePositionCmp);
    sortGroupList = NULL;
    }

/* Sort in species phylogenetic order */
slSort(&lfList, lfZooCmp);


tg->items = lfList;

/*turn off full mode if there are too many rows or each row is too
 * large. A total of maxWiggleTrackHeight is allowed for number of
 * rows times the rowHeight*/
if( tg->visibility == tvFull && sampleTotalHeight( tg, tvFull ) > maxWiggleTrackHeight  )
    {
    tg->limitedVisSet = TRUE;
    tg->limitedVis = tvDense;
    }
}

void zooMethods( struct track *tg )
/* Overide the zoo sample type load function to look for zoomed out tracks. */
{
tg->loadItems = loadSampleZoo;
}


void loadAffyTranscriptome(struct track *tg)
/* Convert sample info in window to linked feature. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
struct sample *sample;
struct linkedFeatures *lfList = NULL, *lf;
char *hasDense = NULL;
char *where = NULL;
char query[256];
char tableName[256];
int zoom1 = 23924, zoom2 = 2991; /* bp per data point */
float pixPerBase = 0;
/* Set it up so we don't have linear interpretation.. */
char *noLinearInterpString = wiggleEnumToString(wiggleNoInterpolation);
cartSetString(cart, "affyTranscriptome.linear.interp", noLinearInterpString);

if(tl.picWidth == 0)
    errAbort("hgTracks.c::loadAffyTranscriptome() - can't have pixel width of 0");
pixPerBase = (winEnd - winStart)/ tl.picWidth;


/* Determine zoom level. */
if(pixPerBase >= zoom1)
    safef(tableName, sizeof(tableName), "%s_%s", "zoom1", tg->mapName);
else if(pixPerBase >= zoom2)
    safef(tableName, sizeof(tableName), "%s_%s", "zoom2", tg->mapName);
else 
    safef(tableName, sizeof(tableName), "%s", tg->mapName);

/*see if we have a summary table*/
if(hTableExists(database, tableName))
    safef(query, sizeof(query), "select name from %s where name = '%s' limit 1",  tableName, tg->shortLabel);
else
    {
    warn("<p>Couldn't find table %s<br><br>", tableName);
    safef(query, sizeof(query), "select name from %s where name = '%s' limit 1",  tg->mapName, tg->shortLabel);
    safef(tableName, sizeof(tableName), "%s", tg->mapName);
    }

hasDense = sqlQuickQuery(conn, query, query, sizeof(query));

/* If we're in dense mode and have a summary table load it. */
if(tg->visibility == tvDense)
    {
    if(hasDense != NULL)
	{
	safef(query, sizeof(query), " name = '%s' ", tg->shortLabel);
	where = cloneString(query);
	}
    }

sr = hRangeQuery(conn, tableName, chromName, winStart, winEnd, where, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sample = sampleLoad(row+rowOffset);
    lf = lfFromSample(sample);
    slAddHead(&lfList, lf);
    sampleFree(&sample);
    }
if(where != NULL)
    freez(&where);
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);

/* sort to bring items with common names to the same line
but only for tracks with a summary table (with name=shortLabel) in
dense mode*/
if( hasDense != NULL )
    {
    sortGroupList = tg; /* used to put track name at top of sorted list. */
    slSort(&lfList, lfNamePositionCmp);
    sortGroupList = NULL;
    }
tg->items = lfList;
}


void affyTranscriptomeMethods(struct track *tg)
/* Overide the load function to look for zoomed out tracks. */
{
tg->loadItems = loadAffyTranscriptome;
}

