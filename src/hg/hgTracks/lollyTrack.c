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

#define LOLLY_DIAMETER    2 * (lollyCart->radius + 2)

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
char *mouseOver;
};

static unsigned getLollyColor( struct hvGfx *hvg, unsigned color)
/* Get the device color from our internal definition. */
{
struct rgbColor itemRgb;
itemRgb.r = (color & 0xff0000) >> 16;
itemRgb.g = (color & 0xff00) >> 8;
itemRgb.b = color & 0xff;
return  hvGfxFindColorIx(hvg, itemRgb.r, itemRgb.g, itemRgb.b);
}

void doYLabels(struct track *tg, struct hvGfx *hvg, int width, int height, struct lollyCartOptions *lollyCart, int xOff, int yOff, Color color, MgFont *font, boolean doLabels )
/* Draw labels or lines for labels. */
{
double range = lollyCart->upperLimit - lollyCart->lowerLimit;
int fontHeight = tl.fontHeight+1;
// we need a margin on top and bottom for half a lolly, and some space at the bottom
// for the lolly stems
double minimumStemHeight = fontHeight;
double topAndBottomMargins = LOLLY_DIAMETER / 2 + LOLLY_DIAMETER / 2;
double usableHeight = height - topAndBottomMargins - minimumStemHeight; 
yOff += LOLLY_DIAMETER / 2;

struct hashEl *hel = trackDbSettingsLike(tg->tdb, "yLabel*");
// parse lines like yLabel <y offset> <draw line ?>  <R,G,B> <label>
for(; hel; hel = hel->next)
    {
    char *setting = cloneString((char *)hel->val);
    int number = atoi(nextWord(&setting)); 
    boolean drawLine = sameString("on", nextWord(&setting));
    unsigned char red, green, blue;
    char *colorStr = nextWord(&setting);
    parseColor(colorStr, &red, &green, &blue);
    unsigned long  lineColor = MAKECOLOR_32(red, green, blue);
    char *label = setting;
    int offset = usableHeight * ((double)number  - lollyCart->lowerLimit) / range;
    if (doLabels)
        hvGfxTextRight(hvg, xOff, yOff + (usableHeight - offset) - fontHeight / 2 , width - 1, fontHeight, color, font, label);
    else if (drawLine)
        hvGfxLine(hvg, xOff, yOff + (usableHeight - offset),  xOff + width , yOff + (usableHeight - offset), lineColor);
    }
}

static void lollyDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw a list of lolly structures. */
{
double scale = scaleForWindow(width, seqStart, seqEnd);
struct lolly *popList = tg->items, *pop;
struct lollyCartOptions *lollyCart = tg->lollyCart;
int trackHeight = tg->lollyCart->height ;

if (popList == NULL)   // nothing to draw
    return;

if ( tg->visibility == tvDense)  // in dense mode we just track lines
    {
    for (pop = popList; pop; pop = pop->next)
        {
        int sx = ((pop->start - seqStart) + .5) * scale + xOff; // x coord of center (lower region)
        unsigned color =  getLollyColor(hvg, pop->color);
        hvGfxLine(hvg, sx, yOff, sx , yOff+ tl.fontHeight, color);
        }
    return;
    }

doYLabels(tg, hvg, width, trackHeight, tg->lollyCart, xOff, yOff, color, font, FALSE);
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
int fontHeight = tl.fontHeight + 1;
double usableHeight = trackHeight - LOLLY_DIAMETER - fontHeight; 
yOff += LOLLY_DIAMETER / 2;

if (!lollyCart->noStems)
    for (pop = popList; pop; pop = pop->next)
        {
        int sx = ((pop->start - seqStart) + .5) * scale + xOff; // x coord of center (lower region)
        hvGfxLine(hvg, sx, yOff + trackHeight, sx , yOff+(usableHeight - pop->height), MG_GRAY);
        }

// now draw the sucker part!
for (pop = popList; pop; pop = pop->next)
    {
    int sx = ((pop->start - seqStart) + .5) * scale + xOff; // x coord of center (lower region)
    unsigned color =  getLollyColor(hvg, pop->color);
    hvGfxCircle(hvg, sx, yOff + (usableHeight - (pop->height )), pop->radius, color, TRUE);
    if ( tg->visibility != tvSquish)  
        hvGfxCircle(hvg, sx, yOff + (usableHeight - (pop->height )), pop->radius, MG_BLACK, FALSE);
    if (!noMapBoxes)
        mapBoxHgcOrHgGene(hvg, pop->start, pop->end, sx - pop->radius, yOff + usableHeight - pop->radius - pop->height, 2 * pop->radius,2 * pop->radius,
                          tg->track, pop->name, pop->mouseOver, NULL, TRUE, NULL);
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

//doYLabels(tg, hvg, width, height-fontHeight, lollyCart, xOff, yOff+fontHeight, color, font, TRUE);
doYLabels(tg, hvg, width, height-fontHeight, lollyCart, xOff, yOff+fontHeight, color, font, TRUE);

hvGfxText(hvg, xOff, yOff+centerLabel, color, font, tg->shortLabel);

char *setting = trackDbSetting(tg->tdb, "yNumLabels");
if (setting && sameString("off", setting))
    return;
if (isnan(lollyCart->upperLimit))
    {
    hvGfxTextRight(hvg, xOff, yOff + LOLLY_DIAMETER, width - 1, fontHeight, color,
        font, "NO DATA");
    return;
    }

char upper[1024];
safef(upper, sizeof(upper), "%g -",   lollyCart->upperLimit);
hvGfxTextRight(hvg, xOff, yOff + fontHeight / 2 + LOLLY_DIAMETER / 2 , width - 1, fontHeight, color,
    font, upper);
char lower[1024];
if (lollyCart->lowerLimit < lollyCart->upperLimit)
    {
    safef(lower, sizeof(lower), "%g -", lollyCart->lowerLimit);
    hvGfxTextRight(hvg, xOff, yOff+height - LOLLY_DIAMETER - fontHeight/2 , width - 1, fontHeight,
        color, font, lower);
    }
}


static int lollyHeight(struct track *tg, enum trackVisibility vis)
/* calculate height of all the lollys being displayed */
{
if ( tg->visibility == tvDense)
    return  tl.fontHeight;

// return the height we calculated at load time
return tg->lollyCart->height;
}

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
    //lollyCart->radius = 2;
    lollyCart->height =  lollyCart->origHeight / 2;
    }
