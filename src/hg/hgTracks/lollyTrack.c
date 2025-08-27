/* lollyTrack -- load and draw lollys */

/* Copyright (C) 2019 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "obscure.h"
#include "hgTracks.h"
#include "bedCart.h"
#include "bigWarn.h"
#include "lolly.h"
#include "limits.h"
#include "float.h"
#include "bigBedFilter.h"
#include "asParse.h"
#include "quickLift.h"

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

// structure to capture yLabels
struct yLabel
{
struct yLabel *next;
unsigned y;
char *label;
unsigned long color;
boolean on;
};

int cmpY(const void *va, const void *vb)
// sort the lines by y value
{
const struct yLabel *a = *((struct yLabel **)va);
const struct yLabel *b = *((struct yLabel **)vb);
return a->y - b->y;
}

// font heights available to the user
unsigned fontHeights[] = { 6,8,10,12,14,18,24 ,34 };

unsigned findBiggest(unsigned num)
/* find biggest font not bigger than num */
{
int ii;
unsigned prev = 6;

for (ii=0; ii < ArraySize(fontHeights); prev = fontHeights[ii], ii++)
    if (fontHeights[ii] > num)
        return prev;

return 34;
}

void doYLabels(struct track *tg, struct hvGfx *hvg, int width, int height, struct lollyCartOptions *lollyCart, int xOff, int yOff, Color color, MgFont *font, boolean doLabels )
/* parse lines like yAxisLabel <y offset> <draw line ?>  <R,G,B> <label>
 * Draw labels or lines for labels. */
{
struct hashEl *hel = trackDbSettingsLike(tg->tdb, "yAxisLabel*");

if (hel == NULL)
    return;

double range = tg->graphUpperLimit - tg->graphLowerLimit;
int fontHeight = tl.fontHeight+1;
// we need a margin on top and bottom for half a lolly, and some space at the bottom
// for the lolly stems
double minimumStemHeight = fontHeight;
double topAndBottomMargins = LOLLY_DIAMETER / 2 + LOLLY_DIAMETER / 2;
double usableHeight = height - topAndBottomMargins - minimumStemHeight; 
yOff += LOLLY_DIAMETER / 2;

struct yLabel *lineList = NULL;
struct yLabel *line;
for(; hel; hel = hel->next)
    {
    AllocVar(line);
    slAddHead(&lineList, line);

    char *setting = cloneString((char *)hel->val);
    double number = atof(nextWord(&setting)); 
    line->on = sameString("on", nextWord(&setting));
    unsigned char red, green, blue;
    char *colorStr = nextWord(&setting);
    parseColor(colorStr, &red, &green, &blue);
    line->color = MAKECOLOR_32(red, green, blue);
    line->label = cloneString(setting);

    int offset = usableHeight * ((double)number  - tg->graphLowerLimit) / range;
    line->y = yOff + (usableHeight - offset);
    }

slSort(&lineList, cmpY);
unsigned minDiff = BIGNUM;
struct yLabel *prev = NULL;
MgFont *labelfont = font;

// figure out what font height we should use
if (doLabels)
    {
    for(line = lineList; line; prev = line, line = line->next)
        {
        if (prev)
            {
            unsigned min = line->y - prev->y;
            
            if (min < minDiff)
                minDiff = min;
            }
        }

    if (minDiff <= tl.fontHeight)
        {
        unsigned choice = findBiggest(minDiff);
        if (doLabels && (choice == 0))
            return;
        if (choice < tl.fontHeight)
            {
            char size[1024];

            safef(size, sizeof size, "%d", choice);
            labelfont = mgFontForSizeAndStyle(size, "medium");
            }
        }
    fontHeight = mgFontPixelHeight(labelfont) + 1;
    }

for(line = lineList; line; line = line->next)
    {
    if (doLabels)
        hvGfxTextRight(hvg, xOff, line->y - fontHeight / 2 , width - 1, fontHeight, line->color, labelfont, line->label);
    else if (line->on)
        hvGfxLine(hvg, xOff, line->y,  xOff + width , line->y, line->color);
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

if ( tg->visibility == tvDense)  // in dense mode we just draw lines
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

if (popList == NULL)   // nothing to draw
    {
    maybeDrawQuickLiftLines(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
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

// calculate the lollipop heights
double range = tg->graphUpperLimit - tg->graphLowerLimit;
int fontHeight = tl.fontHeight+1;
double usableHeight = trackHeight - LOLLY_DIAMETER - fontHeight; 
for(pop = popList; pop; pop = pop->next)
    {
    if (pop->radius == -1)
        pop->radius = lollyCart->radius;
    if (range == 0.0)
        pop->height = usableHeight ;
    else
        pop->height = usableHeight * (pop->val  - tg->graphLowerLimit) / range;
    }

// first draw the lines so they won't overlap any lollies
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
maybeDrawQuickLiftLines(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
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

char *setting = trackDbSetting(tg->tdb, "yAxisNumLabels");
if (setting && sameString("off", setting))
    return;
if (isnan(tg->graphUpperLimit))
    {
    hvGfxTextRight(hvg, xOff, yOff + LOLLY_DIAMETER, width - 1, fontHeight, color,
        font, "NO DATA");
    return;
    }

char upper[1024];
safef(upper, sizeof(upper), "%g -",   tg->graphUpperLimit);
hvGfxTextRight(hvg, xOff, yOff + fontHeight / 2 + LOLLY_DIAMETER / 2 , width - 1, fontHeight, color,
    font, upper);
char lower[1024];
if (tg->graphLowerLimit < tg->graphUpperLimit)
    {
    safef(lower, sizeof(lower), "%g -", tg->graphLowerLimit);
    hvGfxTextRight(hvg, xOff, yOff+height - LOLLY_DIAMETER / 2 - 2 *fontHeight + fontHeight/2 , width - 1, fontHeight,
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

static int getField(struct trackDb *tdb, char *name, struct asObject *as, int defaultValue)
/* Get the as field (if any) that a trackDb variable references.  This can either be
 * a field name, or a number. */
{
int ret = defaultValue;
char *setting = trackDbSetting(tdb, name);
if (setting != NULL)
    {
    if (isNumericString(setting))
        ret = atoi(setting);
    else 
        {
        struct asColumn *columns = as->columnList;
        ret = asColumnMustFindIx(columns, setting) + 1;
        }
    }
return ret;
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
struct asObject *as = bigBedAsOrDefault(bbi);
struct bigBedInterval *bb, *bbList; 
char *quickLiftFile = cloneString(trackDbSetting(tg->tdb, "quickLiftUrl"));
struct hash *chainHash = NULL;
if (quickLiftFile)
    bbList = quickLiftGetIntervals(quickLiftFile, bbi, chromName, winStart, winEnd, &chainHash);
else
    bbList =  bigBedIntervalQuery(bbi, chromName, winStart, winEnd, 0, lm);
char *bedRow[bbi->fieldCount];
char startBuf[16], endBuf[16];
struct lolly *popList = NULL, *pop;

unsigned lollyField = getField(tg->tdb, "lollyField", as, 5);  // we use the score field by default
int lollySizeField = getField(tg->tdb, "lollySizeField", as, -1);  // no size field by default

double minVal = DBL_MAX, maxVal = -DBL_MAX;
//double sumData = 0.0, sumSquares = 0.0;
unsigned count = 0;

int trackHeight = tg->lollyCart->height;
struct bigBedFilter *filters = bigBedBuildFilters(cart, bbi, tg->tdb);
char *mouseOverField = cartOrTdbString(cart, tg->tdb, "mouseOverField", NULL);
int mouseOverIdx = bbExtraFieldIndex(bbi, mouseOverField) ;

char *mouseOverPattern = NULL;
char **fieldNames = NULL;
if (!mouseOverIdx)
    {
    mouseOverPattern = cartOrTdbString(cart, tg->tdb, "mouseOver", NULL);
    if (mouseOverPattern)
        {
        AllocArray(fieldNames, bbi->fieldCount);
        struct slName *field = NULL, *fields = bbFieldNames(bbi);
        int i =  0;
        for (field = fields; field != NULL; field = field->next)
            fieldNames[i++] = field->name;
        }
    }
                    
int filtered = 0;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));

    // throw away items that don't pass the filters
    if (!bigBedFilterInterval(bbi, bedRow, filters))
        {
        filtered++;
        continue;
        }

    // clip out lollies that aren't in our display range
    double val = atof(bedRow[lollyField - 1]);
    if (!((lollyCart->autoScale == wiggleScaleAuto) ||  ((val >= lollyCart->minY) && (val <= lollyCart->maxY) )))
        continue;

    if (quickLiftFile)
        {
        struct bed *bed;
        if ((bed = quickLiftIntervalsToBed(bbi, chainHash, bb)) != NULL)
            {
            // don't draw lollies off the screen
            if (bed->chromStart < winStart)
                continue;

            AllocVar(pop);
            slAddHead(&popList, pop);
            pop->val = val;
            pop->start = bed->chromStart;
            pop->end = bed->chromEnd;
            }
        }
    else
        {
        // don't draw lollies off the screen
        if (atoi(bedRow[1]) < winStart)
            continue;

        AllocVar(pop);
        slAddHead(&popList, pop);
        pop->val = val;
        pop->start = atoi(bedRow[1]);
        pop->end = atoi(bedRow[2]);
        }
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
    else if (mouseOverPattern)
        pop->mouseOver = replaceFieldInPattern(mouseOverPattern, bbi->fieldCount, fieldNames, bedRow);

    if (bbi->fieldCount > 8)
        pop->color = itemRgbColumn(bedRow[8]);
    count++;
    if (val > maxVal)
        maxVal = val;
    if (val < minVal)
        minVal = val;
    }
if (filtered)
   labelTrackAsFilteredNumber(tg, filtered);

if (lollyCart->autoScale == wiggleScaleAuto)
    {
    if (maxVal == -DBL_MAX)
        {
        tg->graphUpperLimit = NAN;
        tg->graphLowerLimit = NAN;
        }
    else
        {
        tg->graphUpperLimit = maxVal;
        tg->graphLowerLimit = minVal;
        }
    }

slSort(&popList, cmpHeight);
tg->items = popList;
}

static struct lollyCartOptions *lollyCartOptionsNew(struct cart *cart, struct track *track, 
                                int wordCount, char *words[])
// the structure that is attached to the track structure
{
struct trackDb *tdb = track->tdb;
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
track->graphUpperLimit = lollyCart->maxY;
track->graphLowerLimit = lollyCart->minY;
lollyCart->radius = 5;
lollyCart->noStems = trackDbSettingOn(tdb, "lollyNoStems");
char *setting = trackDbSetting(tdb, "lollyMaxSize");
if (setting)
    lollyCart->radius = atoi(setting);

return lollyCart;
}

void lollyRegionGraphLimits(struct track *tg)
/* Set common graphLimits across all windows */
{
double graphUpperLimit = -BIGDOUBLE;
double graphLowerLimit = BIGDOUBLE;
struct track *tgSave = tg;
struct window *w;

// find graphLimits across all windows.
for(w=windows,tg=tgSave; w; w=w->next,tg=tg->nextWindow)
    {
    if (isnan(tg->graphUpperLimit))
        continue;

    if (tg->graphUpperLimit > graphUpperLimit)
	graphUpperLimit = tg->graphUpperLimit;
    if (tg->graphLowerLimit < graphLowerLimit)
	graphLowerLimit = tg->graphLowerLimit;
    }
if (graphUpperLimit == -BIGDOUBLE)
    graphUpperLimit = graphLowerLimit = NAN;

// set same common graphLimits in all windows.
for(w=windows,tg=tgSave; w; w=w->next,tg=tg->nextWindow)
    {
    tg->graphUpperLimit = graphUpperLimit;
    tg->graphLowerLimit = graphLowerLimit;
    }
}

void lollyMethods(struct track *track, struct trackDb *tdb, 
                                int wordCount, char *words[])
/* bigLolly track type methods */
{
commonBigBedMethods(track, tdb, wordCount, words);

struct lollyCartOptions *lollyCart = lollyCartOptionsNew(cart, track, wordCount, words);

lollyCart->typeWordCount = wordCount;
AllocArray(lollyCart->typeWords, wordCount);
int ii;
for(ii=0; ii < wordCount; ii++)
    lollyCart->typeWords[ii] = cloneString(words[ii]);
track->loadItems = lollyLoadItems;
track->drawItems = lollyDrawItems;
track->totalHeight = lollyHeight; 
track->preDrawMultiRegion = lollyRegionGraphLimits;
track->drawLeftLabels = lollyLeftLabels;
track->lollyCart = lollyCart;
}
