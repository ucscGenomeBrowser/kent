/* lollyTrack -- load and draw lollys */

/* Copyright (C) 2019 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "obscure.h"
#include "hgTracks.h"
#include "bedCart.h"
#include "bigWarn.h"
#include "lolly.h"
#include "limits.h"
#include "float.h"
#include "bigBedFilter.h"

#define LOLLY_DIAMETER    2 * lollyCart->radius


/* the lolly colors */
static int lollyPalette[] =
{
0x1f77b4, 0xff7f0e, 0x2ca02c, 0xd62728, 0x9467bd, 0x8c564b, 0xe377c2, 0x7f7f7f, 0xbcbd22, 0x17becf
};

struct lolly
{
struct lolly *next;
char *name;       /* the mouseover name */
double val;       /* value in the data file */   
unsigned start;   /* genomic start address */
unsigned end;     /* genomic end address */
unsigned radius;  /* radius of the top of the lolly */
unsigned height;  /* height of the lolly */
Color color;      /* color of the lolly */
};

static void lollyDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw a list of lolly structures. */
{
double scale = scaleForWindow(width, seqStart, seqEnd);
struct lolly *popList = tg->items, *pop;
int trackHeight = tg->lollyCart->height;

if (popList == NULL)   // nothing to draw
    return;

if ( tg->visibility == tvDense)  // in dense mode we just track lines
    {
    for (pop = popList; pop; pop = pop->next)
        {
        int sx = ((pop->start - seqStart) + .5) * scale + xOff; // x coord of center (lower region)
        hvGfxLine(hvg, sx, yOff, sx , yOff+ tl.fontHeight, pop->color);
        }
    return;
    }

boolean noMapBoxes = FALSE;
int numItems =  slCount(popList);
if ( numItems > 5000)
    {
    char buffer[4096];
    safef(buffer, sizeof buffer, "Too many (%d) items in window.  Zoom in or set viewing range to be more restrictive.", numItems);
    noMapBoxes = TRUE;
    mapBoxHgcOrHgGene(hvg, winStart, winEnd, xOff, yOff, width,  trackHeight,
                  tg->track, "", buffer, "hgTracks", FALSE, NULL);
    }
// first draw the lines so they won't overlap any lollies
for (pop = popList; pop; pop = pop->next)
    {
    int sx = ((pop->start - seqStart) + .5) * scale + xOff; // x coord of center (lower region)
    hvGfxLine(hvg, sx, yOff + trackHeight, sx , yOff+(trackHeight - pop->height), MG_GRAY);
    }

// now draw the sucker part!
for (pop = popList; pop; pop = pop->next)
    {
    int sx = ((pop->start - seqStart) + .5) * scale + xOff; // x coord of center (lower region)
    hvGfxCircle(hvg, sx, yOff + trackHeight - pop->radius - pop->height, pop->radius, pop->color, TRUE);
    if (!noMapBoxes)
        mapBoxHgcOrHgGene(hvg, pop->start, pop->end, sx - pop->radius, yOff + trackHeight - pop->radius - pop->height - pop->radius, 2 * pop->radius,2 * pop->radius,
                          tg->track, pop->name, pop->name, NULL, TRUE, NULL);
    }
}

void lollyLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
// draw the labels on the left margin
{
struct lollyCartOptions *lollyCart = tg->lollyCart;
int fontHeight = tl.fontHeight+1;
int centerLabel = (height/2)-(fontHeight/2);
if ( tg->visibility == tvDense)
    {
    hvGfxTextRight(hvg, xOff, yOff+fontHeight, width - 1, fontHeight,  color, font, tg->shortLabel);
    return;
    }

hvGfxText(hvg, xOff, yOff+centerLabel, color, font, tg->shortLabel);

if (isnan(lollyCart->upperLimit))
    {
    hvGfxTextRight(hvg, xOff, yOff + LOLLY_DIAMETER, width - 1, fontHeight, color,
        font, "NO DATA");
    return;
    }

char upper[1024];
safef(upper, sizeof(upper), "%g -",   lollyCart->upperLimit);
hvGfxTextRight(hvg, xOff, yOff + LOLLY_DIAMETER, width - 1, fontHeight, color,
    font, upper);
char lower[1024];
if (lollyCart->lowerLimit < lollyCart->upperLimit)
    {
    safef(lower, sizeof(lower), "%g _", lollyCart->lowerLimit);
    hvGfxTextRight(hvg, xOff, yOff+height-fontHeight - LOLLY_DIAMETER, width - 1, fontHeight,
        color, font, lower);
    }
}


