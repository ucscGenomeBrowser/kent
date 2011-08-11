/* netTrack - stuff to handle loading and display of
 * netAlign type tracks in browser. Nets are derived
 * from cross-species alignments usually. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "netCart.h"
#include "chainNetDbLoad.h"

static char const rcsid[] = "$Id: netTrack.c,v 1.27 2010/05/11 01:43:28 kent Exp $";

struct cartOptions
    {
    enum netColorEnum netColor; /*  ChromColors, GrayScale */
    enum netLevelEnum netLevel; /* filter chains by level 1 thru 6 (0==All) */
    };

struct netItem
/* A net track item. */
    {
    struct netItem *next;
    int level;
    char *className;
    int yOffset;
    };

static char *netClassNames[] =  {
    "Level 1", "Level 2", "Level 3", "Level 4", "Level 5", "Level 6", };

static struct netItem *makeNetItems()
/* Make the levels for net alignment track. */
{
struct netItem *ni, *niList = NULL;
int i;
int numClasses = ArraySize(netClassNames);
for (i=0; i<numClasses; ++i)
    {
    AllocVar(ni);
    ni->level = i;
    ni->className = netClassNames[i];
    slAddHead(&niList, ni);
    }
slReverse(&niList);
return niList;
}

static void netLoad(struct track *tg)
/* Load up net tracks.  (Will query database during drawing for a change.) */
{
tg->items = makeNetItems();
}

static void netFree(struct track *tg)
/* Free up netGroup items. */
{
slFreeList(&tg->items);
}

static char *netName(struct track *tg, void *item)
/* Return name of net level track. */
{
struct netItem *ni = item;
return ni->className;
}

static struct track *rTg;
static struct hvGfx *rHvg;
static int rX;          /* X offset of drawing area. */
static int rHeightPer;  /* Height of boxes. */
static int rMidLineOff; /* Offset to draw connecting lines. */
static int rNextLine;   /* Y offset to next line. */
static double rScale;   /* Scale from bases to pixels. */
static boolean rIsFull; /* Full display? */

static char *netColorLastChrom;

static void netColorClearCache()
{
netColorLastChrom = "UNKNOWN";
}

static Color netColor(struct track *tg, char *chrom, int level)
/* return appropriate color for a chromosome/scaffold */
{
struct cartOptions *netCart;

netCart = (struct cartOptions *) tg->extraUiData;

if (netCart->netColor == netColorGrayScale)
    switch(level)
	{
	case 1: return(shadesOfGray[9]); break;
	case 2: return(shadesOfGray[7]); break;
	case 3: return(shadesOfGray[6]); break;
	case 4: return(shadesOfGray[5]); break;
	case 5: return(shadesOfGray[4]); break;
	case 6: return(shadesOfGray[3]); break;
	default: return(shadesOfGray[2]); break;
	}

static Color color;
if (!sameString(chrom, netColorLastChrom))
    color = getSeqColor(chrom, rHvg);
if (0 == color)
    color = 1;	/*	don't display in white	*/
netColorLastChrom = chrom;
return color;
}

static void rNetBox(struct hvGfx *hvg, struct cnFill *fill, int start, int end, int y, int level,
	Color color, Color barbColor, int orientation)
/* Draw a scaled box. */
{
int x1,x2,w;

/* Do clipping before scaling because scaling may cause
 * integer overflow. */
if (start < winStart) start = winStart;
if (end > winEnd) end = winEnd;
if (start >= end)
    return;

x1 = round((double)(start-winStart)*rScale) + rX;
x2 = round((double)(end-winStart)*rScale) + rX;
w = x2-x1;

if (w < 1)
    w = 1;
hvGfxBox(rHvg, x1, y, w, rHeightPer, color);
if (w > 3)
    clippedBarbs(rHvg, x1, y+rMidLineOff, w, 2, 5, orientation, barbColor, TRUE);
if (w > 1)
    {
    if (rNextLine > 0)	 /* Put up click info in full mode. */
	{
	struct dyString *bubble = newDyString(256);
	char depth[8];
	snprintf(depth, sizeof(depth), "%d", level);
	dyStringPrintf(bubble, "%s %c %dk ",
	    fill->qName, fill->qStrand, fill->qStart/1000);
	mapBoxHc(hvg, start, end, x1, y, w, rHeightPer, rTg->track,
	    depth, bubble->string);
	dyStringFree(&bubble);
	}
    }
}

static void rNetLine(struct hvGfx *hvg, struct cnFill *gap, int y, int level, Color color,
	int orientation)
