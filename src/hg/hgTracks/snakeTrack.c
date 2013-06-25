/* snakeTrack - stuff to load and display snake type tracks in browser.  */
/*       uses standard chains from the database or HAL files */
/*       probably the chain and hal support should be in different files
 *       but they share drawing code
 */

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

#ifdef USE_HAL
#include "halBlockViz.h"
#endif

struct snakeFeature
    {
    struct snakeFeature *next;
    int start, end;			/* Start/end in browser coordinates. */
    int qStart, qEnd;			/* query start/end */
    int level;				/* level in packed snake */
    int orientation;			/* strand... -1 is '-', 1 is '+' */
    boolean drawn;			/* did we draw this feature? */
    char *sequence;			/* may have sequence, or NULL */
    char *qName;			/* chrom name on other species */
    };

// static machinery to calculate packed snake

struct level
{
boolean init;		/* has this level been initialized */
int orientation;	/* strand.. see above */
int edge;		/* the leading edge of this level */
int adjustLevel;	/* used to compress out the unused levels */
boolean hasBlock;	/* are there any blocks in this level */
};

static struct level Levels[1000000]; /* for packing the snake, not re-entrant! */
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

static void clearLevels()
/* clear out the data structure that we use to pack the snake */
{
int ii;

for(ii=0; ii < sizeof(Levels) / sizeof(Levels[0]); ii++)
    Levels[ii].init = FALSE;
maxLevel = 0;
}

static void calcSnake(struct snakeFeature *list, int level)
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

static int snakeHeight(struct track *tg, enum trackVisibility vis)
/* calculate height of all the snakes being displayed */
{
if (vis == tvDense)
    return tg->lineHeight;

int height = 0;
struct slList *item = tg->items;

item = tg->items;

for (item=tg->items;item; item = item->next)
    {
    height += tg->itemHeight(tg, item);
    }
return height;
}

// mySQL code to read in chains

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
    sqlDyStringPrintf(query, 
	"select chainId,tStart,tEnd,qStart from %sLink %-s where ",
	fullName, force);
else
    sqlDyStringPrintf(query, 
	"select chainId, tStart,tEnd,qStart from %sLink where chainId=%d and ",
	fullName, chainId);
if (!isSplit)
    sqlDyStringPrintf(query, "tName='%s' and ", chromName);
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

static void calcPackSnake(struct track *tg, void *item)
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

static int packSnakeItemHeight(struct track *tg, void *item)
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

static int snakeItemHeight(struct track *tg, void *item)
{
return packSnakeItemHeight(tg, item);
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

static void snakeDraw(struct track *tg, int seqStart, int seqEnd,
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
    if (vis == tvFull)
	y += tg->itemHeight(tg, item);
    } 
}

static void packSnakeDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
struct linkedFeatures  *lf = (struct linkedFeatures *)item;
calcPackSnake(tg, item);

if (lf->components == NULL)
    return;

#ifdef USE_HAL
boolean isHalSnake = lf->isHalSnake;
#else
boolean isHalSnake = FALSE;
#endif

struct snakeFeature  *sf = (struct snakeFeature *)lf->components, *prevSf = NULL;
int s = tg->itemStart(tg, item);
int sClp = (s < winStart) ? winStart : s;
int x1 = round((sClp - winStart)*scale) + xOff;
int textX = x1;
int yOff = y;
//boolean withLabels = (withLeftLabels && (vis == tvFull) && !tg->drawName);
unsigned   labelColor = MG_BLACK;

// draw the labels (this code needs a clean up )
if (0)//withLabels)
    {
    char *name = tg->itemName(tg, item);
    int nameWidth = mgFontStringWidth(font, name);
    int dotWidth = tl.nWidth/2;
    boolean snapLeft = FALSE;
    boolean drawNameInverted = FALSE;
    textX -= nameWidth + dotWidth;
    snapLeft = (textX < insideX);
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
        hvGfxSetClip(hvgSide, leftLabelX, yOff, insideWidth, tg->height);
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
        if(drawNameInverted)
            {
            hvGfxBox(hvg, textX - 1, y, nameWidth+1, tg->heightPer-1, color);
            hvGfxTextRight(hvg, textX, y, nameWidth, tg->heightPer, MG_WHITE, font, name);
            }
        else
            hvGfxTextRight(hvg, textX, y, nameWidth, tg->heightPer, labelColor, font, name);
        }
    }

