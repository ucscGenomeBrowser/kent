/* A container for multiple wiggles with a couple of options for combining them. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "dystring.h"
#include "hdb.h"
#include "hgTracks.h"
#include "container.h"
#include "wiggle.h"
#include "wigCommon.h"
#include "hui.h"

struct floatPic
/* A picture that stores RGB values in floating point. */
    {
    float **lines;   /* points to start of each line of image */
    float *image;
    int width;	/* Width in pixels. */
    int height; /* Heigh in pixels. */
    };

struct floatPic *floatPicNew(int width, int height)
/* Return a new floatPic. */
{
long lineSize = 3L * width;
long imageSize = lineSize * height;
struct floatPic *pic = needMem(sizeof(struct floatPic));
pic->width = width;
pic->height = height;
pic->image = needHugeMem(imageSize * sizeof(float));

/* Create and initialize line start array */
AllocArray(pic->lines, height);
int i = height;
float *line = pic->image;
float **lines = pic->lines;
while (--i >= 0)
    {
    *lines++ = line;
    line += lineSize;
    }
return pic;
}

void floatPicFree(struct floatPic **pPic)
/* Free up resources associated with floatPic. */
{
struct floatPic *pic = *pPic;
if (pic != NULL)
    {
    freeMem(pic->lines);
    freeMem(pic->image);
    freez(pPic);
    }
}

void floatPicSet(struct floatPic *pic, float r, float g, float b)
/* Set full image to a single color */
{
long totalSize = pic->width * pic->height;
float *p = pic->image;
while (--totalSize >= 0)
    {
    *p++ = r;
    *p++ = g;
    *p++ = b;
    }
}

// The value below is useful when scaling between floating point 0-1 representation
// and 0-255 fit-in-a-byte representation.  If you use 256, then a valid 1.0 wraps
// to an invalid 256 value in the byte.   If you use 255 then even 0.999999 maps
// to 254, which is a waste of space.
#define FLOAT_FOR_BIGGEST_BYTE  255.9

void vLineViaFloat(void *image, int x, int y, int height, Color color)
/* A vertical line drawer that works via floatPic. */
{
struct floatPic *pic = image;

/* First do some clipping */
if (x < 0 || x > pic->width)
    return;
if (y < 0)
    {
    height += y;
    y = 0;
    }
int yEnd = y + height;
if (yEnd > pic->height)
    {
    yEnd = pic->height;
    height = yEnd - y;
    }
if (height <= 0)
    return;

int hOffset = x*3;
const float scaleColor = 1/FLOAT_FOR_BIGGEST_BYTE;

float r = COLOR_32_RED(color) * scaleColor;
float g = COLOR_32_GREEN(color) * scaleColor;
float b = COLOR_32_BLUE(color) * scaleColor;
while (--height >= 0)
    {
    float *p = pic->lines[y++] + hOffset;
    p[0] *= r;
    p[1] *= g;
    p[2] *= b;
    }
}

struct wigGraphOutput *wigGraphOutputTransparent(struct floatPic *image)
/* Get appropriate wigGraphOutput for non-transparent rendering */
{
struct wigGraphOutput *wgo;
AllocVar(wgo);
wgo->image = image;
wgo->vLine = vLineViaFloat;
return wgo;
}

static void reverseLineOfColors(Color *line, int length)
/* Reverse order of colors in line. */
{
long halfLen = (length>>1);
Color *end = line+length;
Color c;
while (--halfLen >= 0)
    {
    c = *line;
    *line++ = *--end;
    *end = c;
    }
}

void floatPicIntoHvg(struct floatPic *pic, int xOff, int yOff, struct hvGfx *hvg)
/* Copy float pic into hvg image at given offset. */
{
int width = pic->width, height = pic->height;
Color *lineBuf;
AllocArray(lineBuf, width);
int y;
for (y=0; y<height; ++y)
    {
    float *fp = pic->lines[y];
    Color *cp = lineBuf;
    int i = width;
    while (--i >= 0)
        {
	int red = fp[0]*FLOAT_FOR_BIGGEST_BYTE;
	int green = fp[1]*FLOAT_FOR_BIGGEST_BYTE;
	int blue = fp[2]*FLOAT_FOR_BIGGEST_BYTE;
	*cp++ = MAKECOLOR_32(red, green, blue);
	fp += 3;
	}
    if (hvg->rc)
        reverseLineOfColors(lineBuf, width);
    hvGfxVerticalSmear(hvg, xOff, y + yOff, width, 1, lineBuf, TRUE);
    }
freez(&lineBuf);
}


