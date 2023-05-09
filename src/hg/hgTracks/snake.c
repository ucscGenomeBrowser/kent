/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

/* snakeTrack - stuff to load and display snake type tracks in browser.  */

#include "common.h"
#include "hash.h"
#include "localmem.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "chainBlock.h"
#include "chainLink.h"
#include "chainDb.h"
#include "chainCart.h"
#include "errCatch.h"
#include "twoBit.h"
#include "bigWarn.h"
#include <pthread.h>
#include "trackHub.h"
#include "limits.h"
#include "snakeUi.h"
#include "bits.h"
#include "trix.h"
#include "chromAlias.h"

#include "bigPsl.h"
#include "snake.h"

typedef int (*compareFunction )(const void *elem1,  const void *elem2);

static int snakeFeatureCmpScore(const void *va, const void *vb)
/* sort by score of the alignment. */
{
const struct snakeFeature *a = *((struct snakeFeature **)va);
const struct snakeFeature *b = *((struct snakeFeature **)vb);
int diff = b->score - a->score;

return diff;
}

static int snakeFeatureCmpTStart(const void *va, const void *vb)
/* sort by start position on the target sequence */
{
const struct snakeFeature *a = *((struct snakeFeature **)va);
const struct snakeFeature *b = *((struct snakeFeature **)vb);
int diff = a->start - b->start;

if (diff == 0)
    {
    diff = a->qStart - b->qStart;
    }

return diff;
}

static int snakeFeatureCmpQStart(const void *va, const void *vb)
/* sort by start position on the query sequence */
{
const struct snakeFeature *a = *((struct snakeFeature **)va);
const struct snakeFeature *b = *((struct snakeFeature **)vb);
int diff = a->qStart - b->qStart;

if (diff == 0)
    {
    diff = a->start - b->start;
    }

return diff;
}

// static machinery to calculate full and pack snake

#define NUM_LEVELS	1000

struct level
{
boolean init;		/* has this level been initialized */
int orientation;	/* strand.. see above */
unsigned long edge;	/* the leading edge of this level */
int adjustLevel;	/* used to compress out the unused levels */
boolean hasBlock;	/* are there any blocks in this level */

// these fields are used to compact full snakes
unsigned *connectsToLevel;  /* what levels does this level connect to */
Bits *pixels;               /* keep track of what pixels are used */
struct snakeFeature *blocks; /* the ungapped blocks attached to this level */
};

static struct level Levels[NUM_LEVELS]; /* for packing the snake, not re-entrant! */
static int maxLevel = 0;     	     /* deepest level */	

/* blocks that make it through the min size filter */
static struct snakeFeature *newList = NULL;   

static void clearLevels()
/* clear out the data structure that we use to calculate snakes */
{
int ii;

for(ii=0; ii < sizeof(Levels) / sizeof(Levels[0]); ii++)
    Levels[ii].init = FALSE;
maxLevel = 0;
}

static void calcFullSnakeHelper(struct snakeFeature *list, int level)
// calculate a full snake
// updates global newList with unsorted blocks that pass the min
// size filter.
{
struct snakeFeature *cb = list;
struct snakeFeature *proposedList = NULL;

if (level > maxLevel)
    maxLevel = level;
if (level > ArraySize(Levels))
    errAbort("too many levels");

if (Levels[level].init == FALSE)
    {
    // initialize the level if this is the first time we've seen it
    Levels[level].init = TRUE;
    Levels[level].orientation = list->orientation;
    if (list->orientation == -1)
	Levels[level].edge = LONG_MAX; // bigger than the biggest chrom
    else
	Levels[level].edge = 0;
    }

// now we step through the blocks and assign them to levels
struct snakeFeature *next;
struct snakeFeature *insertHead = NULL;
for(; cb; cb = next)
    {
    // we're going to add this block to a different list 
    // so keep track of the next pointer
    next = cb->next;

    // if this block can't fit on this level add to the insert
    // list and move on to the next block
    if ((Levels[level].orientation != cb->orientation) ||
	((cb->orientation == 1) && (Levels[level].edge > cb->start)) ||
	((cb->orientation == -1) && (Levels[level].edge < cb->end)))
	{
	// add this to the list of an insert
	slAddHead(&insertHead, cb);
	continue;
	}

    // the current block will fit on this level
    // go ahead and deal with the blocks that wouldn't 
    if (insertHead)
	{
	// we had an insert, go ahead and calculate where that goes
	slReverse(&insertHead);
	calcFullSnakeHelper(insertHead, level + 1);
	insertHead = NULL;
	}

    // assign the current block to this level
    cb->level = level;

    if (cb->orientation == 1)
	Levels[level].edge = cb->end;
    else
	Levels[level].edge = cb->start;

    // add this block to the list of proposed blocks for this level
    slAddHead(&proposedList, cb);
    }

// we're at the end of the list.  Deal with any blocks
// that didn't fit on this level
if (insertHead)
    {
    slReverse(&insertHead);
    int nextLevel = level + 1;
    calcFullSnakeHelper(insertHead, nextLevel);
    insertHead = NULL;
    }

// do we have any proposed blocks
if (proposedList == NULL) 
    return;

// we parsed all the blocks in the list to see if they fit in our level
// now let's see if the list of blocks for this level is big enough
// to actually add

struct snakeFeature *temp;
double scale = scaleForWindow(insideWidth, winStart, winEnd);
int start, end;

// order the blocks so lowest start position is first
if (Levels[level].orientation == 1)
    slReverse(&proposedList);

start=proposedList->start;
for(temp=proposedList; temp->next; temp = temp->next)
    ;
end=temp->end;

// calculate how big the string of blocks is in screen coordinates
double apparentSize = scale * (end - start);

if (apparentSize < 0)
    errAbort("size of a list of blocks should not be less than zero");

// check to see if the apparent size is big enough
if (apparentSize < 1)
    return;

// transfer proposedList to new block list
for(temp=proposedList; temp; temp = next)
    {
    next = temp->next;
    temp->next = NULL;
    slAddHead(&newList, temp);
    }
}

