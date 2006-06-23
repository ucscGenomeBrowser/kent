/* netTrack - stuff to handle loading and display of
 * netAlign type tracks in browser. Nets are derived
 * from cross-species alignments usually. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "chainNet.h"
#include "chainNetDbLoad.h"

static char const rcsid[] = "$Id: netTrack.c,v 1.20 2006/06/23 23:45:04 kent Exp $";

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
static struct vGfx *rVg;
static int rX;          /* X offset of drawing area. */
static int rHeightPer;  /* Height of boxes. */
static int rMidLineOff; /* Offset to draw connecting lines. */
static int rNextLine;   /* Y offset to next line. */
static double rScale;   /* Scale from bases to pixels. */
static boolean rIsFull; /* Full display? */

static char *netColorLastChrom;

static void netColorClearCashe()
{
netColorLastChrom = "UNKNOWN";
}

static Color netColor(char *chrom)
/* return appropriate color for a chromosome/scaffold */
{
static Color color;
if (!sameString(chrom, netColorLastChrom))
    color = getSeqColor(chrom, rVg);
if (0 == color)
    color = 1;	/*	don't display in white	*/
netColorLastChrom = chrom;
return color;
}

static void rNetBox(struct cnFill *fill, int start, int end, int y, int level,
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
vgBox(rVg, x1, y, w, rHeightPer, color);
if (w > 3)
    clippedBarbs(rVg, x1, y+rMidLineOff, w, 2, 5, orientation, barbColor, TRUE);
if (w > 1)
    {
    if (rNextLine > 0)	 /* Put up click info in full mode. */
	{
	struct dyString *bubble = newDyString(256);
	char depth[8];
	snprintf(depth, sizeof(depth), "%d", level);
	dyStringPrintf(bubble, "%s %c %dk ", 
	    fill->qName, fill->qStrand, fill->qStart/1000);
	mapBoxHc(start, end, x1, y, w, rHeightPer, rTg->mapName, 
	    depth, bubble->string);
	dyStringFree(&bubble);
	}
    }
}

static void rNetLine(struct cnFill *gap, int y, int level, Color color,
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
    clippedBarbs(rVg, x1, midY, w, 2, 5, orientation, color, FALSE);
    vgLine(rVg, x1, midY, x2, midY, color);
    if (rNextLine > 0)	 /* Put up click info in full mode. */
	{
	snprintf(depth, sizeof(depth), "%d", level);
	dyStringPrintf(bubble, "size %d/%d Ns %d/%d newRep %d/%d", 
	    gap->qSize, gap->tSize, gap->qN, gap->tN,
	    gap->qNewR, gap->tNewR);
	    mapBoxHc(start, end, x1, y, w, rHeightPer, rTg->mapName, 
		depth, bubble->string);
	dyStringFree(&bubble);
	}
    }
}


static void rNetDraw(struct cnFill *fillList, int level, int y)
/* Recursively draw net. */
{
struct cnFill *fill;
struct cnFill *gap;
Color color, invColor;
int orientation;

for (fill = fillList; fill != NULL; fill = fill->next)
    {
    color = netColor(fill->qName);
    invColor = vgContrastingColor(rVg, color);
    orientation = orientFromChar(fill->qStrand);
    if (fill->children == NULL || fill->tSize * rScale < 2.5)
    /* Draw single solid box if no gaps or no room to draw gaps. */
        {
	rNetBox(fill, fill->tStart, fill->tStart + fill->tSize, y, level,
		color, invColor, orientation);
	}
    else
        {
	int fStart = fill->tStart;
	for ( gap = fill->children; gap != NULL; gap = gap->next)
	    {
	    if (gap->tSize * rScale >= 1)
		{
		rNetBox(fill, fStart, gap->tStart, y, level,
			color, invColor, orientation);
		fStart = gap->tStart + gap->tSize;
		rNetLine(gap, y, level+1, color, orientation);
		}
	    }
	rNetBox(fill, fStart, fill->tStart + fill->tSize, y, level,
		color, invColor, orientation);
	}
    for (gap = fill->children; gap != NULL; gap = gap->next)
        {
	if (gap->children)
	    {
	    rNetDraw(gap->children, level+2, y + rNextLine);
	    }
	}
    }
}

static void netDraw(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw routine for netAlign type tracks.  This will load
 * the items as well as drawing them. */
{
/* Load Net. */
struct chainNet *net = chainNetLoadRange(database, tg->mapName, chromName,
	seqStart, seqEnd, NULL);

if (net != NULL)
    {
    /* Copy parameters to statics for recursive routine. */
    rTg = tg;
    rVg = vg;
    rX = xOff;

    /* Clear cashe. */
    netColorClearCashe();

    /* Compute a few other positional things for recursive routine. */
    rHeightPer = tg->heightPer;
    rMidLineOff = rHeightPer/2;
    rIsFull = (vis == tvFull);
    if (rIsFull)
	rNextLine = tg->lineHeight;
    else
	rNextLine = 0;
    rScale = scaleForPixels(width);

    /* Recursively do main bit of work. */
    rNetDraw(net->fillList, 1, yOff);
    chainNetFree(&net);
    }
if (vis == tvDense)
    mapBoxToggleVis(xOff, yOff, width, tg->heightPer, tg);
}

void netMethods(struct track *tg)
/* Make track group for chain/net alignment. */
{
tg->loadItems = netLoad;
tg->freeItems = netFree;
tg->drawItems = netDraw;
tg->itemName = netName;
tg->mapItemName = netName;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
tg->mapsSelf = TRUE;
}