static void minMaxVals(struct slRef *refList, double *retMin, double *retMax,
     enum wiggleWindowingEnum windowingFunction,enum wiggleAlwaysZeroEnum alwaysZero, double *yOffsets)
/* Figure out min/max of everything in list.  The refList contains pointers to
 * preDrawContainers. Calculates the yOffsets used to make the stacked-graph overlay mode. */
{
/* Turns out to be *much* shorter to rewrite than to reuse preDrawAutoScale */
double max = -BIGDOUBLE, min = BIGDOUBLE;
struct slRef *ref;
int numTrack = 0;
for (ref = refList; ref != NULL; numTrack++,ref = ref->next)
    {
    struct preDrawContainer *pre = ref->val;
    if (pre == NULL)  // pre may be null if the bigWig file didn't load
	continue;

    struct preDrawElement *p = pre->preDraw + pre->preDrawZero;
    int width = pre->width;
    int i;
    int offset = numTrack * pre->width;
    int prevOffset = (numTrack - 1) * pre->width;
    for (i=0; i<width; ++i, offset++,prevOffset++)
	{
	double val = p->smooth;

	if (p->count)
	    {
	    if (yOffsets)
		yOffsets[offset] = val;

            if ((numTrack == 0) || (yOffsets == NULL))
		{
		if (min > val) min = val;
		if (max < val) max = val;
		}
	    }
	else if (yOffsets)
	    {
	    yOffsets[offset] = 0;
	    }
	if (yOffsets && (numTrack > 0))
	    {
	    yOffsets[offset] += yOffsets[prevOffset];
	    if (min > yOffsets[offset]) min = yOffsets[offset];
	    if (max < yOffsets[offset]) max = yOffsets[offset];
	    }
	++p;
	}
    }
if (alwaysZero == wiggleAlwaysZeroOn)
    {
    if ( max < 0)
	max = 0.0;
    else if ( min > 0)
	min = 0.0;
    }
*retMax = max;
*retMin = min;
}


void AllocPixelBins(struct wigGraphOutput *wgo, int width)
{
struct pixelCountBin *bins;

AllocVar(bins);

wgo->pixelBins = bins;
bins->binSize = 1;
bins->binCount = width / bins->binSize + 1;
AllocArray(bins->bins, bins->binCount);
}

struct wigGraphOutput *setUpWgo(int xOff, int yOff, int width, int height, int numTracks, struct wigCartOptions *wigCart, struct hvGfx *hvg)
{
/* Deal with tranparency possibly */
struct wigGraphOutput *wgo = NULL;
struct floatPic *floatPic = NULL;
switch(wigCart->aggregateFunction)
    {
    case wiggleAggregateTransparent:
	{
	//int height = tg->lineHeight;
	floatPic = floatPicNew(width, height);
	floatPicSet(floatPic, 1, 1, 1);
	wgo = wigGraphOutputTransparent(floatPic);
	break;
	}
    case wiggleAggregateSubtract:
    case wiggleAggregateNone:
    case wiggleAggregateAdd:
    case wiggleAggregateSolid:
	{
	wgo = wigGraphOutputSolid(xOff, yOff, hvg);
	break;
	}
    case wiggleAggregateStacked:
	{
	wgo = wigGraphOutputStack(xOff, yOff, width, numTracks, hvg);
	break;
	}
    default:
	{
	errAbort("bad aggregate function (value: %d)\n", wigCart->aggregateFunction);
	break;
	}
    }
AllocPixelBins(wgo, width);
return wgo;
}