/* Write out line filling gap and associated info. */
{
int start = gap->tStart;
int end = start + gap->tSize;
int x1,x2,w;

/* Do clipping before scaling because scaling may cause
 * integer overflow. */
if (start < winStart) start = winStart;
if (end > winEnd) end = winEnd;
if (start >= end)
    return;
x1 = round((double)(start-winStart)*rScale) + rX;
x2 = round((double)(end-winStart)*rScale) + rX;
w = x2-x1;

if (w >= 1)
    {
    struct dyString *bubble = newDyString(256);
    char depth[8];
    int midY = y + rMidLineOff;
    clippedBarbs(rHvg, x1, midY, w, 2, 5, orientation, color, FALSE);
    hvGfxLine(rHvg, x1, midY, x2, midY, color);
    if (rNextLine > 0)	 /* Put up click info in full mode. */
	{
	snprintf(depth, sizeof(depth), "%d", level);
	dyStringPrintf(bubble, "size %d/%d Ns %d/%d newRep %d/%d",
	    gap->qSize, gap->tSize, gap->qN, gap->tN,
	    gap->qNewR, gap->tNewR);
        mapBoxHc(hvg, start, end, x1, y, w, rHeightPer, rTg->track,
		depth, bubble->string);
	dyStringFree(&bubble);
	}
    }
}


static void rNetDraw(struct track *tg, struct hvGfx *hvg, struct cnFill *fillList, int level, int y)
/* Recursively draw net. */
{
struct cnFill *fill;
struct cnFill *gap;
Color color, invColor;
int orientation;

for (fill = fillList; fill != NULL; fill = fill->next)
    {
    color = netColor(tg, fill->qName, level);
    invColor = hvGfxContrastingColor(rHvg, color);
    orientation = orientFromChar(fill->qStrand);
    if (fill->children == NULL || fill->tSize * rScale < 2.5)
    /* Draw single solid box if no gaps or no room to draw gaps. */
        {
	rNetBox(hvg, fill, fill->tStart, fill->tStart + fill->tSize,
		y, level, color, invColor, orientation);
	}
    else
        {
	int fStart = fill->tStart;
	for ( gap = fill->children; gap != NULL; gap = gap->next)
	    {
	    if (gap->tSize * rScale >= 1)
		{
		rNetBox(hvg, fill, fStart, gap->tStart, y, level,
			color, invColor, orientation);
		fStart = gap->tStart + gap->tSize;
		rNetLine(hvg, gap, y, level+1, color, orientation);
		}
	    }
	rNetBox(hvg, fill, fStart, fill->tStart + fill->tSize, y, level,
		color, invColor, orientation);
	}
    for (gap = fill->children; gap != NULL; gap = gap->next)
        {
	if (gap->children)
	    {
	    rNetDraw(tg, hvg, gap->children, level+2, y + rNextLine);
	    }
	}
    }
}

static void netDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw routine for netAlign type tracks.  This will load
 * the items as well as drawing them. */
{
/* Load Net. */
struct chainNet *net = chainNetLoadRange(database, tg->table, chromName,
	seqStart, seqEnd, NULL);

if (net != NULL)
    {
    /* Copy parameters to statics for recursive routine. */
    rTg = tg;
    rHvg = hvg;
    rX = xOff;

    /* Clear cache. */
    netColorClearCache();

    /* Compute a few other positional things for recursive routine. */
    rHeightPer = tg->heightPer;
    rMidLineOff = rHeightPer/2;
    rIsFull = (vis == tvFull || vis == tvPack || vis == tvSquish);
    if (rIsFull)
	rNextLine = tg->lineHeight;
    else
	rNextLine = 0;
    rScale = scaleForPixels(width);

    /* Recursively do main bit of work. */
    rNetDraw(tg, hvg, net->fillList, 1, yOff);
    chainNetFree(&net);
    }
#ifndef IMAGEv2_DRAG_SCROLL
if (vis == tvDense)
    mapBoxToggleVis(hvg, xOff, yOff, width, tg->heightPer, tg);
#endif///ndef IMAGEv2_DRAG_SCROLL
}

static int netTotalHeight(struct track *tg, enum trackVisibility vis)
/* A copy of tgFixedTotalHeightNoOverflow() with visibility forced */
{
int ret = 0;
switch (vis)
    {
    case tvSquish:
	ret = tgFixedTotalHeightOptionalOverflow(tg, tvFull, (tl.fontHeight/3)+1, (tl.fontHeight/3), FALSE);
	break;
    case tvPack:
	ret = tgFixedTotalHeightOptionalOverflow(tg, tvFull, (tl.fontHeight/2)+1, (tl.fontHeight/2), FALSE);
	break;
    case tvFull:
	ret = tgFixedTotalHeightOptionalOverflow(tg, tvFull, tl.fontHeight+1, tl.fontHeight, FALSE);
	break;
    case tvDense:
	ret = tgFixedTotalHeightOptionalOverflow(tg, tvDense, tl.fontHeight+1, tl.fontHeight, FALSE);
	break;
    case tvHide:
    default:
	break;
    }
return(ret);
}

void netMethods(struct track *tg)
/* Make track group for chain/net alignment. */
{
struct cartOptions *netCart;

AllocVar(netCart);

netCart->netColor = netFetchColorOption(cart, tg->tdb, FALSE);
netCart->netLevel = netFetchLevelOption(cart, tg->tdb, FALSE);

tg->loadItems = netLoad;
tg->freeItems = netFree;
tg->drawItems = netDraw;
tg->itemName = netName;
tg->mapItemName = netName;
tg->totalHeight = netTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
tg->mapsSelf = TRUE;
tg->extraUiData = (void *) netCart;
}