#ifdef USE_HAL
// let's draw some blue bars for the duplications
struct hal_target_dupe_list_t* dupeList = lf->dupeList;

for(; dupeList ; dupeList = dupeList->next)
    {
    struct hal_target_range_t *range = dupeList->tRange;
    for(; range; range = range->next)
	{
	int s = range->tStart;
	int e = range->tStart + range->size;
	int sClp = (s < winStart) ? winStart : s;
	int eClp = (e > winEnd) ? winEnd : e;
	int x1 = round((sClp - winStart)*scale) + xOff;
	int x2 = round((eClp - winStart)*scale) + xOff;
	hvGfxLine(hvg, x1, y , x2, y , MG_BLUE);
	}
    }
y+=2;
#endif

// now we're going to draw the boxes

s = sf->start;
int lastE = -1;
int lastS = -1;
int offY = y;
int lineHeight = tg->lineHeight ;
int tStart, tEnd, qStart;
int  qs, qe;
int heightPer = tg->heightPer;
int lastX = -1,lastY = y;
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
    else
	y = offY + (sf->level * 2) * lineHeight;
    s = sf->start; e = sf->end;
    tEnd = sf->end;
    int osx;

    int sx, ex;
    if (!positiveRangeIntersection(winStart, winEnd, s, e))
	continue;
    osx = sx = round((double)((int)s-winStart)*scale) + xOff;
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
    color = (sf->orientation == -1) ? darkRedColor : darkBlueColor;

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
    char qAddress[4096];
    if (vis == tvFull)
	{
	safef(qAddress, sizeof qAddress, "qName=%s&qs=%d&qe=%d&qWidth=%d",tg->itemName(tg, item),  qs, qe,  winEnd - winStart);
	mapBoxHgcOrHgGene(hvg, s, e, sx+1, y, w-2, heightPer, tg->track,
		    buffer, buffer, NULL, TRUE, qAddress);
	}
    hvGfxBox(hvg, sx, y, w, heightPer, color);
    int ow = w;

    // now draw the mismatches if we're at high enough resolution 
    if ((winBaseCount < 50000) && (vis == tvFull))
    {
	char *twoBitString = trackDbSetting(tg->tdb, "twoBit");
	static struct twoBitFile *tbf = NULL;
	static char *lastTwoBitString = NULL;
	static struct dnaSeq *seq = NULL;
	static char *lastQName = NULL;

	// sequence for chain snakes is in 2bit files which we cache
	if (!isHalSnake)
	    {
	    if (twoBitString == NULL)
		twoBitString = "/gbdb/hg19/hg19.2bit";

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
	    }

	char *ourDna;
	if (isHalSnake)
	    ourDna = sf->sequence;
	else
	    ourDna = &seq->dna[sf->qStart];

	int seqLen = sf->qEnd - sf->qStart;
	toUpperN(ourDna, seqLen);
	if (!isHalSnake && (sf->orientation == -1))
	    reverseComplement(ourDna,seqLen);

	// get the reference sequence
	struct dnaSeq *extraSeq = hDnaFromSeq(database, chromName, sf->start, sf->end, dnaUpper);
	int si = s;
	char *ptr1 = extraSeq->dna;
	char *ptr2 = ourDna;
	for(; si < e; si++,ptr1++,ptr2++)
	    {
	    if (*ptr1 != *ptr2)
		{
		int misX1 = round((double)((int)si-winStart)*scale) + xOff;
		int misX2 = round((double)((int)(si+1)-winStart)*scale) + xOff;
		int w1 = misX2 - misX1;
		if (w1 < 1)
		    w1 = 1;

		// mismatch!
		hvGfxBox(hvg, misX1, y, w1, heightPer, MG_RED);
		}
	    }

	// if we're zoomed to base level, draw sequence of mismatch
	if (zoomedToBaseLevel)
	    {
	    spreadAlignString(hvg, osx, y, ow, heightPer, MG_WHITE, font, ourDna,
		extraSeq->dna, seqLen, TRUE, FALSE);
	    }

    }
    sf->drawn = TRUE;
    tEnd = e;
    tStart = s;
    qStart = sf->qStart;
    lastY = y;
    lastLevel = sf->level;
    //lastX = x;
    }