static int lollyHeight(struct track *tg, enum trackVisibility vis)
/* calculate height of all the lollys being displayed */
{
if ( tg->visibility == tvDense)
    return  tl.fontHeight;

// if we're pack, then use bigBed drawing
if (tg->visibility == tvPack)
    {
    bigBedMethods(tg, tg->tdb, tg->lollyCart->wordCount, tg->lollyCart->words);
    tg->mapsSelf = FALSE;
    tg->drawLeftLabels = NULL;
    return tg->totalHeight(tg, vis);
    }

// return the height we calculated at load time
return tg->lollyCart->height;
}

#ifdef NOTUSED
double calcVarianceFromSums(double sum, double sumSquares, bits64 n)
/* Calculate variance. */
{   
double var = sumSquares - sum*sum/n;
if (n > 1)
    var /= n-1;
return var;
}   
    
double calcStdFromSums(double sum, double sumSquares, bits64 n)
/* Calculate standard deviation. */
{   
return sqrt(calcVarianceFromSums(sum, sumSquares, n));
}
#endif // NOTUSED

int cmpHeight(const void *va, const void *vb)
// sort the lollies by height 
{
const struct lolly *a = *((struct lolly **)va);
const struct lolly *b = *((struct lolly **)vb);
return a->height - b->height;
}

void lollyLoadItems(struct track *tg)
// load lollies from the data file
{
struct lollyCartOptions *lollyCart = tg->lollyCart;
if (tg->visibility == tvSquish)
    {
    lollyCart->radius = 2;
    lollyCart->height /= 3;
    }
struct lm *lm = lmInit(0);
struct bbiFile *bbi =  fetchBbiForTrack(tg);
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chromName, winStart, winEnd, 0, lm);
char *bedRow[bbi->fieldCount];
char startBuf[16], endBuf[16];
struct lolly *popList = NULL, *pop;

unsigned lollyField = 5;  // we use the score field by default
char *setting = trackDbSetting(tg->tdb, "lollyField");
if (setting != NULL)
    lollyField = atoi(setting);

double minVal = DBL_MAX, maxVal = -DBL_MAX;
//double sumData = 0.0, sumSquares = 0.0;
unsigned count = 0;

int trackHeight = tg->lollyCart->height;
struct bigBedFilter *filters = bigBedBuildFilters(cart, bbi, tg->tdb);
                    
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));

    // throw away items that don't pass the filters
    if (!bigBedFilterInterval(bedRow, filters))
        continue;

    // clip out lollies that aren't in our display range
    double val = atof(bedRow[lollyField - 1]);
    if (!((lollyCart->autoScale == wiggleScaleAuto) ||  ((val >= lollyCart->minY) && (val <= lollyCart->maxY) )))
        continue;

    // don't draw lollies off the screen
    if (atoi(bedRow[1]) < winStart)
        continue;

    AllocVar(pop);
    slAddHead(&popList, pop);
    pop->val = val;
    pop->start = atoi(bedRow[1]);
    pop->end = atoi(bedRow[2]);
    pop->name = cloneString(bedRow[3]);
    count++;
    // sumData += val;
    // sumSquares += val * val;
    if (val > maxVal)
        maxVal = val;
    if (val < minVal)
        minVal = val;
    }

