/* chainTrack - stuff to load and display chain type tracks in
 * browser.   Chains are typically from cross-species genomic
 * alignments. */

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

static char const rcsid[] = "$Id: chainTrack.c,v 1.27 2006/02/23 01:41:31 baertsch Exp $";


struct cartOptions
    {
    enum chainColorEnum chainColor; /*  ChromColors, ScoreColors, NoColors */
    int scoreFilter ; /* filter chains by score if > 0 */
    };

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
struct simpleFeature *sf;
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
//printf("read chain\n");
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
	//printf("%d \n",sf->qStart);
	sf->grayIx = lf->orientation;
	slAddHead(&lf->components, sf);
	}
    }
sqlFreeResult(&sr);
dyStringFree(&query);
}

int snakeItemHeight(struct track *tg, void *item)
{
//printf("snakeItemHeight\n");
//return 40;
struct linkedFeatures  *lf = (struct linkedFeatures *)item;
struct simpleFeature  *sf;
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
for (sf =  lf->components; sf ;  sf = sf->next)
    {
    int orient = sf->grayIx;

    s = sf->start; e = sf->end;
    /*
    if ((e < winStart) || (s > winEnd))
	continue;
    if (s < winStart) s = winStart;
    */
    //printf("bounds %d %d\n",winStart, winEnd);
    
    if (((oldOrient) && (oldOrient != orient))
	||  ((oldOrient == 1) && (tEnd) && (s < tEnd))
	||  ((oldOrient == -1) && (tStart) && (e > tStart)))
	{
	//printf("plus \n");
	size += lineHeight;
	}
    oldOrient = orient;
    tEnd = e;
    tStart = s;
    }
return size;
}

static int linkedFeaturesCmpScore(const void *va, const void *vb)
/* Help sort linkedFeatures by starting pos. */
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
struct linkedFeatures  *lfStart = (struct linkedFeatures *)tg->items;
struct linkedFeatures  *lf;
double scale = scaleForWindow(width, seqStart, seqEnd);

for(lf = lfStart; lf; lf = lf->next)
    {
    struct simpleFeature  *sf;

    lf->score = 0;
    for (sf =  lf->components; sf != NULL;  sf = sf->next)
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

void snakeDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
struct linkedFeatures  *lf = (struct linkedFeatures *)item;
struct simpleFeature  *sf, *prevSf;
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
for (sf =  lf->components; sf != NULL; lastQEnd = qe, prevSf = sf, sf = sf->next)
    {
    int orient = sf->grayIx;
    midY = y + heightPer/2;

    qs = sf->qStart;
    s = sf->start; e = sf->end;
    if (qs < lastQEnd )
	continue;

    qe = sf->qEnd;
    if ((e < winStart) || (s > winEnd))
	continue;

    if (s < winStart) s = winStart;
    
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

    drawScaledBoxSample(hvg, s, e, scale, xOff, y, heightPer, 
			color, lf->score );
    tEnd = e;
    tStart = s;
    qStart = sf->qStart;
    oldOrient = orient;
    }
}

static int simpleFeatureCmpQStart(const void *va, const void *vb)
{
const struct simpleFeature *a = *((struct simpleFeature **)va);
const struct simpleFeature *b = *((struct simpleFeature **)vb);
int diff = a->qStart - b->qStart;

if (diff == 0)
    {
    diff = a->start - b->start;
    }

return diff;
}

static int linkedFeaturesCmpChrom(const void *va, const void *vb)
/* Help sort linkedFeatures by starting pos. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
return strcmp(a->name, b->name);
}

int snakeHeight(struct track *tg, enum trackVisibility vis)
/* set up size of sequence logo */
{
int height = 0;
struct slList *item = tg->items, *nextItem;
struct linkedFeatures *firstLf, *lf = (struct linkedFeatures *)tg->items;
struct simpleFeature  *sf,  *nextSf;

slSort(&tg->items, linkedFeaturesCmpChrom);

item = tg->items;

lf =  (struct linkedFeatures *)tg->items;
if (item)
    {
    firstLf = lf;
    for (;item; item = nextItem)
	{
	struct linkedFeatures *lf = (struct linkedFeatures *)item;
	nextItem = item->next;
	if (!sameString(firstLf->name, lf->name))
	    {
	    slSort(&firstLf->components, simpleFeatureCmpQStart);
	    firstLf = lf;
	    }
	for (sf =  lf->components; sf != NULL; sf = nextSf)
	    {
	    sf->grayIx = lf->orientation;
	    nextSf = sf->next;
	    if (firstLf != lf)
		{
		
		lf->components = NULL;
		slAddHead(&firstLf->components, sf);
		}
	    }
	}

    slSort(&firstLf->components, simpleFeatureCmpQStart);

    for (item=tg->items;item; item = item->next)
	{
	height += tg->itemHeight(tg, item);
	}
    }
return 1500; //height;
}