static void freeFullLevels()
// free the connection levels and bitmaps for each level
{
int ii;

for(ii=0; ii <= maxLevel; ii++)
    {
    freez(&Levels[ii].connectsToLevel);
    bitFree(&Levels[ii].pixels);
    }
}

static void initializeFullLevels(struct snakeFeature *sfList)
/* initialize levels for compact snakes */
{
int ii;

for(ii=0; ii <= maxLevel; ii++)
    {
    Levels[ii].connectsToLevel = needMem((maxLevel+1) * sizeof(unsigned));
    Levels[ii].pixels = bitAlloc(insideWidth);
    Levels[ii].blocks = NULL;
    }

struct snakeFeature *sf, *next;
int prevLevel = -1;
double scale = scaleForWindow(insideWidth, winStart, winEnd);

for(sf=sfList; sf; sf = next)
    {
    next = sf->next;

    if (positiveRangeIntersection(sf->start, sf->end, winStart, winEnd))
	{
	int sClp = (sf->start < winStart) ? winStart : sf->start;
	sf->pixX1 = round((sClp - winStart)*scale);
	int eClp = (sf->end > winEnd) ? winEnd : sf->end;
	sf->pixX2 = round((eClp - winStart)*scale);
	}
    else
	{
	sf->pixX1 = sf->pixX2 = 0;
	}

    bitSetRange(Levels[sf->level].pixels, sf->pixX1, sf->pixX2 - sf->pixX1);

    if (prevLevel != -1)
	Levels[sf->level].connectsToLevel[prevLevel] = 1;

    if(next != NULL)
	Levels[sf->level].connectsToLevel[next->level] = 1;

    prevLevel = sf->level;

    slAddHead(&Levels[sf->level].blocks, sf);
    }
}

static int highestLevelBelowUs(int level)
// is there a level below us that doesn't connect to us
{
int newLevel;

// find the highest level below us that we connect to
for(newLevel = level - 1; newLevel >= 0; newLevel--)	 
    if (Levels[level].connectsToLevel[newLevel])
	break;

// if the next level is more than one below us, we may
// be able to push our blocks to it.
if ((newLevel < 0) || (newLevel + 1 == level))
    return -1;

return newLevel + 1;
}

// the number of pixels we need clear on either side of the blocks we push
#define EXTRASPACE	10

static boolean checkBlocksOnLevel(int level, struct snakeFeature *sfList)
// is there enough pixels on this level to hold our blocks?
{
struct snakeFeature *sf;

for(sf=sfList; sf;  sf = sf->next)
    {
    if (bitCountRange(Levels[level].pixels, sf->pixX1 - EXTRASPACE, (sf->pixX2 - sf->pixX1) + 2*EXTRASPACE))
	return FALSE;
    }
return TRUE;
}

static void collapseLevel(int oldLevel, int newLevel)
// push level up to higher level
{
struct snakeFeature *sf;
struct snakeFeature *next;

for(sf=Levels[oldLevel].blocks; sf;  sf = next)
    {
    next = sf->next;
    sf->level = newLevel;
    slAddHead(&Levels[newLevel].blocks, sf);

    bitSetRange(Levels[sf->level].pixels, sf->pixX1, sf->pixX2 - sf->pixX1);
    }
Levels[oldLevel].blocks = NULL;
Levels[oldLevel].pixels = bitAlloc(insideWidth);
}

static void remapConnections(int oldLevel, int newLevel)
// if we've moved a level, we need to remap all the connections
{
int ii, jj;

for(ii=0; ii <= maxLevel; ii++)
    {
    for(jj=0; jj <= maxLevel; jj++)
	{
	if (Levels[ii].connectsToLevel[oldLevel])
	    {
	    Levels[ii].connectsToLevel[oldLevel] = 0;
	    Levels[ii].connectsToLevel[newLevel] = 1;
	    }
	}
    }
}

static struct snakeFeature *compactSnakes(struct snakeFeature *sfList)
// collapse levels that will fit on a higer level
{
int ii;

initializeFullLevels(sfList);

// start with third level to see what can be compacted
for(ii=2; ii <= maxLevel; ii++) 
    {
    int newLevel;
    // is there a level below us that doesn't connect to us
    if ((newLevel = highestLevelBelowUs(ii)) > 0)
	{
	int jj;
	for (jj=newLevel; jj < ii; jj++)
	    {
	    if (checkBlocksOnLevel(jj, Levels[ii].blocks))
		{
		collapseLevel(ii, jj);
		remapConnections(ii, jj);
		break;
		}
	    }
	}
    }

// reattach blocks in the levels to linkedFeature
struct snakeFeature *newList = NULL;
for(ii=0; ii <= maxLevel; ii++)
    {
    newList = slCat(Levels[ii].blocks, newList);
    }
slSort(&newList, snakeFeatureCmpQStart);

// figure out the new max
int newMax = 0;
for(ii=0; ii <= maxLevel; ii++)
    if (Levels[ii].blocks != NULL)
	newMax = ii;

freeFullLevels();
maxLevel = newMax;

return newList;
}