static void multiWigPreDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Pre-Draw multiWig container calls preDraw on all subtracks. */
{
int y = yOff;  // The y value here should not matter.
struct track *subtrack;

struct wigCartOptions *wigCart = tg->wigCartData;

// we want to the order to be the same in all the modes 
if (wigCart->aggregateFunction == wiggleAggregateStacked)
    slReverse(&tg->subtracks);

int numTracks = 0;
// determine if any subtracks had errors and count them up
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack) )
	{
	numTracks++;
	}
    }

struct wigGraphOutput *wgo = setUpWgo(xOff, yOff, width, tg->height, numTracks, wigCart, hvg);
tg->wigGraphOutput = wgo;

// Cope with autoScale and stacked bars - we do it here rather than in the child tracks, so that
// all children can be on same scale.
double minVal, maxVal;

// Force load of all predraw arrays so can do calcs. Build up list, and then
// figure out max/min.  No worries about multiple loading, the loaders protect
// themselves. 
struct slRef *refList = NULL;
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
	{
	struct preDrawContainer *pre = subtrack->loadPreDraw(subtrack, seqStart, seqEnd, width);

	if (pre != NULL)  // pre maybe null if the load fails
	    {
	    pre->skipAutoscale = TRUE;
	    subtrack->preDrawItems(subtrack, seqStart, seqEnd, hvg, xOff, y, width, font, color, vis);
	    refAdd(&refList, pre);
	    }
	}
    }
slReverse(&refList);

minMaxVals(refList, &minVal, &maxVal, wigCart->windowingFunction,  wigCart->alwaysZero, wgo->yOffsets);
slFreeList(&refList);

if (!wigCart->autoScale)
    {
    minVal = wigCart->minY;
    maxVal = wigCart->maxY;
    }

// Loop through again setting up the wigCarts of the children to have minY/maxY for
// our limits and autoScale off.
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
	{
	struct wigCartOptions *wigCart = subtrack->wigCartData;
	wigCart->minY = minVal;
	wigCart->maxY = maxVal;
	wigCart->autoScale = wiggleScaleManual;
	struct preDrawContainer *pre = subtrack->preDrawContainer;
	if (pre != NULL)  // pre maybe null if the load fails
	    {
	    pre->graphUpperLimit = maxVal;
	    pre->graphLowerLimit = minVal;
	    }
	}
    }
}

static void multiWigMultiRegionGraphLimits(struct track *tg)
/* Set graphLimits for subtracks. */
{
struct track *subtrack;

double graphUpperLimit = -BIGDOUBLE;
double graphLowerLimit = BIGDOUBLE;
struct window *w;
struct track *tgs;

for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
	{
	// find common graphLimits across all windows.
	for(w=windows,tgs=subtrack; w; w=w->next,tgs=tgs->nextWindow)
	    {
	    struct preDrawContainer *pre = tgs->preDrawContainer;
	    if (pre != NULL)  // pre maybe null if the load fails
		{
		if (pre->graphUpperLimit > graphUpperLimit)
		    graphUpperLimit = pre->graphUpperLimit;
		if (pre->graphLowerLimit < graphLowerLimit)
		    graphLowerLimit = pre->graphLowerLimit;
		}
	    }
	break;  // can exit since the rest of the subtracks have been set to the same limits.
	}
    }

// set same common graphLimits across all subtracks.
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
	{
	// set same common graphLimits across all windows.
	for(w=windows,tgs=subtrack; w; w=w->next,tgs=tgs->nextWindow)
	    {
	    struct wigCartOptions *wigCart = tgs->wigCartData;
	    wigCart->minY = graphUpperLimit;
	    wigCart->maxY = graphLowerLimit;
	    struct preDrawContainer *pre = tgs->preDrawContainer;
	    if (pre != NULL)  // pre maybe null if the load fails
		{
		pre->graphUpperLimit = graphUpperLimit;
		pre->graphLowerLimit = graphLowerLimit;
		}
	    tgs->graphUpperLimit = graphUpperLimit;
	    tgs->graphLowerLimit = graphLowerLimit;
	    }
	}
    }
}

