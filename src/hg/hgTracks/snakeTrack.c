/* snakeTrack - stuff to load and display snake type tracks in browser.  */
/*       uses standard chains from the database */

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

struct snakeFeature
    {
    struct snakeFeature *next;
    int start, end;			/* Start/end in browser coordinates. */
    int qStart, qEnd;			/* query start/end */
    int level;				/* level in packed snake */
    int orientation;			/* strand... -1 is '-', 1 is '+' */
    boolean drawn;			/* did we draw this feature? */
    };

struct level
{
boolean init;		/* has this level been initialized */
int orientation;	/* strand.. see above */
int edge;		/* the leading edge of this level */
int adjustLevel;	/* used to compress out the unused levels */
boolean hasBlock;	/* are there any blocks in this level */
};

static struct level Levels[1000000]; /* for packing the snake */
static int maxLevel = 0;     	     /* deepest level */	

/* blocks that make it through the min size filter */
static struct snakeFeature *newList = NULL;   

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

void clearLevels()
/* clear out the data structure that we use to pack the snake */
{
int ii;

for(ii=0; ii < sizeof(Levels) / sizeof(Levels[0]); ii++)
    Levels[ii].init = FALSE;
maxLevel = 0;
}

void calcSnake(struct snakeFeature *list, int level)
// calculate the packed snake
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
	Levels[level].edge = 1000000000;  // bigger than the biggest chrom
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
	calcSnake(insertHead, level + 1);
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
    calcSnake(insertHead, nextLevel);
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

struct cartOptions
    {
    enum chainColorEnum chainColor; /*  ChromColors, ScoreColors, NoColors */
    int scoreFilter ; /* filter chains by score if > 0 */
    };

int snakeHeight(struct track *tg, enum trackVisibility vis)
/* calculate height of all the snakes being displayed */
{
int height = 0;
struct slList *item = tg->items;

item = tg->items;

for (item=tg->items;item; item = item->next)
    {
    height += tg->itemHeight(tg, item);
    }
return height;
}

static void doQuery(struct sqlConnection *conn, char *fullName, 
			struct lm *lm, struct hash *hash, 
			int start, int end,  boolean isSplit, int chainId)
/* doQuery- check the database for chain elements between
 * 	start and end.  Use the passed hash to resolve chain
 * 	id's and place the elements into the right
 * 	linkedFeatures structure
 */
{
struct sqlResult *sr = NULL;
char **row;
struct linkedFeatures *lf;
struct snakeFeature *sf;
struct dyString *query = newDyString(1024);
char *force = "";

if (isSplit)
    force = "force index (bin)";

if (chainId == -1)
    dyStringPrintf(query, 
	"select chainId,tStart,tEnd,qStart from %sLink %s where ",
	fullName, force);
else
    dyStringPrintf(query, 
	"select chainId, tStart,tEnd,qStart from %sLink where chainId=%d and ",
	fullName, chainId);
if (!isSplit)
    dyStringPrintf(query, "tName='%s' and ", chromName);
hAddBinToQuery(start, end, query);
dyStringPrintf(query, "tStart<%u and tEnd>%u", end, start);
sr = sqlGetResult(conn, query->string);

/* Loop through making up simple features and adding them
 * to the corresponding linkedFeature. */
while ((row = sqlNextRow(sr)) != NULL)
    {
    lf = hashFindVal(hash, row[0]);
    if (lf != NULL)
	{
	struct chain *pChain = lf->extra;
	lmAllocVar(lm, sf);
	sf->start = sqlUnsigned(row[1]);
	sf->end = sqlUnsigned(row[2]);
	sf->qStart = sqlUnsigned(row[3]); 

	sf->qEnd = sf->qStart + (sf->end - sf->start);
	if ((pChain) && pChain->qStrand == '-')
	    {
	    int temp;

	    temp = sf->qStart;
	    sf->qStart = pChain->qSize - sf->qEnd;
	    sf->qEnd = pChain->qSize - temp;
	    }
	sf->orientation = lf->orientation;
	slAddHead(&lf->components, sf);
	}
    }
sqlFreeResult(&sr);
dyStringFree(&query);
}