static void calcFullSnake(struct track *tg, void *item)
// calculate a full snake
{
struct linkedFeatures  *lf = (struct linkedFeatures *)item;

// if there aren't any blocks, don't bother
if (lf->components == NULL)
    return;

// we use the snakeInfo field to keep track of whether we already
// calculated the height of this snake
if (lf->snakeInfo == NULL)
    {
    clearLevels();
    struct snakeFeature *sf;

    // this will destroy lf->components, and add to newList
    calcFullSnakeHelper((struct snakeFeature *)lf->components, 0);
    lf->components = (struct simpleFeature *)newList;
    newList = NULL;
    slSort(&lf->components, snakeFeatureCmpQStart);

    // now we're going to compress the levels that aren't used
    // to do that, we need to see which blocks are on the screen,
    // or connected to something on the screen
    int oldMax = maxLevel;
    clearLevels();
    struct snakeFeature *prev = NULL;
    for(sf=(struct snakeFeature *)lf->components; sf; prev = sf, sf = sf->next)
	{
	if (Levels[sf->level].init == FALSE)
	    {
	    Levels[sf->level].init = TRUE;
	    Levels[sf->level].orientation = sf->orientation;
	    Levels[sf->level].hasBlock = FALSE;
	    }
	else
	    {
	    if (Levels[sf->level].orientation != sf->orientation)
		errAbort("found snakeFeature with wrong orientation");
	    if ((sf->orientation == 1) && (Levels[sf->level].edge > sf->start)) 
		errAbort("found snakeFeature that violates edge");
	    if ((sf->orientation == -1) && (Levels[sf->level].edge < sf->end))
		errAbort("found snakeFeature that violates edge");
	    }
	if (sf->orientation == 1)
	    Levels[sf->level].edge = sf->end;
	else
	    Levels[sf->level].edge = sf->start;
	if (sf->level > maxLevel)
	    maxLevel = sf->level;
	if(positiveRangeIntersection(winStart, winEnd, sf->start, sf->end))
	    {
	    Levels[sf->level].hasBlock = TRUE;
	    if (sf->next != NULL)
		Levels[sf->next->level].hasBlock = TRUE;
	    if (prev != NULL)
		Levels[prev->level].hasBlock = TRUE;

	    }
	}

    // now figure out how to remap the blocks
    int ii;
    int count = 0;
    for(ii=0; ii < oldMax + 1; ii++)
	{
	Levels[ii].adjustLevel = count;
	if ((Levels[ii].init) && (Levels[ii].hasBlock))
	    count++;
	}
    maxLevel = count;

    // remap blocks
    for(sf=(struct snakeFeature *)lf->components; sf; sf = sf->next)
	sf->level = Levels[sf->level].adjustLevel;

    // now compact the snakes
    lf->components = (void *)compactSnakes((struct snakeFeature *)lf->components);
    struct snakeInfo *si;
    AllocVar(si);
    si->maxLevel = maxLevel;
    lf->snakeInfo = si;
    }
}

static void calcPackSnakeHelper(struct snakeFeature *list, int level)
// calculate a packed snake
// updates global newList with unsorted blocks that pass the min
// size filter.
{
struct snakeFeature *cb = list;
struct snakeFeature *proposedList = NULL;

if (level > maxLevel)
    maxLevel = level;
if (level > ArraySize(Levels))
    errAbort("too many levels");

if (Levels[level].init == FALSE)
    {
    // initialize the level if this is the first time we've seen it
    Levels[level].init = TRUE;
    Levels[level].edge = 0;
    }

// now we step through the blocks and assign them to levels
struct snakeFeature *next;
struct snakeFeature *insertHead = NULL;
for(; cb; cb = next)
    {
    // we're going to add this block to a different list 
    // so keep track of the next pointer
    next = cb->next;

    // if this block can't fit on this level add to the insert
    // list and move on to the next block
    if ( Levels[level].edge > cb->start)
	{
	// add this to the list of an insert
	slAddHead(&insertHead, cb);
	continue;
	}

    // the current block will fit on this level
    // go ahead and deal with the blocks that wouldn't 
    if (insertHead)
	{
	// we had an insert, go ahead and calculate where that goes
	slReverse(&insertHead);
	calcPackSnakeHelper(insertHead, level + 1);
	insertHead = NULL;
	}

    // assign the current block to this level
    cb->level = level;

    Levels[level].edge = cb->end;

    // add this block to the list of proposed blocks for this level
    slAddHead(&proposedList, cb);
    }

// we're at the end of the list.  Deal with any blocks
// that didn't fit on this level
if (insertHead)
    {
    slReverse(&insertHead);
    calcPackSnakeHelper(insertHead,  level + 1);
    insertHead = NULL;
    }

// do we have any proposed blocks
if (proposedList == NULL) 
    return;

// we parsed all the blocks in the list to see if they fit in our level
// now let's see if the list of blocks for this level is big enough
// to actually add

// order the blocks so lowest start position is first
slReverse(&proposedList);
struct snakeFeature *temp;
double scale = scaleForWindow(insideWidth, winStart, winEnd);
int start, end;


start=proposedList->start;
for(temp=proposedList; temp->next; temp = temp->next)
    ;
end=temp->end;

// calculate how big the string of blocks is in screen coordinates
double apparentSize = scale * (end - start);

if (apparentSize < 0)
    errAbort("size of a list of blocks should not be less than zero");

// check to see if the apparent size is big enough
if (apparentSize < 1)
    return;

// transfer proposedList to new block list
for(temp=proposedList; temp; temp = next)
    {
    next = temp->next;
    temp->next = NULL;
    slAddHead(&newList, temp);
    }
}