if (count == 0)
    lollyCart->upperLimit = lollyCart->lowerLimit = NAN; // no lollies in range
else if (lollyCart->autoScale == wiggleScaleAuto)
    {
    lollyCart->upperLimit = maxVal;
    lollyCart->lowerLimit = minVal;
    }

double range = lollyCart->upperLimit - lollyCart->lowerLimit;
int usableHeight = trackHeight - 2 * LOLLY_DIAMETER; 
for(pop = popList; pop; pop = pop->next)
    {
    pop->radius = lollyCart->radius;
    pop->color = MG_RED;
    if (range == 0.0)
        {
        pop->height = usableHeight ;
        pop->color = lollyPalette[0] | 0xff000000;
        }
    else
        {
        pop->height = usableHeight * (pop->val  - lollyCart->lowerLimit) / range + LOLLY_DIAMETER;
        int colorIndex = 8 * (pop->val  - lollyCart->lowerLimit) / range;
        pop->color = lollyPalette[colorIndex] | 0xff000000;
        }
    }

#ifdef NOTUSED   // a method of scaling assuming a sort of normal distribution
double average = sumData/count;
double stdDev = calcStdFromSums(sumData, sumSquares, count);

for(pop = popList; pop; pop = pop->next)
    {
    if (pop->val > average + stdDev / 5)
        {
        pop->color = MG_RED;
        pop->radius = 8;
        pop->height = 3 * tl.fontHeight;
        }
    else if (pop->val < average - stdDev / 5)
        {
        pop->color = MG_GREEN;
        pop->radius = 3;
        pop->height = 1 * tl.fontHeight;
        }
    else
        {
        pop->color = MG_GRAY;
        pop->radius = 5;
        pop->height = 2 * tl.fontHeight;
        }
        
    }
#endif // NOTUSED

slSort(&popList, cmpHeight);
tg->items = popList;
}

static struct lollyCartOptions *lollyCartOptionsNew(struct cart *cart, struct trackDb *tdb, 
                                int wordCount, char *words[])
// the structure that is attached to the track structure
{
struct lollyCartOptions *lollyCart;
AllocVar(lollyCart);

int maxHeightPixels;
int minHeightPixels;
int defaultHeight;  /*  pixels per item */
int settingsDefault;

cartTdbFetchMinMaxPixels(cart, tdb, MIN_HEIGHT_PER, atoi(DEFAULT_HEIGHT_PER), atoi(DEFAULT_HEIGHT_PER),
                                &minHeightPixels, &maxHeightPixels, &settingsDefault, &defaultHeight);
lollyCart->height = defaultHeight;

lollyCart->autoScale = wigFetchAutoScaleWithCart(cart,tdb, tdb->track, NULL);

double tDbMinY;     /*  data range limits from trackDb type line */
double tDbMaxY;     /*  data range limits from trackDb type line */
char *trackWords[8];     /*  to parse the trackDb type line  */
int trackWordCount = 0;  /*  to parse the trackDb type line  */
wigFetchMinMaxYWithCart(cart, tdb, tdb->track, &lollyCart->minY, &lollyCart->maxY, &tDbMinY, &tDbMaxY, trackWordCount, trackWords);
lollyCart->upperLimit = lollyCart->maxY;
lollyCart->lowerLimit = lollyCart->minY;

return lollyCart;
}

void lollyMethods(struct track *track, struct trackDb *tdb, 
                                int wordCount, char *words[])
/* bigLolly track type methods */
{
bigBedMethods(track, tdb, wordCount, words);

struct lollyCartOptions *lollyCart = lollyCartOptionsNew(cart, tdb, wordCount, words);
lollyCart->radius = 5;
lollyCart->words = words;
lollyCart->wordCount = wordCount;
track->loadItems = lollyLoadItems;
track->drawItems = lollyDrawItems;
track->totalHeight = lollyHeight; 
track->drawLeftLabels = lollyLeftLabels;
track->lollyCart = lollyCart;
}