struct snakeInfo
{
int maxLevel;
} snakeInfo;

void calcPackSnake(struct track *tg, void *item)
{
struct linkedFeatures  *lf = (struct linkedFeatures *)item;
if (lf->components == NULL)
    return;

// we use the codons field to keep track of whether we already
// calculated the height of this snake
if (lf->codons == NULL)
    {
    clearLevels();
    struct snakeFeature *sf;

    // this will destroy lf->components, and add to newList
    calcSnake((struct snakeFeature *)lf->components, 0);
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
    maxLevel = count - 1;

    // remap blocks
    for(sf=(struct snakeFeature *)lf->components; sf; sf = sf->next)
	sf->level = Levels[sf->level].adjustLevel;

    struct snakeInfo *si;
    AllocVar(si);
    si->maxLevel = maxLevel;
    lf->codons = (struct simpleFeature *)si;
    }
}

int packSnakeItemHeight(struct track *tg, void *item)
// return height of a single packed snake 
{
if (item == NULL)
    return 0;
struct linkedFeatures  *lf = (struct linkedFeatures *)item;
if (lf->components == NULL)
    return 0;
calcPackSnake(tg, item);
struct snakeInfo *si = (struct snakeInfo *)lf->codons;
int lineHeight = tg->lineHeight ;
return (si->maxLevel + 1) * (2 * lineHeight);
}

int fullSnakeItemHeight(struct track *tg, void *item)
// return height of full snake
{
struct linkedFeatures  *lf = (struct linkedFeatures *)item;
struct snakeFeature  *sf;
int s, e;
int lineHeight = tg->lineHeight ;
int oldOrient = 0;
int tStart, tEnd;
int size = 0;
//int count = 0;
tStart = 0;
tEnd = 0;
if (lf->components)
    size = lineHeight;
for (sf =  (struct snakeFeature *)lf->components; sf ;  sf = sf->next)
    {
    int orient = sf->orientation;

    s = sf->start; e = sf->end;
    /*
    if ((e < winStart) || (s > winEnd))
	continue;
    if (s < winStart) s = winStart;
    */
    
    if (((oldOrient) && (oldOrient != orient))
	||  ((oldOrient == 1) && (tEnd) && (s < tEnd))
	||  ((oldOrient == -1) && (tStart) && (e > tStart)))
	{
	size += lineHeight;
	}
    oldOrient = orient;
    tEnd = e;
    tStart = s;
    }
return size;
}

int snakeItemHeight(struct track *tg, void *item)
{
    if (tg->visibility == tvFull)
	return fullSnakeItemHeight(tg, item);
    else if (tg->visibility == tvPack)
	return packSnakeItemHeight(tg, item);
return 0;
}

static int linkedFeaturesCmpScore(const void *va, const void *vb)
/* Help sort linkedFeatures by score */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
if (a->score > b->score)
    return -1;
else if (a->score < b->score)
    return 1;
return 0;
}

void snakeDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw linked features items. */
{
struct slList *item;
int y;
struct linkedFeatures  *lf;
double scale = scaleForWindow(width, seqStart, seqEnd);
int height = snakeHeight(tg, vis);

hvGfxSetClip(hvg, xOff, yOff, width, height);

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

y = yOff;
for (item = tg->items; item != NULL; item = item->next)
    {
    if(tg->itemColor != NULL) 
	color = tg->itemColor(tg, item, hvg);
    tg->drawItemAt(tg, item, hvg, xOff, y, scale, font, color, vis);
    y += tg->itemHeight(tg, item);
    } 
}

void snakeLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
{
}

void packSnakeDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
struct linkedFeatures  *lf = (struct linkedFeatures *)item;
calcPackSnake(tg, item);