static void calcPackSnake(struct track *tg, void *item)
{
struct linkedFeatures  *lf = (struct linkedFeatures *)item;
if (lf->components == NULL)
    return;

// we use the snakeIfno field to keep track of whether we already
// calculated the height of this snake
if (lf->snakeInfo == NULL)
    {
    clearLevels();

    // this will destroy lf->components, and add to newList
    calcPackSnakeHelper((struct snakeFeature *)lf->components, 0);
    lf->components = (struct simpleFeature *)newList;
    newList = NULL;
    
    //slSort(&lf->components, snakeFeatureCmpQStart);

    struct snakeInfo *si;
    AllocVar(si);
    si->maxLevel = maxLevel;
    lf->snakeInfo = si;
    }
}

static int snakeItemHeight(struct track *tg, void *item)
// return height of a single packed snake 
{
if ((item == NULL) || (tg->visibility == tvSquish) || (tg->visibility == tvDense)) 
    return 0;

struct linkedFeatures  *lf = (struct linkedFeatures *)item;
if (lf->components == NULL)
    return 0;

if (tg->visibility == tvFull) 
    calcFullSnake(tg, item);
else if (tg->visibility == tvPack) 
    calcPackSnake(tg, item);

struct snakeInfo *si = (struct snakeInfo *)lf->snakeInfo;
int lineHeight = tg->lineHeight ;
int multiplier = 1;

if (tg->visibility == tvFull)
    multiplier = 2;
return (si->maxLevel + 1) * (multiplier * lineHeight);
}

static int snakeHeight(struct track *tg, enum trackVisibility vis)
/* calculate height of all the snakes being displayed */
{
if (tg->networkErrMsg != NULL)
    {
    // we had a parallel load failure
    tg->drawItems = bigDrawWarning;
    tg->totalHeight = bigWarnTotalHeight;
    return bigWarnTotalHeight(tg, vis);
    }

if (vis == tvDense)
    return tg->lineHeight;

if (vis == tvSquish)
    return tg->lineHeight/2;

int height = 0;
struct linkedFeatures *item = tg->items;

for (;item; item = item->next)
    {
    height += tg->itemHeight(tg, item);
    }
return height;
}


//  this is a 16 color palette with every other color being a lighter version of
//  the color before it
static int snakePalette2[] =
{
0x1f77b4, 0xaec7e8, 0xff7f0e, 0xffbb78, 0x2ca02c, 0x98df8a, 0xd62728, 0xff9896, 0x9467bd, 0xc5b0d5, 0x8c564b, 0xc49c94, 0xe377c2, 0xf7b6d2, 0x7f7f7f, 0xc7c7c7, 0xbcbd22, 0xdbdb8d, 0x17becf, 0x9edae5
};


static Color hashColor(char *name)
{
bits32 hashVal = hashString(name);
unsigned int colorInt = snakePalette2[hashVal % (sizeof(snakePalette2)/sizeof(Color))];

return MAKECOLOR_32(((colorInt >> 16) & 0xff),((colorInt >> 8) & 0xff),((colorInt >> 0) & 0xff));
}

static void boundMapBox(struct hvGfx *hvg, int start, int end, int x, int y, int width, int height,
                       char *track, char *item, char *statusLine, char *directUrl, boolean withHgsid,
                       char *extra)
// make sure start x and end x position are on the screen
// otherwise the tracking box code gets confused
{
if (x > insideWidth)
    return;

if (x < 0)
    {
    width -= -x;
    x = 0;
    }

if (x + width > insideWidth)
    {
    width -= x + width - insideWidth;
    }

mapBoxHgcOrHgGene(hvg, start, end, x, y, width, height,
                       track, item, statusLine, directUrl, withHgsid,
                       extra);
}

static void snakeDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw snake items. */
{
struct slList *item;
int y;
struct linkedFeatures  *lf;
double scale = scaleForWindow(width, seqStart, seqEnd);
int height = snakeHeight(tg, vis);

hvGfxSetClip(hvg, xOff, yOff, width, height);

if ((tg->visibility == tvFull) || (tg->visibility == tvPack))
    {
    // score snakes by how many bases they cover
    for (item = tg->items; item != NULL; item = item->next)
	{
	lf = (struct linkedFeatures *)item;
	struct snakeFeature  *sf;

	lf->score = 0;
	for (sf =  (struct snakeFeature *)lf->components; sf != NULL;  sf = sf->next)
	    {
	    lf->score += sf->end - sf->start;
	    }
	}

    slSort(&tg->items, linkedFeaturesCmpScore);
    }

y = yOff;
for (item = tg->items; item != NULL; item = item->next)
    {
    if(tg->itemColor != NULL) 
	color = tg->itemColor(tg, item, hvg);
    tg->drawItemAt(tg, item, hvg, xOff, y, scale, font, color, vis);
    if (vis == tvFull)
	y += tg->itemHeight(tg, item);
    } 
}

static void snakeDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
unsigned showSnpWidth = cartOrTdbInt(cart, tg->tdb, 
    SNAKE_SHOW_SNP_WIDTH, SNAKE_DEFAULT_SHOW_SNP_WIDTH);