else if (tg->visibility == tvPack)
    {
    //lollyCart->radius = 4;
    lollyCart->height =  lollyCart->origHeight / 1.5;
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

int lollySizeField = -1 ;
setting = trackDbSetting(tg->tdb, "lollySizeField");
if (setting != NULL)
    lollySizeField = atoi(setting);
    
double minVal = DBL_MAX, maxVal = -DBL_MAX;
//double sumData = 0.0, sumSquares = 0.0;
unsigned count = 0;

int trackHeight = tg->lollyCart->height;
struct bigBedFilter *filters = bigBedBuildFilters(cart, bbi, tg->tdb);
char *mouseOverField = cartOrTdbString(cart, tg->tdb, "mouseOverField", NULL);
int mouseOverIdx = bbExtraFieldIndex(bbi, mouseOverField) ;
                    
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
    pop->radius = -1;
    if (lollySizeField > 0)
        {
        double radius = atoi(bedRow[lollySizeField - 1]) * trackHeight / 100.0;
        pop->radius = radius;
        }
    pop->color = 0;

    pop->mouseOver = pop->name;
    extern char* restField(struct bigBedInterval *bb, int fieldIdx) ;
    if (mouseOverIdx > 0)
        pop->mouseOver   = restField(bb, mouseOverIdx);

    if (bbi->fieldCount > 8)
        pop->color = itemRgbColumn(bedRow[8]);
    count++;
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
int fontHeight = tl.fontHeight+1;
double usableHeight = trackHeight - LOLLY_DIAMETER - fontHeight; 
for(pop = popList; pop; pop = pop->next)
    {
    if (pop->radius == -1)
        pop->radius = lollyCart->radius;
    if (range == 0.0)
        pop->height = usableHeight ;
    else
        pop->height = usableHeight * (pop->val  - lollyCart->lowerLimit) / range;
    }

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
lollyCart->origHeight = lollyCart->height = defaultHeight;

lollyCart->autoScale = wigFetchAutoScaleWithCart(cart,tdb, tdb->track, NULL);

double tDbMinY;     /*  data range limits from trackDb type line */
double tDbMaxY;     /*  data range limits from trackDb type line */
char *trackWords[8];     /*  to parse the trackDb type line  */
int trackWordCount = 0;  /*  to parse the trackDb type line  */
wigFetchMinMaxYWithCart(cart, tdb, tdb->track, &lollyCart->minY, &lollyCart->maxY, &tDbMinY, &tDbMaxY, trackWordCount, trackWords);
lollyCart->upperLimit = lollyCart->maxY;
lollyCart->lowerLimit = lollyCart->minY;
lollyCart->radius = 5;
lollyCart->noStems = trackDbSettingOn(tdb, "noStems");
char *setting = trackDbSetting(tdb, "lollyMaxSize");
if (setting)
    lollyCart->radius = atoi(setting);

return lollyCart;
}

void lollyMethods(struct track *track, struct trackDb *tdb, 
                                int wordCount, char *words[])
/* bigLolly track type methods */
{
bigBedMethods(track, tdb, wordCount, words);

struct lollyCartOptions *lollyCart = lollyCartOptionsNew(cart, tdb, wordCount, words);

lollyCart->typeWordCount = wordCount;
AllocArray(lollyCart->typeWords, wordCount);
int ii;
for(ii=0; ii < wordCount; ii++)
    lollyCart->typeWords[ii] = cloneString(words[ii]);
track->loadItems = lollyLoadItems;
track->drawItems = lollyDrawItems;
track->totalHeight = lollyHeight; 
track->drawLeftLabels = lollyLeftLabels;
track->lollyCart = lollyCart;
}