int lastE = -1;
int offY = y;
struct snakeFeature  *sf, *prevSf;
int lineHeight = tg->lineHeight ;
int tStart, tEnd, qStart;
int  qs, qe;
int s, e;
int heightPer = tg->heightPer;
int lastX = -1,lastY = y;
int lastQEnd = 0;
int lastLevel = -1;
qe = lastQEnd = 0;
for (sf =  (struct snakeFeature *)lf->components; sf != NULL; lastQEnd = qe, prevSf = sf, sf = sf->next)
    {
    qs = sf->qStart;
    qe = sf->qEnd;
    y = offY + (sf->level * 2) * lineHeight;
    s = sf->start; e = sf->end;
    tEnd = sf->end;

    int sx, ex;
    if (!positiveRangeIntersection(winStart, winEnd, s, e))
	continue;
    sx = round((double)((int)s-winStart)*scale) + xOff;
    ex = round((double)((int)e-winStart)*scale) + xOff;

    // color by strand
    color = (sf->orientation == -1) ? MG_RED : MG_BLUE;

    int w = ex - sx;
    if (w == 0) 
	w = 1;
    assert(w > 0);
    char buffer[1024];
    safef(buffer, sizeof buffer, "%d %d",sf->qStart,sf->qEnd);
    if (sx < insideX)
	{
	int olap = insideX - sx;
	sx = insideX;
	w -= olap;
	}
    mapBoxHgcOrHgGene(hvg, s, e, sx+1, y, w-2, heightPer, tg->track,
		"blockAlign", buffer, NULL, TRUE, "boxAlignExtra=on");
    hvGfxBox(hvg, sx, y, w, heightPer, color);
    sf->drawn = TRUE;
    tEnd = e;
    tStart = s;
    qStart = sf->qStart;
    lastY = y;
    lastLevel = sf->level;
    //lastX = x;
    }
lastX = -1,lastY = y;
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

	if (lastQEnd != qs)
	    color = MG_ORANGE;

#ifdef NOTNOW   // insert in query
	if (sf->orientation == -1)
	    {
	    if ((lastE == e) && (qs != lastQEnd))
		{
		printf("%d ",qs-lastQEnd);
		hvGfxLine(hvg, ex, y2, ex, y2 + lineHeight, MG_ORANGE);
		}
	    }
	else
	    {
	    if ((lastE == s )) // && (qs != lastQEnd))
		printf("%d ",qs-lastQEnd);
	    if ((lastE == s ) && (qs != lastQEnd))
		{
		printf("%d ",qs-lastQEnd);
		hvGfxLine(hvg, sx, y2, sx, y2 + lineHeight, MG_ORANGE);
		printf("%d %d |",lastE,s);
		}
	    }