struct linkedFeatures  *lf = (struct linkedFeatures *)item;


if (tg->visibility == tvFull) 
    calcFullSnake(tg, item);
else if (tg->visibility == tvPack)
    calcPackSnake(tg, item);

if (lf->components == NULL)
    return;

struct snakeFeature  *sf = (struct snakeFeature *)lf->components, *prevSf = NULL;
int s = tg->itemStart(tg, item);
int sClp = (s < winStart) ? winStart : s;
int x1 = round((sClp - winStart)*scale) + xOff;
int textX = x1;
int yOff = y;
boolean withLabels = (withLeftLabels && (vis == tvFull) && !tg->drawName);
unsigned   labelColor = MG_BLACK;

// draw the labels
if (withLabels)
    {
    char *name = tg->itemName(tg, item);
    int nameWidth = mgFontStringWidth(font, name);
    int dotWidth = tl.nWidth/2;
    boolean snapLeft = FALSE;
    boolean drawNameInverted = FALSE;
    textX -= nameWidth + dotWidth;
    snapLeft = (textX < fullInsideX);
    /* Special tweak for expRatio in pack mode: force all labels
     * left to prevent only a subset from being placed right: */
    snapLeft |= (startsWith("expRatio", tg->tdb->type));
#ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
    if (theImgBox == NULL && snapLeft)
#else///ifndef IMAGEv2_NO_LEFTLABEL_ON_FULL
    if (snapLeft)        /* Snap label to the left. */
#endif ///ndef IMAGEv2_NO_LEFTLABEL_ON_FULL
        {
        textX = leftLabelX;
        assert(hvgSide != NULL);
        hvGfxUnclip(hvgSide);
        hvGfxSetClip(hvgSide, leftLabelX, yOff, fullInsideX - leftLabelX, tg->height);
        if(drawNameInverted)
            {
            int boxStart = leftLabelX + leftLabelWidth - 2 - nameWidth;
            hvGfxBox(hvgSide, boxStart, y, nameWidth+1, tg->heightPer - 1, color);
            hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, tg->heightPer,
                        MG_WHITE, font, name);
            }
        else
            hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, tg->heightPer,
                        labelColor, font, name);
        hvGfxUnclip(hvgSide);
        hvGfxSetClip(hvgSide, insideX, yOff, insideWidth, tg->height);
        }
    else
        {
        int pdfSlop=nameWidth/5;
        hvGfxUnclip(hvg);
        hvGfxSetClip(hvg, textX-1-pdfSlop, y, nameWidth+1+pdfSlop, tg->heightPer);
        if(drawNameInverted)
            {
            hvGfxBox(hvg, textX - 1, y, nameWidth+1, tg->heightPer-1, color);
            hvGfxTextRight(hvg, textX, y, nameWidth, tg->heightPer, MG_WHITE, font, name);
            }
        else
            hvGfxTextRight(hvg, textX, y, nameWidth, tg->heightPer, labelColor, font, name);
        hvGfxUnclip(hvg);
        hvGfxSetClip(hvg, insideX, yOff, insideWidth, tg->height);
        }
    }

// now we're going to draw the boxes