static void chainDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw chained features. This loads up the simple features from 
 * the chainLink table, calls linkedFeaturesDraw, and then
 * frees the simple features again. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf;
struct lm *lm;
struct hash *hash;	/* Hash of chain ids. */
struct sqlConnection *conn;
//double scale = ((double)(winEnd - winStart))/width;
char fullName[64];
int start, end, extra;
struct simpleFeature *lastSf = NULL;
int maxOverLeft = 0, maxOverRight = 0;
int overLeft, overRight;

if (tg->items == NULL)		/*Exit Early if nothing to do */
    return;

lm = lmInit(1024*4);
hash = newHash(0);
conn = hAllocConn(database);

/* Make up a hash of all linked features keyed by
 * id, which is held in the extras field.  To
 * avoid burning memory on full chromosome views
 * we'll just make a single simple feature and
 * exclude from hash chains less than three pixels wide, 
 * since these would always appear solid. */
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    //double pixelWidth = (lf->end - lf->start) / scale;
    if (1)//pixelWidth >= 2.5)
	{
	char buf[256];
	struct chain *pChain = lf->extra;
	safef(buf, sizeof(buf), "%d", pChain->id);
	//hashAdd(hash, lf->extra, lf);
	hashAdd(hash, buf, lf);
	overRight = lf->end - seqEnd;
	if (overRight > maxOverRight)
	    maxOverRight = overRight;
	overLeft = seqStart - lf->start ;
	if (overLeft > maxOverLeft)
	    maxOverLeft = overLeft;
	}
    else
	{
	lmAllocVar(lm, sf);
	sf->start = lf->start;
	sf->end = lf->end;
	sf->grayIx = lf->grayIx;
	lf->components = sf;
	}
    }

/* if some chains are bigger than 3 pixels */
if (hash->size)
    {
    boolean isSplit = TRUE;
    /* Make up range query. */
    sprintf(fullName, "%s_%s", chromName, tg->table);
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
#define STARTSLOP	10000
#define MULTIPLIER	10
#define MAXLOOK		100000
	extra = (STARTSLOP < maxOverLeft) ? STARTSLOP : maxOverLeft;
	start = seqStart - extra;
	extra = (STARTSLOP < maxOverRight) ? STARTSLOP : maxOverRight;
	end = seqEnd + extra;
	doQuery(conn, fullName, lm,  hash, start, end,  isSplit, -1);

	for (lf = tg->items; lf != NULL; lf = lf->next)
	    {
	    struct chain *pChain = lf->extra;
	    int chainId = pChain->id;

	    if (lf->components == NULL)
		continue;
	    slSort(&lf->components, linkedFeaturesCmpStart);
	    extra = (STARTSLOP < maxOverRight)?STARTSLOP:maxOverRight;
	    end = seqEnd + extra;
	    lastSf = NULL;
	    while (lf->end > end )
		{
		for(lastSf=sf=lf->components;sf;lastSf=sf,sf=sf->next)
		    ;

		/* get out if we have an element off right side */
		if (( (lastSf != NULL) &&(lastSf->end > seqEnd)) || (extra > MAXLOOK))
		    break;

		extra *= MULTIPLIER;
		start = end;
		end = start + extra;
                doQuery(conn, fullName, lm,  hash, start, end,  isSplit, chainId);
		slSort(&lf->components, linkedFeaturesCmpStart);
		}

	    /* if we didn't find an element off to the right , add one */
	    if ((lf->end > seqEnd) && ((lastSf == NULL) ||(lastSf->end < seqEnd)))
		{
		lmAllocVar(lm, sf);
		sf->start = seqEnd;
		sf->end = seqEnd+1;
		sf->grayIx = lf->grayIx;
		sf->qStart = 0;
		sf->qEnd = sf->qStart + (sf->end - sf->start);
		sf->next = lf->components;
		lf->components = sf;
		slSort(&lf->components, linkedFeaturesCmpStart);
		}

	    /* we know we have a least one component off right
	     * now look for one off left
	     */
	    extra = (STARTSLOP < maxOverLeft) ? STARTSLOP:maxOverLeft;
	    start = seqStart - extra; 
	    while((extra < MAXLOOK) && (lf->start < seqStart) && 
		(lf->components->start > seqStart))
		{
		extra *= MULTIPLIER;
		end = start;
		start = end - extra;
                if (start < 0)
                    start = 0;
		doQuery(conn, fullName, lm,  hash, start, end,  isSplit, chainId);
		slSort(&lf->components, linkedFeaturesCmpStart);
		}
	    if ((lf->components->start > seqStart) && (lf->start < lf->components->start))
		{
		lmAllocVar(lm, sf);
		sf->start = 0;
		sf->end = 1;
		sf->grayIx = lf->grayIx;
		sf->qStart = lf->components->qStart;
		sf->qEnd = sf->qStart + (sf->end - sf->start);
		sf->next = lf->components;
		lf->components = sf;
		slSort(&lf->components, linkedFeaturesCmpStart);
		}
	    }
	}
    }