#endif

	if ((!((lastX == sx) && (y1 == y2))) &&
	    (sf->drawn  || ((prevSf != NULL) && (prevSf->drawn))) &&
	    (((lastE > winStart) && (lastE < winEnd)) || 
	    ((s > winStart) && (s < winEnd))))
	    {
	    if (lastLevel == sf->level)
		{
		safef(buffer, sizeof buffer, "%dbp", qs - lastQEnd);
		if (sf->orientation == -1)
		    {
		    if (lastX != ex)
			{
			hvGfxLine(hvg, ex, y1, lastX, y2, color);
			mapBoxHgcOrHgGene(hvg, s, e, ex, y1, lastX-ex, 1, tg->track,
				"foo", buffer, NULL, TRUE, NULL);
			}
		    }
		else
		    {
		    if (lastX != sx - 1)
			{
			hvGfxLine(hvg, lastX, y1, sx, y2, color);
			mapBoxHgcOrHgGene(hvg, s, e, lastX, y1, sx-lastX, 1, tg->track,
				"foo", buffer, NULL, TRUE, NULL);
			}
		    }
		}
	    else if (lastLevel > sf->level)
		{
		hvGfxLine(hvg, lastX, y1, sx, y2, color);
		hvGfxLine(hvg, sx, y2, sx, y2 - lineHeight - lineHeight/3, color);
		char buffer[1024];
		safef(buffer, sizeof buffer, "%d-%d %dbp gap",prevSf->qStart,prevSf->qEnd, qs - lastQEnd);
		mapBoxHgcOrHgGene(hvg, s, e, sx, y2 - lineHeight - lineHeight/3, 2, lineHeight + lineHeight/3, tg->track,
	                    "foo", buffer, NULL, TRUE, NULL);

		}
	    else
		{
		hvGfxLine(hvg, lastX-1, y1, sx, y2, color);
		hvGfxLine(hvg, sx, y2, sx, y2 + lineHeight , color);
		char buffer[1024];
		safef(buffer, sizeof buffer, "%d-%d %dbp gap",prevSf->qStart,prevSf->qEnd, qs - lastQEnd);
		mapBoxHgcOrHgGene(hvg, s, e, sx-1, y2, 2, lineHeight , tg->track,
	                    "foo", buffer, NULL, TRUE, NULL);
		}
	    }
	}
    tEnd = e;
    tStart = s;
    qStart = sf->qStart;
    if (sf->orientation == -1)
	lastX = sx;
    else
	lastX = ex;
    lastE = e;
    lastLevel = sf->level;
    lastQEnd = qe;
    }
}

void fullSnakeDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
struct linkedFeatures  *lf = (struct linkedFeatures *)item;
struct snakeFeature  *sf, *prevSf;
int s, e;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight ;
int oldOrient = 0;
int tStart, tEnd, qStart;
int lastQEnd = 0;
int midY;
int  qs, qe;
qStart = 0;
tStart = xOff;
tEnd = winEnd;
prevSf = NULL;