s = sf->start;
int lastE = -1;
int lastS = -1;
int offY = y;
int lineHeight = tg->lineHeight ;
int  qs, qe;
int heightPer = tg->heightPer;
int lastX = -1;
int lastQEnd = 0;
int lastLevel = -1;
int e;
qe = lastQEnd = 0;
for (sf =  (struct snakeFeature *)lf->components; sf != NULL; lastQEnd = qe, prevSf = sf, sf = sf->next)
    {
    qs = sf->qStart;
    qe = sf->qEnd;
    if (vis == tvDense)
	y = offY;
    else if ((vis == tvPack) || (vis == tvSquish))
	y = offY + (sf->level * 1) * lineHeight;
    else if (vis == tvFull)
	y = offY + (sf->level * 2) * lineHeight;
    s = sf->start; e = sf->end;

    int sx, ex;
    if (!positiveRangeIntersection(winStart, winEnd, s, e))
	continue;
    sx = round((double)((int)s-winStart)*scale) + xOff;
    ex = round((double)((int)e-winStart)*scale) + xOff;

    // color by strand
    static Color darkBlueColor = 0;
    static Color darkRedColor = 0;
    if (darkRedColor == 0)
	{
	//the light blue: rgb(149, 204, 252)
	//the light red: rgb(232, 156, 156)
	darkRedColor = hvGfxFindColorIx(hvg, 232,156,156);
	darkBlueColor = hvGfxFindColorIx(hvg, 149,204,252);
	}
    
    char *colorBy = cartOrTdbString(cart, tg->tdb, 
	SNAKE_COLOR_BY, SNAKE_DEFAULT_COLOR_BY);

    extern Color getChromColor(char *name, struct hvGfx *hvg);
    if (sameString(colorBy, SNAKE_COLOR_BY_STRAND_VALUE))
	color = (sf->orientation == -1) ? darkRedColor : darkBlueColor;
    else if (sameString(colorBy, SNAKE_COLOR_BY_CHROM_VALUE))
	color = hashColor(sf->qName);
    else
	color =  darkBlueColor;

    int w = ex - sx;
    if (w == 0) 
	w = 1;
    assert(w > 0);
    char buffer[1024];
	
    if (vis == tvFull)
	safef(buffer, sizeof buffer, "%d-%d",sf->qStart,sf->qEnd);
    else
	safef(buffer, sizeof buffer, "%s:%d-%d",sf->qName,sf->qStart,sf->qEnd);
    if (sx < insideX)
	{
	int olap = insideX - sx;
	sx = insideX;
	w -= olap;
	}
    char qAddress[4096];
    if ((vis == tvFull) || (vis == tvPack) )
	{
	safef(qAddress, sizeof qAddress, "qName=%s&qs=%d&qe=%d&qWidth=%d",sf->qName,  qs, qe,  winEnd - winStart);
	boundMapBox(hvg, s, e, sx+1, y, w-2, heightPer, tg->track,
		    sf->qName, sf->qName, NULL, TRUE, qAddress);
	}
    hvGfxBox(hvg, sx, y, w, heightPer, color);

    // now draw the mismatches if we're at high enough resolution and have the sequence
    char *twoBitString = trackDbSetting(tg->tdb, "twoBit");
    if (twoBitString &&  ((winBaseCount < showSnpWidth) && ((vis == tvFull) || (vis == tvPack))))
	{
	static struct twoBitFile *tbf = NULL;
	static char *lastTwoBitString = NULL;
	static struct dnaSeq *seq = NULL;
	static char *lastQName = NULL;

	// sequence for chain snakes is in 2bit files which we cache
        if ((lastTwoBitString == NULL) ||
            differentString(lastTwoBitString, twoBitString))
            {
            if (tbf != NULL)
                {
                lastQName = NULL;
                twoBitClose(&tbf);
                }
            tbf = twoBitOpen(twoBitString);
            }

        // we're reading in the whole chrom
        if ((lastQName == NULL) || differentString(sf->qName, lastQName))
            seq = twoBitReadSeqFrag(tbf, sf->qName,  0, 0);
        lastQName = sf->qName;
        lastTwoBitString = twoBitString;

	char *ourDna;
        ourDna = &seq->dna[sf->qStart];

	int seqLen = sf->qEnd - sf->qStart;
	toUpperN(ourDna, seqLen);
	if (sf->orientation == -1)
	    reverseComplement(ourDna,seqLen);

	// get the reference sequence
	char *refDna;
        struct dnaSeq *extraSeq = hDnaFromSeq(database, chromName, sf->start, sf->end, dnaUpper);
        refDna = extraSeq->dna;
	int si = s;
	char *ptr1 = refDna;
	char *ptr2 = ourDna;
	for(; si < e; si++,ptr1++,ptr2++)
	    {
	    // if mismatch!  If reference is N ignore, if query is N, paint yellow
	    if ( (*ptr1 != *ptr2) && !((*ptr1 == 'N') || (*ptr1 == 'n')))
		{
		int misX1 = round((double)((int)si-winStart)*scale) + xOff;
		int misX2 = round((double)((int)(si+1)-winStart)*scale) + xOff;
		int w1 = misX2 - misX1;
		if (w1 < 1)
		    w1 = 1;

		Color boxColor = MG_RED;
		if ((*ptr2 == 'N') || (*ptr2 == 'n'))
		    boxColor = hvGfxFindRgb(hvg, &undefinedYellowColor);
		hvGfxBox(hvg, misX1, y, w1, heightPer, boxColor);
		}
	    }

	// if we're zoomed to base level, draw sequence of mismatch
	if (zoomedToBaseLevel)
	    {
	    int mysx = round((double)((int)s-winStart)*scale) + xOff;
	    int myex = round((double)((int)e-winStart)*scale) + xOff;
	    int myw = myex - mysx;
	    spreadAlignString(hvg, mysx, y, myw, heightPer, MG_WHITE, font, ourDna,
		refDna, seqLen, TRUE, FALSE);
	    }

	}
    sf->drawn = TRUE;
    lastLevel = sf->level;
    //lastX = x;
    }

if (vis != tvFull)
    return;

// now we're going to draw the lines between the blocks