static void mergeWiggles(struct track *tg, boolean add)
{
struct preDrawContainer *firstPre = tg->preDrawContainer;

struct track *firstTrack = tg;
//struct wigCartOptions *wigCart = (struct wigCartOptions *) tg->wigCartData;
int ii;

for(tg = tg->next; tg; tg = tg->next)
    {
    struct preDrawContainer *pre = tg->preDrawContainer;

    for(ii=firstPre->preDrawZero; ii < firstPre->preDrawZero + firstPre->width; ii++)
        {
        struct preDrawElement *firstPreDraw = &firstPre->preDraw[ii];
        struct preDrawElement *thisPreDraw = &pre->preDraw[ii];
        if (add)
            firstPreDraw->smooth += thisPreDraw->smooth;
        else
            firstPreDraw->smooth -= thisPreDraw->smooth;
        }
    }
//if (wigCart->autoScale == wiggleScaleAuto)
    {
    double upperLimit = wigEncodeStartingUpperLimit;
    double lowerLimit = wigEncodeStartingLowerLimit;
    for(ii=firstPre->preDrawZero; ii < firstPre->preDrawZero + firstPre->width; ii++)
        {
        struct preDrawElement *firstPreDraw = &firstPre->preDraw[ii];
        if (firstPreDraw->smooth < lowerLimit)
            lowerLimit = firstPreDraw->smooth;
        if (firstPreDraw->smooth > upperLimit)
            upperLimit = firstPreDraw->smooth;
        }
    firstTrack->graphLowerLimit = lowerLimit;
    firstTrack->graphUpperLimit = upperLimit;
    firstPre->graphLowerLimit = lowerLimit;
    firstPre->graphUpperLimit = upperLimit;
    }
}

static void multiWigDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw items in multiWig container. */
{
struct track *subtrack;
boolean errMsgShown = FALSE;
int y = yOff;
boolean errMsgFound = FALSE;
// determine if any subtracks had errors and count them up
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack) )
	{
	if (subtrack->networkErrMsg)
	    errMsgFound = TRUE;
	}
    }

if (errMsgFound && currentWindow==windows)  // first window
    {
    int clipXBak, clipYBak, clipWidthBak, clipHeightBak;
    hvGfxGetClip(hvg, &clipXBak, &clipYBak, &clipWidthBak, &clipHeightBak);
    hvGfxUnclip(hvg);
    hvGfxSetClip(hvg, fullInsideX, yOff, fullInsideWidth, tg->height);
    // use the height of the multiWig 
    Color yellow = hvGfxFindRgb(hvg, &undefinedYellowColor);
    hvGfxBox(hvg, fullInsideX, yOff, fullInsideWidth, tg->height, yellow);
    hvGfxUnclip(hvg);
    hvGfxSetClip(hvg, clipXBak, clipYBak, clipWidthBak, clipHeightBak);
    }

struct wigCartOptions *wigCart = tg->wigCartData;
struct wigGraphOutput *wgo = tg->wigGraphOutput;

if (wigCart->aggregateFunction == wiggleAggregateAdd || wigCart->aggregateFunction == wiggleAggregateSubtract)
    {
    mergeWiggles(tg->subtracks, wigCart->aggregateFunction == wiggleAggregateAdd);
    tg->subtracks->next = NULL;
    }
int numTrack = 0;
int height = tg->totalHeight(tg, vis); // use the parent track for the height
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
	{
        if (!subtrack->networkErrMsg || !errMsgShown)
	    {
	    if (subtrack->networkErrMsg)
	       errMsgShown = TRUE;
	    wgo->numTrack = numTrack++;
	    subtrack->wigGraphOutput = wgo;
	    hvGfxSetClip(hvg, xOff, y, width, height);
	    if (wigCart->aggregateFunction != wiggleAggregateNone)
		subtrack->lineHeight = tg->lineHeight;
	    subtrack->drawItems(subtrack, seqStart, seqEnd, hvg, xOff, y, width, font, color, vis);
	    if (wigCart->aggregateFunction == wiggleAggregateNone)
		{
		y += height + 1;
		wgo->yOff = y;
		}
	    hvGfxUnclip(hvg);
	    }
	}
    }

if (wigCart->aggregateFunction == wiggleAggregateTransparent)
   {
   floatPicIntoHvg(wgo->image, xOff, yOff, hvg);
   floatPicFree((struct floatPic **)&wgo->image);
   }