qe = lastQEnd = 0;
for (sf =  (struct snakeFeature *)lf->components; sf != NULL; lastQEnd = qe, prevSf = sf, sf = sf->next)
    {
    int orient = sf->orientation;
    midY = y + heightPer/2;

    qs = sf->qStart;
    s = sf->start; e = sf->end;
    /*
    if (qs < lastQEnd )
	continue;
	*/

    qe = sf->qEnd;
    /*
    if ((e < winStart) || (s > winEnd))
	continue;
	*/

    //if (s < winStart) s = winStart;
    
    if (((oldOrient) && (oldOrient != orient))
	||  ((oldOrient == 1) && (tEnd) && (s < tEnd))
	||  ((oldOrient == -1) && (tStart) && (e > tStart)))
	{
	if ((qStart) && (sf->qStart - qStart) < 500000)
	    {
	    if (oldOrient == 1)
		{
		if ((orient == -1) && (tEnd < sf->start))
		    {
		    int x1, x2, x3, w;

		    x1 = round((double)((int)tEnd-winStart)*scale) + xOff;
		    x2 = round((double)((int)e-winStart)*scale) + xOff + 8;
		    x3 = round((double)((int)sf->end-winStart)*scale) + xOff;
		    w = x2-x1;
		    hvGfxLine(hvg, x1, midY, x2, midY, color);
		    hvGfxLine(hvg, x2, lineHeight + midY, x2, midY, color);
		    hvGfxLine(hvg, x2, lineHeight+midY, x3, lineHeight+midY, color);
		    clippedBarbs(hvg, x1, midY, w, tl.barbHeight, tl.barbSpacing, 
			     oldOrient, color, FALSE);
		    clippedBarbs(hvg, x3, midY + lineHeight, x2-x3, tl.barbHeight, tl.barbSpacing, 
			     orient, color, FALSE);
		    }
		else if ((orient == -1) && (tEnd > sf->start))
		    {
		    int x1, x2, x3, w;

		    x1 = round((double)((int)tEnd-winStart)*scale) + xOff;
		    x2 = round((double)((int)tEnd-winStart)*scale) + xOff + 8;
		    x3 = round((double)((int)sf->end-winStart)*scale) + xOff;
		    w = x2-x1;
		    hvGfxLine(hvg, x1, midY, x2, midY, color);
		    hvGfxLine(hvg, x2, lineHeight + midY, x2, midY, color);
		    hvGfxLine(hvg, x2, lineHeight+midY, x3, lineHeight+midY, color);
		    clippedBarbs(hvg, x1, midY, w+1, tl.barbHeight, tl.barbSpacing, 
			     oldOrient, color, FALSE);
		    clippedBarbs(hvg, x3, midY + lineHeight, x2-x3+1, tl.barbHeight, tl.barbSpacing, 
			     orient, color, FALSE);
		    }
		else if ((orient == 1) && (s < tEnd))
		    {
		    int x1, x2, x3,x4, w;

		    x1 = round((double)((int)tEnd-winStart)*scale) + xOff;
		    x2 = round((double)((int)tEnd-winStart)*scale) + xOff + 8;
		    x3 = round((double)((int)s-winStart)*scale) + xOff - 8;
		    x4 = round((double)((int)s-winStart)*scale) + xOff;
		    w = x2-x1;
		    hvGfxLine(hvg, x1, midY, x2, midY, color);
		    hvGfxBox(hvg, x2, midY-2, 4, 4, color);
		    hvGfxBox(hvg, x3 - 4, midY-2 + lineHeight, 4, 4, color);
		    hvGfxLine(hvg, x3, lineHeight + midY, x4, lineHeight+midY, color);
		    clippedBarbs(hvg, x1, midY, w+1, tl.barbHeight, tl.barbSpacing, 
			     oldOrient, color, FALSE);
		    if (x3 > x4)
			printf("oops\n");
		    clippedBarbs(hvg, x3, midY +  lineHeight, x4-x3+1, tl.barbHeight, tl.barbSpacing, 
			     orient , color, FALSE);
		    }
		}
	    else if (oldOrient == -1)
		{
		if ((orient == 1) && (tStart >= sf->start))
		    {
		    int x1, x2, x3, w;

		    x1 = round((double)((int)tStart-winStart)*scale) + xOff;
		    x2 = round((double)((int)sf->start-winStart)*scale) + xOff - 8;
		    x3 = round((double)((int)sf->start-winStart)*scale) + xOff;
		    w = x1 - x2;
		    hvGfxLine(hvg, x1, midY, x2, midY, color);
		    hvGfxLine(hvg, x2, lineHeight + midY, x2, midY, color);
		    hvGfxLine(hvg, x2, lineHeight+midY, x3, lineHeight+midY, color);
		    clippedBarbs(hvg, x2, midY, w, tl.barbHeight, tl.barbSpacing, 
			     oldOrient, color, FALSE);
		    clippedBarbs(hvg, x2, lineHeight+midY, x3-x2+1, tl.barbHeight, tl.barbSpacing, 
			     orient, color, FALSE);
		    }
		else if ((orient == 1) && (tStart < sf->start))
		    {
		    int x1, x2, x3, w;

		    x1 = round((double)((int)tStart-winStart)*scale) + xOff;
		    x2 = round((double)((int)tStart-winStart)*scale) + xOff - 8;
		    x3 = round((double)((int)sf->start-winStart)*scale) + xOff;
		    w = x1-x2;
		    hvGfxLine(hvg, x1, midY, x2, midY, color);
		    hvGfxLine(hvg, x2, lineHeight + midY, x2, midY, color);
		    hvGfxLine(hvg, x2, lineHeight+midY, x3, lineHeight+midY, color);
		    clippedBarbs(hvg, x2, midY, w, tl.barbHeight, tl.barbSpacing, 
			     oldOrient, color, FALSE);
		    if (x3 < x2)
			printf("oops\n");
		    clippedBarbs(hvg, x2, midY + lineHeight, x3-x2, tl.barbHeight, tl.barbSpacing, 
			     orient, color, FALSE);
		    }
		else if ((orient == -1) && (e > sf->start))
		    {
		    int x1, x2, x3,x4, w;

		    x1 = round((double)((int)tStart-winStart)*scale) + xOff;
		    x2 = round((double)((int)tStart-winStart)*scale) + xOff - 8;
		    x3 = round((double)((int)e-winStart)*scale) + xOff + 8;
		    x4 = round((double)((int)e-winStart)*scale) + xOff;
		    w = x1-x2;
		    hvGfxLine(hvg, x1, midY, x2, midY, color);
		    hvGfxBox(hvg, x2, midY-2, 4, 4, color);
		    hvGfxLine(hvg, x3, lineHeight + midY, x4, lineHeight+midY, color);
		    hvGfxBox(hvg, x3, lineHeight + midY-2, 4, 4, color);
		    clippedBarbs(hvg, x2, midY, w+1, tl.barbHeight, tl.barbSpacing, 
			     oldOrient, color, FALSE);
		    if (x4 > x3)
			printf("oops\n");
		    clippedBarbs(hvg, x4, midY + lineHeight, x3-x4+1, tl.barbHeight, tl.barbSpacing, 
			     oldOrient , color, FALSE);
		    }
		}
		}

	y += lineHeight;
	}
    else if ((oldOrient) && ((qStart) && (sf->qStart - qStart) < 500000))
	{
	    int x1, x2, w;

	    x1 = round((double)((int)tEnd-winStart)*scale) + xOff;
	    x2 = round((double)((int)sf->start -winStart)*scale) + xOff;
	    hvGfxLine(hvg, x1, midY, x2, midY, color);
	    if (x2 > x1)
		{
		w = x2-x1;
		clippedBarbs(hvg, x1, midY, w+1, tl.barbHeight, tl.barbSpacing, 
			 oldOrient, color, FALSE);
		}
	    else
		{
		w = x1-x2;
		clippedBarbs(hvg, x2, midY, w+1, tl.barbHeight, tl.barbSpacing, 
			 oldOrient, color, FALSE);
		}
	}
    color =	    lfChromColor(tg, item, hvg);

color = (sf->orientation == -1) ? MG_RED : MG_BLUE;
    drawScaledBoxSample(hvg, s, e, scale, xOff, y, heightPer, 
			color, lf->score );
    tEnd = e;
    tStart = s;
    qStart = sf->qStart;
    oldOrient = orient;
    }
}

void snakeDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
    
    if (tg->visibility == tvFull)
	fullSnakeDrawAt(tg, item, hvg, xOff, y, scale, 
	    font, color, vis);
    else if (tg->visibility == tvPack)
	packSnakeDrawAt(tg, item, hvg, xOff, y, scale, 
	    font, color, vis);
}

void fixItems(struct linkedFeatures *lf)
// put all blocks from a single query chromosome into one
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
	slSort(&firstLf->components, snakeFeatureCmpQStart);
	firstLf = lf;
	}
    for (sf =  (struct snakeFeature *)lf->components; sf != NULL; sf = nextSf)
	{
	sf->orientation = lf->orientation;
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
    slSort(&firstLf->components, snakeFeatureCmpQStart);
    firstLf->next = 0;
    }
}


static void loadLinks(struct track *tg, int seqStart, int seqEnd,
         enum trackVisibility vis)
// load up the chain elements into linkedFeatures
{
int start, end, extra;
char fullName[64];
int maxOverLeft = 0, maxOverRight = 0;
int overLeft, overRight;
struct linkedFeatures *lf;
struct lm *lm;
struct hash *hash;	/* Hash of chain ids. */
struct sqlConnection *conn;
lm = lmInit(1024*4);
hash = newHash(0);
conn = hAllocConn(database);

/* Make up a hash of all linked features keyed by
 * id, which is held in the extras field.  */
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    char buf[256];
    struct chain *pChain = lf->extra;
    safef(buf, sizeof(buf), "%d", pChain->id);
    hashAdd(hash, buf, lf);
    overRight = lf->end - seqEnd;
    if (overRight > maxOverRight)
	maxOverRight = overRight;
    overLeft = seqStart - lf->start ;
    if (overLeft > maxOverLeft)
	maxOverLeft = overLeft;
    }