lastX = -1;
lastQEnd = 0;
lastLevel = 0;
qe = lastQEnd = 0;
prevSf = NULL;
for (sf =  (struct snakeFeature *)lf->components; sf != NULL; lastQEnd = qe, prevSf = sf, sf = sf->next)
    {
    int y1, y2;
    int sx, ex;
    qs = sf->qStart;
    qe = sf->qEnd;
    if (lastLevel == sf->level)
	{
	y1 = offY + (lastLevel * 2) * lineHeight + lineHeight/2;
	y2 = offY + (sf->level * 2) * lineHeight + lineHeight/2;
	}
    else if (lastLevel > sf->level)
	{
	y1 = offY + (lastLevel * 2 ) * lineHeight;
	y2 = offY + (sf->level * 2 + 1) * lineHeight + lineHeight/3;
	}
    else
	{
	y1 = offY + (lastLevel * 2 + 1) * lineHeight - 1;
	y2 = offY + (sf->level * 2 ) * lineHeight - lineHeight/3;;
	}
    s = sf->start; e = sf->end;

    sx = round((double)((int)s-winStart)*scale) + xOff;
    ex = round((double)((int)e-winStart)*scale) + xOff;
    color = (sf->orientation == -1) ? MG_RED : MG_BLUE;

    if (lastX != -1)
	{
	char buffer[1024];
#define MG_ORANGE  0xff0082E6
	int color = MG_GRAY;

	if (lastQEnd != qs) {
            long long queryInsertSize = llabs(lastQEnd - qs);
        if (queryInsertSize > 100)
            color = MG_ORANGE;
        }
        double queryGapNFrac = 0.0;
        double queryGapMaskedFrac = 0.0;

	// draw the vertical orange bars if there is an insert in the other sequence
	if ((winBaseCount < showSnpWidth) )
	    {
	    if ((sf->orientation == 1) && (qs != lastQEnd) && (lastE == s))
		{
		hvGfxLine(hvg, sx, y2 - lineHeight/2 , sx, y2 + lineHeight/2, MG_ORANGE);
		safef(buffer, sizeof buffer, "%dbp (%.1lf%% N, %.1lf%% masked)", qs - lastQEnd, queryGapNFrac*100, queryGapMaskedFrac*100);
		boundMapBox(hvg, s, e, sx, y2 - lineHeight/2, 1, lineHeight, tg->track,
				    "foo", buffer, NULL, TRUE, NULL);
		safef(buffer, sizeof buffer, "%dbp", qs - lastQEnd);
                //hvGfxTextCentered(hvg, sx - 10, y2 + lineHeight/2, 20, INSERT_TEXT_HEIGHT, MG_ORANGE, font, buffer);
		}
	    else if ((sf->orientation == -1) && (qs != lastQEnd) && (lastS == e))
		{
		hvGfxLine(hvg, ex, y2 - lineHeight/2 , ex, y2 + lineHeight/2, MG_ORANGE);
		safef(buffer, sizeof buffer, "%dbp (%.1lf%% N, %.1lf%% masked)", qs - lastQEnd, queryGapNFrac*100, queryGapMaskedFrac*100);
		boundMapBox(hvg, s, e, ex, y2 - lineHeight/2, 1, lineHeight, tg->track,
				    "foo", buffer, NULL, TRUE, NULL);
		safef(buffer, sizeof buffer, "%dbp", qs - lastQEnd);
                //hvGfxTextCentered(hvg, ex - 10, y2 + lineHeight/2, 20, INSERT_TEXT_HEIGHT, MG_ORANGE, font, buffer);
		}
	    }

	// now draw the lines between blocks
	if ((!((lastX == sx) && (y1 == y2))) &&
	    (sf->drawn  || ((prevSf != NULL) && (prevSf->drawn))) &&
	    (((lastE >= winStart) && (lastE <= winEnd)) || 
	    ((s > winStart) && (s < winEnd))))
	    {
	    if (lastLevel == sf->level)
		{
                if (sf->orientation == 1)
                    safef(buffer, sizeof buffer, "%dbp (%dbp in ref) (%.1lf%% N, %.1lf%% masked)", qs - lastQEnd, s - lastE, queryGapNFrac*100, queryGapMaskedFrac*100);
                else
                    safef(buffer, sizeof buffer, "%dbp (%dbp in ref) (%.1lf%% N, %.1lf%% masked)", qs - lastQEnd, lastS - e, queryGapNFrac*100, queryGapMaskedFrac*100);
		if (sf->orientation == -1)
		    {
		    if (lastX != ex)
			{
			hvGfxLine(hvg, ex, y1, lastX, y2, color);
			boundMapBox(hvg, s, e, ex, y1, lastX-ex, 1, tg->track,
				"", buffer, NULL, TRUE, NULL);
                        if (lastQEnd != qs) {
                            safef(buffer, sizeof buffer, "%dbp", qs - lastQEnd);
                            //hvGfxTextCentered(hvg, ex, y2 + lineHeight/2, lastX-ex, INSERT_TEXT_HEIGHT, MG_ORANGE, font, buffer);
                        }
			}
		    }
		else
		    {
		    if (lastX != sx)
			{
			hvGfxLine(hvg, lastX, y1, sx, y2, color);
			boundMapBox(hvg, s, e, lastX, y1, sx-lastX, 1, tg->track,
				"", buffer, NULL, TRUE, NULL);
                        if (lastQEnd != qs) {
                            safef(buffer, sizeof buffer, "%dbp", qs - lastQEnd);
                            //hvGfxTextCentered(hvg, lastX, y2 + lineHeight/2, sx-lastX, INSERT_TEXT_HEIGHT, MG_ORANGE, font, buffer);
                        }
			}
		    }
		}
	    else if (lastLevel > sf->level)
		{
		hvGfxLine(hvg, lastX, y1, sx, y2, color);
		hvGfxLine(hvg, sx, y2, sx, y2 - lineHeight - lineHeight/3, color);
		char buffer[1024];
		safef(buffer, sizeof buffer, "%d-%d %dbp gap (%.1lf%% N, %.1lf%% masked)",prevSf->qStart,prevSf->qEnd, qs - lastQEnd, queryGapNFrac*100, queryGapMaskedFrac*100);
		boundMapBox(hvg, s, e, sx, y2 - lineHeight - lineHeight/3, 2, lineHeight + lineHeight/3, tg->track,
	                    "", buffer, NULL, TRUE, NULL);
		safef(buffer, sizeof buffer, "%dbp", qs - lastQEnd);
                //if (lastQEnd != qs)
                    //hvGfxTextCentered(hvg, sx - 10, y2 + lineHeight/2, 20, INSERT_TEXT_HEIGHT, MG_ORANGE, font, buffer);

		}
	    else
		{
		char buffer[1024];
		safef(buffer, sizeof buffer, "%d-%d %dbp gap (%.1lf%% N, %.1lf%% masked)",prevSf->qStart,prevSf->qEnd, qs - lastQEnd, queryGapNFrac*100, queryGapMaskedFrac*100);
		if (sf->orientation == -1)
		    {
		    hvGfxLine(hvg, lastX-1, y1, ex, y2, color);
		    hvGfxLine(hvg, ex, y2, ex, y2 + lineHeight , color);
		    boundMapBox(hvg, s, e, ex-1, y2, 2, lineHeight , tg->track,
				"", buffer, NULL, TRUE, NULL);
                    safef(buffer, sizeof buffer, "%dbp", qs - lastQEnd);
                    //if (lastQEnd != qs)
                        //hvGfxTextCentered(hvg, ex - 10, y2 + lineHeight, 20, INSERT_TEXT_HEIGHT, MG_ORANGE, font, buffer);
		    }
		else
		    {
		    hvGfxLine(hvg, lastX-1, y1, sx, y2, color);
		    hvGfxLine(hvg, sx, y2, sx, y2 + lineHeight , color);
		    boundMapBox(hvg, s, e, sx-1, y2, 2, lineHeight , tg->track,
				"", buffer, NULL, TRUE, NULL);

                    safef(buffer, sizeof buffer, "%dbp", qs - lastQEnd);
                    //if (lastQEnd != qs)
                        //hvGfxTextCentered(hvg, sx - 10, y2 + lineHeight, 20, INSERT_TEXT_HEIGHT, MG_ORANGE, font, buffer);
		    }
		}
	    }
	}
    if (sf->orientation == -1)
	lastX = sx;
    else
	lastX = ex;
    lastS = s;
    lastE = e;
    lastLevel = sf->level;
    lastQEnd = qe;
    }
}

