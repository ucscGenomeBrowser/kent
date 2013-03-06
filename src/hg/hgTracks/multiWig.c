/* A container for multiple wiggles with a couple of options for combining them. */

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
const float scaleColor = 1/255.9;

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
char c;
while (--halfLen >= 0)
    {
    c = *line;
    *line++ = *--end;
    *end = c;
    }
}

void floatPicIntoHvg(struct floatPic *pic, int xOff, int yOff, struct hvGfx *hvg)
/* Copy float pic into hvg at given offset. */
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
	int red = fp[0]*255.9;
	int green = fp[1]*255.9;
	int blue = fp[2]*255.9;
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
     enum wiggleAlwaysZeroEnum alwaysZero)
/* Figure out min/max of everything in list.  The refList contains pointers to
 * preDrawContainers */
{
/* Turns out to be *much* shorter to rewrite than to reuse preDrawAutoScale */
double max = -BIGDOUBLE, min = BIGDOUBLE;
struct slRef *ref;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct preDrawContainer *pre = ref->val;
    struct preDrawElement *p = pre->preDraw + pre->preDrawZero;
    int width = pre->width;
    int i;
    for (i=0; i<width; ++i)
	{
	if (p->count)
	    {
	    if (min > p->min) min = p->min;
	    if (max < p->max) max = p->max;
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

static void multiWigDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw items in multiWig container. */
{
struct track *subtrack;
char *aggregate = wigFetchAggregateValWithCart(cart, tg->tdb);
boolean overlay = wigIsOverlayTypeAggregate(aggregate);
boolean errMsgShown = FALSE;
int y = yOff;
boolean errMsgFound = FALSE;
// determine if any subtracks had errors
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack) && subtrack->networkErrMsg)
	errMsgFound = TRUE;
    }
if (errMsgFound)
    {
    Color yellow = hvGfxFindRgb(hvg, &undefinedYellowColor);
    hvGfxBox(hvg, xOff, yOff, width, tg->height, yellow);
    }

/* Cope with autoScale - we do it here rather than in the child tracks, so that
 * all children can be on same scale. */
struct wigCartOptions *wigCart = tg->extraUiData;
if (wigCart->autoScale)
    {
    /* Force load of all predraw arrays so can do calcs. Build up list, and then
     * figure out max/min.  No worries about multiple loading, the loaders protect
     * themselves. */
    struct slRef *refList = NULL;
    for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
        {
	if (isSubtrackVisible(subtrack))
	    {
	    struct preDrawContainer *pre = subtrack->loadPreDraw(subtrack, seqStart, seqEnd, width);
	    refAdd(&refList, pre);
	    }
	}
    double minVal, maxVal;
    minMaxVals(refList, &minVal, &maxVal, wigCart->alwaysZero);
    slFreeList(&refList);

    /* Cope with log transform if need be */
    if (wigCart->transformFunc == wiggleTransformFuncLog)
         {
	 minVal = wiggleLogish(minVal);
	 maxVal = wiggleLogish(maxVal);
	 }

    /* Loop through again setting up the wigCarts of the children to have minY/maxY for
     * our limits and autoScale off. */
    for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
        {
	struct wigCartOptions *wigCart = subtrack->extraUiData;
	wigCart->minY = minVal;
	wigCart->maxY = maxVal;
	wigCart->autoScale = wiggleScaleManual;
	}
    }

/* Deal with tranparency possibly */
struct wigGraphOutput *wgo;
struct floatPic *floatPic = NULL;
if (wigCart->transparent)
    {
    int height = tg->lineHeight;
    floatPic = floatPicNew(width, height);
    floatPicSet(floatPic, 1, 1, 1);
    wgo = wigGraphOutputTransparent(floatPic);
    }
else
    wgo = wigGraphOutputSolid(xOff, yOff, hvg);

for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
	{
        if (!subtrack->networkErrMsg || !errMsgShown)
	    {
	    if (subtrack->networkErrMsg)
	       errMsgShown = TRUE;
	    subtrack->wigGraphOutput = wgo;
	    int height = subtrack->totalHeight(subtrack, vis);
	    hvGfxSetClip(hvg, xOff, y, width, height);
	    if (overlay)
		subtrack->lineHeight = tg->lineHeight;
	    subtrack->drawItems(subtrack, seqStart, seqEnd, hvg, xOff, y, width, font, color, vis);
	    if (!overlay)
		y += height + 1;
	    hvGfxUnclip(hvg);
	    }
	}
    }

if (wigCart->transparent)
   {
   floatPicIntoHvg(floatPic, xOff, yOff, hvg);
   floatPicFree(&floatPic);
   }

char *url = trackUrl(tg->track, chromName);
mapBoxHgcOrHgGene(hvg, seqStart, seqEnd, xOff, y, width, tg->height, tg->track, tg->track, NULL,
	      url, TRUE, NULL);
}

static int multiWigTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return total height of multiWigcontainer. */
{
char *aggregate = wigFetchAggregateValWithCart(cart, tg->tdb);
boolean overlay = wigIsOverlayTypeAggregate(aggregate);
int totalHeight =  0;
if (overlay)                                                                                       
    totalHeight =  wigTotalHeight(tg, vis);                                                        
struct track *subtrack;
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
	{
	// Logic is slightly complicated by fact we want to call the totalHeight
	// method for each subtrack even if in overlay mode.
	int oneHeight = subtrack->totalHeight(subtrack, vis);
	if (!overlay)
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
char *aggregate = wigFetchAggregateValWithCart(cart, tg->tdb);
boolean overlay = wigIsOverlayTypeAggregate(aggregate);
if (overlay)
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
		wigLeftAxisLabels(subtrack, seqStart, seqEnd, hvg, xOff, y, width, height, 
			withCenterLabels, font, subtrack->ixColor, vis, subtrack->shortLabel, 
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
track->drawItems = multiWigDraw;
track->drawLeftLabels = multiWigLeftLabels;
}