if (hash->size)
    {
    boolean isSplit = TRUE;
    /* Make up range query. */
    safef(fullName, sizeof fullName, "%s_%s", chromName, tg->table);
    if (!hTableExists(database, fullName))
	{
	strcpy(fullName, tg->table);
	isSplit = FALSE;
	}

    /* in dense mode we don't draw the lines 
     * so we don't need items off the screen 
     */
    if (vis == tvDense)
	doQuery(conn, fullName, lm,  hash, seqStart, seqEnd,  isSplit, -1);
    else
	{
	/* if chains extend beyond edge of window we need to get 
	 * elements that are off the screen
	 * in both directions so we know whether to draw
	 * one or two lines to the edge of the screen.
	 */
#define STARTSLOP	0
#define MULTIPLIER	10
#define MAXLOOK		100000
	extra = (STARTSLOP < maxOverLeft) ? STARTSLOP : maxOverLeft;
	start = seqStart - extra;
	extra = (STARTSLOP < maxOverRight) ? STARTSLOP : maxOverRight;
	end = seqEnd + extra;
	doQuery(conn, fullName, lm,  hash, start, end,  isSplit, -1);
	}
    }
hFreeConn(&conn);
}

void snakeLoadItems(struct track *tg)
{
char *track = tg->table;
struct chain chain;
int rowOffset;
char **row;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
struct linkedFeatures *list = NULL, *lf;
int qs;
char optionChr[128]; /* Option -  chromosome filter */
char *optionChrStr;
char extraWhere[128] ;
struct cartOptions *chainCart;
struct chain *pChain;

chainCart = (struct cartOptions *) tg->extraUiData;

safef( optionChr, sizeof(optionChr), "%s.chromFilter", tg->table);
optionChrStr = cartUsualString(cart, optionChr, "All");
int ourStart = winStart;
int ourEnd = winEnd;

// we're grabbing everything now.. we really should be 
// doing this as a preprocessing stage, rather than at run-time
ourStart = 0;
ourEnd = 500000000;
if (startsWith("chr",optionChrStr)) 
    {
    safef(extraWhere, sizeof(extraWhere), 
            "qName = \"%s\" and score > %d",optionChrStr, 
            chainCart->scoreFilter);
    sr = hRangeQuery(conn, track, chromName, ourStart, ourEnd, 
            extraWhere, &rowOffset);
    }
else
    {
    if (chainCart->scoreFilter > 0)
        {
        safef(extraWhere, sizeof(extraWhere), 
                "score > \"%d\"",chainCart->scoreFilter);
        sr = hRangeQuery(conn, track, chromName, ourStart, ourEnd, 
                extraWhere, &rowOffset);
        }
    else
        {
        safef(extraWhere, sizeof(extraWhere), " ");
        sr = hRangeQuery(conn, track, chromName, ourStart, ourEnd, 
                NULL, &rowOffset);
        }
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    chainHeadStaticLoad(row + rowOffset, &chain);
    AllocVar(pChain);
    *pChain = chain;
    AllocVar(lf);
    lf->start = lf->tallStart = chain.tStart;
    lf->end = lf->tallEnd = chain.tEnd;
    lf->grayIx = maxShade;
    if (chainCart->chainColor == chainColorScoreColors)
	{
	float normScore = sqlFloat((row+rowOffset)[11]);
	lf->grayIx = (int) ((float)maxShade * (normScore/100.0));
	if (lf->grayIx > (maxShade+1)) lf->grayIx = maxShade+1;
	lf->score = normScore;
	}
    else
	lf->score = chain.score;

    lf->filterColor = -1;

    if (chain.qStrand == '-')
	{
	lf->orientation = -1;
        qs = chain.qSize - chain.qEnd;
	}
    else
        {
	lf->orientation = 1;
	qs = chain.qStart;
	}
    char buffer[1024];
    safef(buffer, sizeof(buffer), "%s", chain.qName);
    lf->name = cloneString(buffer);
    lf->extra = pChain;
    slAddHead(&list, lf);
    }

/* Make sure this is sorted if in full mode. Sort by score when
 * coloring by score and in dense */
if (tg->visibility != tvDense)
    slSort(&list, linkedFeaturesCmpStart);
else if ((tg->visibility == tvDense) &&
	(chainCart->chainColor == chainColorScoreColors))
    slSort(&list, chainCmpScore);
else
    slReverse(&list);
tg->items = list;


/* Clean up. */
sqlFreeResult(&sr);
hFreeConn(&conn);

/* now load the items */
loadLinks(tg, ourStart, ourEnd, tg->visibility);
lf=tg->items;
fixItems(lf);
}	/*	chainLoadItems()	*/