struct cartOptions
    {
    enum chainColorEnum chainColor; /*  ChromColors, ScoreColors, NoColors */
    int scoreFilter ; /* filter chains by score if > 0 */
    };

static void fixItems(struct linkedFeatures *lf, compareFunction compare)
// put all chain blocks from a single query chromosome into one
// linkedFeatures structure
{
struct linkedFeatures *firstLf, *next;
struct snakeFeature  *sf,  *nextSf;

firstLf = lf;
for (;lf; lf = next)
    {
    next = lf->next;
    if (!sameString(firstLf->name, lf->name) && (lf->components != NULL))
	{
	slSort(&firstLf->components, compare);
        firstLf->next = lf;
	firstLf = lf;
	}
    for (sf =  (struct snakeFeature *)lf->components; sf != NULL; sf = nextSf)
	{
	sf->qName = lf->name;
	sf->orientation = lf->orientation;
        sf->score = lf->score;
	nextSf = sf->next;
	if (firstLf != lf)
	    {
	    lf->components = NULL;
	    slAddHead(&firstLf->components, sf);
	    }
	}
    }

if (firstLf != NULL)
    {
    slSort(&firstLf->components, compare);
    firstLf->next = 0;
    }
}


void snakeDrawLeftLabels()
{
}

static void makeSnakeFeatures(struct linkedFeatures *lf)
/* allocate an info structure for every block in the alignment. */
{
for(; lf; lf = lf->next)
    {
    struct simpleFeature *sf = lf->components;
    struct snakeFeature *sfList = NULL;

    for(;sf; sf = sf->next)
        {
        struct snakeFeature *this;

        AllocVar(this);
        *(struct simpleFeature *)this = *sf;
	this->orientation = lf->orientation;
        slAddHead(&sfList, this);
        }
    lf->components = (struct simpleFeature *)sfList;
    }
}

static void makeSingleCoverage(struct linkedFeatures *lf, compareFunction compare)
/* Because snakes are a display of O+O of the query sequence projected onto the reference sequence, 
 * we need to only have one copy of each piece of the query sequence. */
{
for(; lf; lf = lf->next)   // for each query sequence
    {
    slSort(&lf->components, snakeFeatureCmpScore); // we want to choose the higher scoring alignements
    Bits *qBits = bitAlloc(lf->qSize); // the bit array we use to keep track if we've seen this query sequence
    struct simpleFeature *sf = lf->components;
    struct simpleFeature *prevSf = NULL;

    for(; sf ; sf = sf->next)
        {
        // have we seen this query range?
        if (bitCountRange(qBits, sf->qStart, sf->qEnd - sf->qStart))
            {
            // if we've seen this query sequence, then delete it
            if (prevSf == NULL)
                lf->components = sf->next;
            else
                prevSf->next = sf->next;
            }
        else
            {
            bitSetRange(qBits, sf->qStart, sf->qEnd - sf->qStart);
            prevSf = sf;
            }
        }

    slSort(&lf->components, compare);
    }
}

void maybeLoadSnake(struct track *track)
/* check to see if we're doing snakes and if so load the correct methods. */
{
boolean doSnake = cartOrTdbBoolean(cart, track->tdb, "doSnake", FALSE);
if (doSnake)
    {
    compareFunction compare;
    if (track->visibility == tvFull)
        compare = snakeFeatureCmpQStart;
    else
        // we don't care about the order on query
        compare = snakeFeatureCmpTStart;

    track->drawLeftLabels = snakeDrawLeftLabels;
    track->itemHeight = snakeItemHeight;
    track->totalHeight = snakeHeight;
    track->drawItemAt = snakeDrawAt;
    track->drawItems = snakeDraw;

    slSort(&track->items, linkedFeaturesCmpName);
    makeSnakeFeatures(track->items);
    fixItems(track->items, compare);
    makeSingleCoverage(track->items, compare);
    track->canPack = FALSE;
    }
}