if (vis == tvDense)
    return;

// now we're going to draw the lines between the blocks

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

	// draw the vertical orange bars if there is an insert in the other sequence
	if ((winBaseCount < 50000) )
	    {
	    if ((sf->orientation == 1) && (qs != lastQEnd) && (lastE == s))
		{
		hvGfxLine(hvg, sx, y2 - lineHeight/2 , sx, y2 + lineHeight/2, MG_ORANGE);
		safef(buffer, sizeof buffer, "%dbp", qs - lastQEnd);
		mapBoxHgcOrHgGene(hvg, s, e, sx, y2 - lineHeight/2, 1, lineHeight, tg->track,
				    "foo", buffer, NULL, TRUE, NULL);
		}
	    else if ((sf->orientation == -1) && (qs != lastQEnd) && (lastS == e))
		{
		hvGfxLine(hvg, ex, y2 - lineHeight/2 , ex, y2 + lineHeight/2, MG_ORANGE);
		safef(buffer, sizeof buffer, "%dbp", qs - lastQEnd);
		mapBoxHgcOrHgGene(hvg, s, e, ex, y2 - lineHeight/2, 1, lineHeight, tg->track,
				    "foo", buffer, NULL, TRUE, NULL);
		}
	    }

	// now draw the lines between blocks
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
				"", buffer, NULL, TRUE, NULL);
			}
		    }
		else
		    {
		    if (lastX != sx)
			{
			hvGfxLine(hvg, lastX, y1, sx, y2, color);
			mapBoxHgcOrHgGene(hvg, s, e, lastX, y1, sx-lastX, 1, tg->track,
				"", buffer, NULL, TRUE, NULL);
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
	                    "", buffer, NULL, TRUE, NULL);

		}
	    else
		{
		char buffer[1024];
		safef(buffer, sizeof buffer, "%d-%d %dbp gap",prevSf->qStart,prevSf->qEnd, qs - lastQEnd);
		if (sf->orientation == -1)
		    {
		    hvGfxLine(hvg, lastX-1, y1, ex, y2, color);
		    hvGfxLine(hvg, ex, y2, ex, y2 + lineHeight , color);
		    mapBoxHgcOrHgGene(hvg, s, e, ex-1, y2, 2, lineHeight , tg->track,
				"", buffer, NULL, TRUE, NULL);
		    }
		else
		    {
		    hvGfxLine(hvg, lastX-1, y1, sx, y2, color);
		    hvGfxLine(hvg, sx, y2, sx, y2 + lineHeight , color);
		    mapBoxHgcOrHgGene(hvg, s, e, sx-1, y2, 2, lineHeight , tg->track,
				"", buffer, NULL, TRUE, NULL);

		    }
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
    lastS = s;
    lastE = e;
    lastLevel = sf->level;
    lastQEnd = qe;
    }
}


static void snakeDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
if ((tg->visibility == tvFull) || (tg->visibility == tvDense))
    packSnakeDrawAt(tg, item, hvg, xOff, y, scale, 
	font, color, vis);
}