if (1)
{
snakeHeight(tg, vis);
snakeDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
	font, color, vis);
	}
    else
    {
linkedFeaturesDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
	font, color, vis);
	}
/* Cleanup time. */
for (lf = tg->items; lf != NULL; lf = lf->next)
    lf->components = NULL;

lmCleanup(&lm);
freeHash(&hash);
hFreeConn(&conn);
}

void snakeLoadItems(struct track *tg)
/* Load up all of the chains from correct table into tg->items 
 * item list.  At this stage to conserve memory for other tracks
 * we don't load the links into the components list until draw time. */
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

snprintf( optionChr, sizeof(optionChr), "%s.chromFilter", tg->table);
optionChrStr = cartUsualString(cart, optionChr, "All");
if (startsWith("chr",optionChrStr)) 
    {
    snprintf(extraWhere, sizeof(extraWhere), 
            "qName = \"%s\" and score > %d",optionChrStr, 
            chainCart->scoreFilter);
    sr = hRangeQuery(conn, track, chromName, winStart, winEnd, 
            extraWhere, &rowOffset);
    }
else
    {
    if (chainCart->scoreFilter > 0)
        {
        snprintf(extraWhere, sizeof(extraWhere), 
                "score > \"%d\"",chainCart->scoreFilter);
        sr = hRangeQuery(conn, track, chromName, winStart, winEnd, 
                extraWhere, &rowOffset);
        }
    else
        {
        snprintf(extraWhere, sizeof(extraWhere), " ");
        sr = hRangeQuery(conn, track, chromName, winStart, winEnd, 
                NULL, &rowOffset);
        }
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    //char buf[16];
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
    snprintf(buffer, sizeof(buffer), "%s", chain.qName);
    //snprintf(buffer, sizeof(buffer), "%s %c %dk", 
    	//chain.qName, chain.qStrand, qs/1000);
    lf->name = cloneString(buffer);
    //snprintf(lf->name, sizeof(lf->name), "%s %c %dk", 
    //	chain.qName, chain.qStrand, qs/1000);
    //snprintf(lf->name, sizeof(lf->name), "%s", 
    //	chain.qName);
    //snprintf(lf->popUp, sizeof(lf->name), "%s %c start %d size %d",
    //	chain.qName, chain.qStrand, qs, chain.qEnd - chain.qStart);
    //snprintf(buf, sizeof(buf), "%d", chain.id);
    //lf->extra = cloneString(buf);
    lf->extra = pChain;
    //printf("setting extra to %llu\n",(long long) pChain);
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

//boolean normScoreAvailable = FALSE;
struct cartOptions *chainCart;
//char scoreOption[256];

AllocVar(chainCart);

boolean normScoreAvailable = chainDbNormScoreAvailable(tdb);
//normScoreAvailable = chainDbNormScoreAvailable(chromName, tg->table, NULL);

/*	what does the cart say about coloring option	*/
chainCart->chainColor = chainFetchColorOption(cart, tdb, FALSE);

//snprintf( scoreOption, sizeof(scoreOption), "%s.scoreFilter", tdb->tableName);
chainCart->scoreFilter = cartUsualIntClosestToHome(cart, tdb,
	FALSE, SCORE_FILTER, 0);
//chainCart->scoreFilter = cartUsualInt(cart, scoreOption, 0);


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

    snprintf(option, sizeof(option), "%s.color", tg->table);
    optionStr = cartUsualString(cart, option, "on");
    if (differentWord("on",optionStr))
	{
	setNoColor(tg);
	chainCart->chainColor = chainColorNoColors;
	}
    else
	chainCart->chainColor = chainColorChromColors;
    }

tg->loadItems = snakeLoadItems;
tg->drawItems = chainDraw;
tg->mapItemName = lfMapNameFromExtra;
tg->subType = lfSubChain;
tg->extraUiData = (void *) chainCart;
//    tg->drawItems = snakeDraw;
    tg->drawItemAt = snakeDrawAt;
    tg->totalHeight = snakeHeight; 
    tg->itemHeight = snakeItemHeight;
}
