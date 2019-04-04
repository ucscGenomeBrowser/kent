/* lolly -- load and draw lollipops */

/* Copyright (C) 2019 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "obscure.h"
#include "hgTracks.h"
#include "bedCart.h"
#include "bigWarn.h"
#include "lolly.h"
#include "limits.h"

static int trackHeight;

struct lollipop
{
struct lollipop *next;
double val;
unsigned start;
unsigned radius;
unsigned height;
Color color;
};

static void lollipopDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw a list of lollipop structures. */
{
double scale = scaleForWindow(width, seqStart, seqEnd);
struct lollipop *popList = tg->items, *pop;

if (popList == NULL)
    return;

for (pop = popList; pop; pop = pop->next)
    {
    int sx = ((pop->start - seqStart) + .5) * scale; // x coord of center (lower region)
    hvGfxLine(hvg, sx, yOff + trackHeight, sx , yOff+(trackHeight - pop->height), pop->color);
    hvGfxCircle(hvg, sx, yOff + trackHeight - pop->radius - pop->height, pop->radius, pop->color, TRUE);
    }
}

void lollipopLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
{
}


static int lollipopHeight(struct track *tg, enum trackVisibility vis)
/* calculate height of all the lollipops being displayed */
{
if ( tg->visibility == tvDense)
    return  tl.fontHeight;
trackHeight = 5 * tl.fontHeight;
return 5 * tl.fontHeight;
}

static void lollipopMapItem(struct track *tg, struct hvGfx *hvg, void *item, char *itemName, char *mapItemName, int start, int end,
                                                                int x, int y, int width, int height)
{
}
                                                               

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

int cmpHeight(const void *va, const void *vb)
{
const struct lollipop *a = *((struct lollipop **)va);
const struct lollipop *b = *((struct lollipop **)vb);
return a->height - b->height;
}

void lollipopLoadItems(struct track *tg)
{
struct lm *lm = lmInit(0);
struct bbiFile *bbi =  fetchBbiForTrack(tg);
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chromName, winStart, winEnd, 0, lm);
char *bedRow[bbi->fieldCount];
char startBuf[16], endBuf[16];
struct lollipop *popList = NULL, *pop;
unsigned lollipopField = atoi(trackDbSetting(tg->tdb, "lollipopField"));
double sumData = 0.0, sumSquares = 0.0;
//double minVal = DBL_MAX, maxVal = -DBL_MAX;
unsigned count = 0;
                    
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    AllocVar(pop);
    slAddHead(&popList, pop);
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    double val = atof(bedRow[lollipopField]);
    pop->val = val;
    pop->start = atoi(bedRow[1]);
    count++;
    sumData += val;
    sumSquares += val * val;
    }

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
slSort(&popList, cmpHeight);
tg->items = popList;
}

void lollipopMethods(struct track *track, struct trackDb *tdb, 
                                int wordCount, char *words[])
/* Interact track type methods */
{
char *ourWords[2];
ourWords[0] = "bigBed";
ourWords[1] = "4";
bigBedMethods(track, tdb,2,ourWords);
if (tdb->visibility == tvDense)
    return;
track->loadItems = lollipopLoadItems;
track->drawItems = lollipopDrawItems;
track->totalHeight = lollipopHeight; 
track->drawLeftLabels = lollipopLeftLabels;
track->mapItem = lollipopMapItem;
}