char *url = trackUrl(tg->track, chromName);
mapBoxHgcOrHgGene(hvg, seqStart, seqEnd, xOff, yOff, width, tg->height, tg->track, tg->track, NULL,
	      url, TRUE, NULL);
}

static int multiWigTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return total height of multiWigcontainer. */
{
struct wigCartOptions *wigCart = tg->wigCartData;
int totalHeight =  0;
if (wigCart->aggregateFunction !=  wiggleAggregateNone)
    totalHeight =  wigTotalHeight(tg, vis);                                                        
struct track *subtrack;
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
	{
	// Logic is slightly complicated by fact we want to call the totalHeight
	// method for each subtrack even if in overlay mode.
	int oneHeight = subtrack->totalHeight(subtrack, vis);
	if (wigCart->aggregateFunction ==  wiggleAggregateNone)
	    {
	    if (totalHeight != 0)
	       totalHeight += 1;
	    totalHeight += oneHeight;
	    }
	}
    }
tg->height = totalHeight;
return totalHeight;
}

static boolean graphLimitsAllSame(struct track *trackList, struct track **retFirstTrack)
/* Return TRUE if graphUpperLimit and graphLowerLimit same for all tracks. */
{
struct track *firstTrack = NULL;
struct track *track;
for (track = trackList; track != NULL; track = track->next)
    {
    if (isSubtrackVisible(track) && !track->networkErrMsg)
        {
	if (firstTrack == NULL)
	    *retFirstTrack = firstTrack = track;
	else if (track->graphUpperLimit != firstTrack->graphUpperLimit 
	    || track->graphLowerLimit != firstTrack->graphLowerLimit)
	    return FALSE;
	}
    }
return firstTrack != NULL;
}

static void multiWigLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
/* Draw left labels - by deferring to first subtrack. */
{
struct wigCartOptions *wigCart = tg->wigCartData;
if (wigCart->aggregateFunction != wiggleAggregateNone)
    {
    struct track *firstVisibleSubtrack = NULL;
    boolean showNumbers = graphLimitsAllSame(tg->subtracks, &firstVisibleSubtrack);
    struct track *subtrack = (showNumbers ? firstVisibleSubtrack : tg->subtracks);
    wigLeftAxisLabels(tg, seqStart, seqEnd, hvg, xOff, yOff, width, height, withCenterLabels,
	    font, color, vis, tg->shortLabel, subtrack->graphUpperLimit, 
	    subtrack->graphLowerLimit, showNumbers);
    }
else
    {
    struct track *subtrack;
    int y = yOff;
    if (withCenterLabels)
       y += tl.fontHeight+1;
    for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
	{
	if (isSubtrackVisible(subtrack))
	    {
	    int height = subtrack->totalHeight(subtrack, vis);
	    if (vis == tvDense)
	        {
		/* Avoid wigLeftAxisLabels here because it will repeatedly add center label 
		 * offsets, and in dense mode we will only draw the one label. */
		hvGfxTextRight(hvg, xOff, y, width - 1, height, subtrack->ixColor, font, 
			subtrack->shortLabel);
		}
	    else
		{
                // black labels for readability 
		wigLeftAxisLabels(subtrack, seqStart, seqEnd, hvg, xOff, y, width, height, 
			withCenterLabels, font, MG_BLACK, vis, subtrack->shortLabel, 
			subtrack->graphUpperLimit, subtrack->graphLowerLimit, TRUE);
		}
	    y += height+1;
	    }
	}
    }
}

void multiWigLoadItems(struct track *track)
/* Load multiWig items. */
{
containerLoadItems(track);
struct track *subtrack;
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    subtrack->mapsSelf = FALSE;	/* Round about way to tell wig not to do own mapping. */
    }
}

void multiWigContainerMethods(struct track *track)
/* Override general container methods for multiWig. */
{
track->syncChildVisToSelf = TRUE;
track->loadItems = multiWigLoadItems;
track->totalHeight = multiWigTotalHeight;
track->preDrawItems = multiWigPreDraw;
track->preDrawMultiRegion = multiWigMultiRegionGraphLimits;
track->drawItems = multiWigDraw;
track->drawLeftLabels = multiWigLeftLabels;
}