static Color chainScoreColor(struct track *tg, void *item, struct hvGfx *hvg)
{
struct linkedFeatures *lf = (struct linkedFeatures *)item;

return(tg->colorShades[lf->grayIx]);
}

static Color chainNoColor(struct track *tg, void *item, struct hvGfx *hvg)
{
return(tg->ixColor);
}

static void setNoColor(struct track *tg)
{
tg->itemColor = chainNoColor;
tg->color.r = 0;
tg->color.g = 0;
tg->color.b = 0;
tg->altColor.r = 127;
tg->altColor.g = 127;
tg->altColor.b = 127;
tg->ixColor = MG_BLACK;
tg->ixAltColor = MG_GRAY;
}

void snakeMethods(struct track *tg, struct trackDb *tdb, 
	int wordCount, char *words[])
/* Fill in custom parts of alignment chains. */
{

struct cartOptions *chainCart;

AllocVar(chainCart);

boolean normScoreAvailable = chainDbNormScoreAvailable(tdb);

/*	what does the cart say about coloring option	*/
chainCart->chainColor = chainFetchColorOption(cart, tdb, FALSE);

chainCart->scoreFilter = cartUsualIntClosestToHome(cart, tdb,
	FALSE, SCORE_FILTER, 0);


linkedFeaturesMethods(tg);
tg->itemColor = lfChromColor;	/*	default coloring option */

/*	if normScore column is available, then allow coloring	*/
if (normScoreAvailable)
    {
    switch (chainCart->chainColor)
	{
	case (chainColorScoreColors):
	    tg->itemColor = chainScoreColor;
	    tg->colorShades = shadesOfGray;
	    break;
	case (chainColorNoColors):
	    setNoColor(tg);
	    break;
	default:
	case (chainColorChromColors):
	    break;
	}
    }
else
    {
    char option[128]; /* Option -  rainbow chromosome color */
    char *optionStr;	/* this old option was broken before */

    safef(option, sizeof(option), "%s.color", tg->table);
    optionStr = cartUsualString(cart, option, "on");
    if (differentWord("on",optionStr))
	{
	setNoColor(tg);
	chainCart->chainColor = chainColorNoColors;
	}
    else
	chainCart->chainColor = chainColorChromColors;
    }

tg->canPack = TRUE;
tg->loadItems = snakeLoadItems;
tg->drawItems = snakeDraw;
tg->mapItemName = lfMapNameFromExtra;
tg->subType = lfSubChain;
tg->extraUiData = (void *) chainCart;
tg->totalHeight = snakeHeight; 

tg->drawItemAt = snakeDrawAt;
tg->itemHeight = snakeItemHeight;
}