static void fixItems(struct linkedFeatures *lf)
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
	slSort(&firstLf->components, snakeFeatureCmpQStart);
	firstLf = lf;
	}
    for (sf =  (struct snakeFeature *)lf->components; sf != NULL; sf = nextSf)
	{
	sf->qName = lf->name;
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

#ifdef USE_HAL
void halSnakeLoadItems(struct track *tg)
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    char *fileName = trackDbSetting(tg->tdb, "bigDataUrl");
    char *otherSpecies = trackDbSetting(tg->tdb, "otherSpecies");
    int handle = halOpenLOD(fileName);
    int needSeq = (winBaseCount < 50000) ? 1 : 0;
    struct hal_block_results_t *head = halGetBlocksInTargetRange(handle, otherSpecies, trackHubSkipHubName(database), chromName, winStart, winEnd, needSeq, 1);
    struct hal_block_t* cur = head->mappedBlocks;
    struct linkedFeatures *lf;
    struct hash *qChromHash = newHash(5);
    struct linkedFeatures *lfList = NULL;
    char buffer[4096];

#ifdef NOTNOW
    struct hal_target_dupe_list_t* targetDupeBlocks = head->targetDupeBlocks;

    for(;targetDupeBlocks; targetDupeBlocks = targetDupeBlocks->next)
	{
	printf("<br>id: %d qChrom %s\n", targetDupeBlocks->id, targetDupeBlocks->qChrom);
	struct hal_target_range_t *range = targetDupeBlocks->tRange;
	for(; range; range = range->next)
	    {
	    printf("<br>   %ld : %ld\n", range->tStart, range->size);
	    }
	}
#endif

    while (cur)
    {
	struct hashEl* hel;

	//safef(buffer, sizeof buffer, "%s.%c", cur->qChrom,cur->strand);
	safef(buffer, sizeof buffer, "%s", cur->qChrom);
	if ((hel = hashLookup(qChromHash, buffer)) == NULL)
	    {
	    AllocVar(lf);
	    lf->isHalSnake = TRUE;
	    slAddHead(&lfList, lf);
	    lf->start = 0;
	    lf->end = 1000000000;
	    lf->grayIx = maxShade;
	    lf->name = cloneString(buffer);
	    lf->extra = cloneString(buffer);
	    lf->orientation = (cur->strand == '+') ? 1 : -1;
	    hashAdd(qChromHash, lf->name, lf);

	    // now figure out where the blue bars go
	    struct hal_target_dupe_list_t* targetDupeBlocks = head->targetDupeBlocks;

	    for(;targetDupeBlocks; targetDupeBlocks = targetDupeBlocks->next)
		{
		if (sameString(targetDupeBlocks->qChrom, cur->qChrom))
		    {
		    struct hal_target_dupe_list_t* dupeList;
		    AllocVar(dupeList);
		    *dupeList = *targetDupeBlocks;
		    slAddHead(&lf->dupeList, dupeList);
		    // TODO: should clone the target_range structures
		    }
		}
	    }
	else
	    {
	    lf = hel->val;
	    }

	struct snakeFeature  *sf;
	AllocVar(sf);
	slAddHead(&lf->components, sf);
	
	sf->start = cur->tStart;
	sf->end = cur->tStart + cur->size;
	sf->qStart = cur->qStart;
	sf->qEnd = cur->qStart + cur->size;
	sf->orientation = (cur->strand == '+') ? 1 : -1;
	sf->sequence = cloneString(cur->sequence);

    //  printBlock(stdout, cur);
      cur = cur->next;
    }
    for(lf=lfList; lf ; lf = lf->next)
	{
	slSort(&lf->components, snakeFeatureCmpQStart);
	}
    //halFreeBlocks(head);
    //halClose(handle, myThread);

    tg->items = lfList;
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    tg->networkErrMsg = cloneString(errCatch->message->string);
    tg->drawItems = bigDrawWarning;
    tg->totalHeight = bigWarnTotalHeight;
    }
errCatchFree(&errCatch);
}
#endif // USE_HAL


void snakeLoadItems(struct track *tg)
// Load chains from a mySQL database
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
//ourStart = winStart;
//ourEnd = winEnd;
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

#ifdef USE_HAL
void halSnakeMethods(struct track *tg, struct trackDb *tdb, 
	int wordCount, char *words[])
{
//errAbort("halSnakeMethds\n");
linkedFeaturesMethods(tg);
tg->canPack = FALSE;
tdb->canPack = FALSE;
tg->loadItems = halSnakeLoadItems;
tg->drawItems = snakeDraw;
tg->mapItemName = lfMapNameFromExtra;
tg->subType = lfSubChain;
//tg->extraUiData = (void *) chainCart;
tg->totalHeight = snakeHeight; 

tg->drawItemAt = snakeDrawAt;
tg->itemHeight = snakeItemHeight;
}
#endif

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

tg->canPack = FALSE;
tg->loadItems = snakeLoadItems;
tg->drawItems = snakeDraw;
tg->mapItemName = lfMapNameFromExtra;
tg->subType = lfSubChain;
tg->extraUiData = (void *) chainCart;
tg->totalHeight = snakeHeight; 

tg->drawItemAt = snakeDrawAt;
tg->itemHeight = snakeItemHeight;
}
