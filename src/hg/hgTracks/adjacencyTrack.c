/* adjacencyTrack -- handlers for Variant Call Format data. */

#include "common.h"
#include "bigWarn.h"
#include "dystring.h"
#include "errCatch.h"
#include "hacTree.h"
#include "hdb.h"
#include "hgColors.h"
#include "hgTracks.h"
#include "pgSnp.h"
#include "trashDir.h"
#include "adjacency.h"
//#include "adjacencyUi.h"


struct adjacencyInfo
{
struct adjacencyInfo *next;
struct adjacency *list;
boolean calced;
int height;
int ordering;
};

unsigned makeColor(unsigned score)
{
unsigned colorDelta;
unsigned red, green, blue;

if (score > 500)
    {
    colorDelta = (score - 500) * 255 / 500;
    red = 255;
    green = 127 - colorDelta/2;
    blue = 127 - colorDelta/2;
    }
else
    {
    colorDelta = (score) * 255 / 500;
    blue = 255;
    green = colorDelta/2;
    red = colorDelta/2;
    }
unsigned color =  MAKECOLOR_32(red, green, blue);

return color;
}

int cmpOrdering(const void *va, const void *vb)
{
const struct adjacency *a = *((struct adjacency **)va);
const struct adjacency *b = *((struct adjacency **)vb);

int diff = a->ordering - b->ordering;

return diff;
}

static void adjacencyLoadItems(struct track *tg)
{
struct adjacency *list = NULL, *el;
struct sqlResult *sr;
char **row;
int rowOffset = 0;
struct sqlConnection *conn;
char *table = tg->table;
struct customTrack *ct = tg->customPt;
if (ct == NULL)
    conn = hAllocConn(database);
else
    {
    conn = hAllocConn(CUSTOM_TRASH);
    table = ct->dbTableName;
    }

sr = hRangeQuery(conn, table, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = adjacencyLoad(row+rowOffset);
    // only grab those that have a tick on screen
    if ( sameString("SEGMENT", el->name) ||
	((el->chromStart >= winStart) && (el->chromStart < winEnd)) ||
	(sameString(chromName, el->name) &&
	    ((el->chromEnd >= winStart) && (el->chromEnd < winEnd)))
	)
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);

slSort(&list, cmpOrdering);

struct adjacencyInfo *adjInfoList = NULL;
struct adjacencyInfo *adjInfo = NULL;
struct adjacency *next;
for(el = list; el; el = next)
    {
    next = el->next;
    el->next = NULL;

    if ((adjInfo == NULL) || (adjInfo->ordering != el->ordering))
	{
	AllocVar(adjInfo);
	slAddHead(&adjInfoList, adjInfo);
	adjInfo->ordering = el->ordering;
	adjInfo->calced = FALSE;
	adjInfo->list = NULL;
	}

    slAddHead(&adjInfo->list, el);
    }
slReverse(&adjInfoList);
tg->items = adjInfoList;

//printf("adfInfo Count %d\n", slCount(adjInfoList));
}

static void arcThrough( struct hvGfx *hvg, int x1, int y1,
    int mx, int my, int x2, int y2, unsigned color)
{
hvGfxLine(hvg, x1, y1, mx, my, color);
hvGfxLine(hvg, mx, my, x2, y2, color);
}

void adjacencyDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y,
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
struct adjacency *adjacency = item;
int heightPer = tg->heightPer;
//int s = max(bed->chromStart, winStart), e = min(bed->chromEnd, winEnd);
int s = adjacency->chromStart, e = adjacency->chromEnd;
int x1 = round((s-winStart)*scale) + xOff;
int x2 = round((e-winStart)*scale) + xOff;

if (sameString("SEGMENT", adjacency->name))
    {
    hvGfxBox(hvg, x1, y + 3 , x2-x1, heightPer, color);
    return;
    }
hvGfxBox(hvg, x1, y - heightPer, 1, heightPer, color);
if (sameString(adjacency->chrom, adjacency->name))
    {
    hvGfxBox(hvg, x2, y - heightPer, 1, heightPer, color);
    arcThrough(hvg, x1, y - heightPer, 
	(x1 + x2) / 2, y - adjacency->level, 
	x2, y - heightPer, color);
    /*
    arcThrough(hvg, x1, y - heightPer, 
	(x1 + x2) / 2, y - (adjacency->level + 2) * heightPer, 
	x2, y - heightPer, color);
	*/
    }
else
    {
    char buffer[100];
    safef(buffer, sizeof buffer, "%s", adjacency->name);
    int textWidth = mgFontStringWidth(font, buffer);
    int fontHeight = mgFontLineHeight(font);
    hvGfxTextCentered(hvg, x1 - textWidth / 2, y - 2 * heightPer, textWidth , fontHeight, MG_BLACK, font, buffer);
    }

//y += 2 * heightPer;

int xt1, xt2;
if (adjacency->strand1[0] == '+')
    xt1 = x1 + 5;
else
    xt1 = x1 - 5;
hvGfxLine(hvg, x1, y, xt1, y, color);

if (sameString(adjacency->chrom, adjacency->name))
    {
    if (adjacency->strand2[0] == '+')
	xt2 = x2 + 5;
    else
	xt2 = x2 - 5;

    hvGfxLine(hvg, x2, y, xt2, y, color);
    }

// now write out the other chrom if there is one
}

struct edge
{
struct edge *next;
unsigned x;
struct adjacency *adj;
boolean in;
};


#ifdef NOTNOW
static void makeEdge(struct edge **edgeList, struct adjacency *adj)
{
struct edge *edge;

AllocVar(edge);
slAddHead(edgeList, edge);
edge->x = adj->chromStart;
edge->adj = adj;
edge->in = TRUE;

AllocVar(edge);
slAddHead(edgeList, edge);
edge->adj = adj;
edge->in = FALSE;
if (sameString(adj->chrom, adj->name))
    edge->x = adj->chromEnd;
else
    edge->x = adj->chromStart;
}

int cmpEdgeX(const void *va, const void *vb)
{
const struct edge *a = *((struct edge **)va);
const struct edge *b = *((struct edge **)vb);

int diff = a->x - b->x;

if (diff == 0)
    diff = b->in - a->in;
if (diff == 0)
    {
    int bWidth = b->adj->chromEnd - b->adj->chromStart;
    int aWidth = a->adj->chromEnd - a->adj->chromStart;
    diff = bWidth - aWidth;
    }

return diff;
}

static struct adjacency *removeAdjacency(struct adjacency *list, 
    struct adjacency *toRemove)
{
struct adjacency *prev = NULL;
struct adjacency *adj;

for(adj = list; adj != toRemove; prev = adj, adj = adj->next)
    ;

struct adjacency *ret;
if (prev == NULL)
    {
    ret = list->next;
    list->next = NULL;
    return ret;
    }

prev->next = adj->next;
adj->next = NULL;

return list;
}
#endif

void calcAdjacencyLevels(struct adjacencyInfo *adjInfo, int heightPer)
{
if (adjInfo->calced)
    return;

struct adjacency *adj = adjInfo->list;

//struct adjacency *next;
int maxLevel = 0;
for(; adj; adj = adj->next)
    {
    adj->level = 5*log((double)adj->chromEnd - adj->chromStart);
    if (adj->level > maxLevel)
	maxLevel = adj->level;
    }
adjInfo->height =  2* (6 + maxLevel);
adjInfo->calced = TRUE;

#ifdef NOTNOW
struct adjacency *adj = adjInfo->list;
struct adjacency *segList = NULL;

// build up list of edges to sort by
struct edge *edgeList = NULL;
struct adjacency *next;
for(; adj; adj = next)
    {
    next = adj->next;
    if (differentString(adj->name, "SEGMENT"))
	makeEdge(&edgeList, adj);
    else
	slAddHead(&segList, adj);

    adj->level = 0;
    }
slSort(&edgeList, cmpEdgeX);

//printf("Edge Count %d\n",slCount(edgeList));
//for(edge = edgeList ; edge; edge = edge->next)
//    printf("edge x %d in %d |", edge->x, edge->in);

struct edge *edge;
int level = 0;
int maxLevel = 0;
struct adjacency *onDeck = NULL;
for(edge = edgeList ; edge; edge = edge->next)
    {
    if (edge->in)
	{
	level++;
	if (level > maxLevel)
	    maxLevel = level;

	// if the current level is as high as what's on deck, raise the roof!
	if ((onDeck != NULL) && (level > onDeck->level))
	    {
	    struct adjacency *adj = onDeck;
	    for(; adj; adj = adj->next)
		adj->level++;
	    }

	// add this adjacency to the "on-deck" list
	slAddHead(&onDeck, edge->adj);
	}
    else
	{
	level--;

	// we have to remove this adjacency from onDeck
	onDeck = removeAdjacency(onDeck, edge->adj);
	}
    }
//printf("maxLevel %d level %d\n",maxLevel, level);

// rebuild adjacency list
//printf("Edge Count %d\n",slCount(edgeList));
struct adjacency *newList = NULL;
for(edge = edgeList ; edge; edge = edge->next)
    {
    if (edge->in)
	slAddHead(&newList, edge->adj);
    }
newList = slCat(newList, segList);
adjInfo->list = newList;

//printf("Count %d\n",slCount(newList));
//adj = newList;
//for(; adj; adj = adj->next)
//    {
//    printf("adj %d %d level %d\n", adj->chromStart, adj->chromEnd, adj->level);
//    }

//adjInfo->height = maxLevel;
adjInfo->height =  (6 + maxLevel)  * heightPer;
adjInfo->calced = TRUE;
#endif
}

int adjacencyHeight(struct track *tg, enum trackVisibility vis)
/* calculate height of all the cycles being displayed */
{
int height = 0;

struct adjacencyInfo *adjInfo = tg->items;
for(; adjInfo; adjInfo = adjInfo->next)
    {
    calcAdjacencyLevels(adjInfo, tg->heightPer);
    height +=   adjInfo->height;
    //height +=  (4 + adjInfo->height)  * tg->heightPer;
    //printf("height %d\n", height);
    }
return height;
    
#ifdef NOTNOW
switch (tg->visibility  )
    {
    case tvDense:
	return (4 + adjInfo->height)  * tg->heightPer;
    case tvFull:
	{
	struct adjacency *adj = adjInfo->list;
	int height = 0;
	for(adj = adjInfo->list; adj; adj = adj->next)
	    height += (adj->level + 2) * tg->heightPer;
	return height;
	}
    default:
	return 0;
    }
#endif
}

void adjacencyDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw simple Bed items. */
{
//struct slList *item;
int y = yOff;
double scale = scaleForWindow(width, seqStart, seqEnd);
int height = adjacencyHeight(tg, vis) +1;

hvGfxSetClip(hvg, xOff, yOff, width, height);


#ifdef NOTNOW
switch (tg->visibility  )
    {
    case tvDense:
	y = yOff + (adjInfo->height + 3) * tg->heightPer;
	break;
    case tvFull:
	y = yOff + (adj->level + 2) * tg->heightPer;
	break;
    default:
	y = yOff;
	break;
    }
#endif

struct adjacencyInfo *adjInfo = tg->items;
for(; adjInfo; adjInfo = adjInfo->next)
    {
    struct adjacency *adj = adjInfo->list;
    y += adjInfo->height;
    for(adj = adjInfo->list; adj; adj = adj->next)
	{
    //printf("y %d\n", y);
	unsigned color = makeColor(adj->score);
	tg->drawItemAt(tg, adj, hvg, xOff, y, scale, font, color, vis);
	} 
    //y += adjInfo->height;
    }
}


int adjacencyItemHeight(struct track *tg, void *item)
{
struct adjacencyInfo *adjInfo = item;
switch (tg->visibility  )
    {
    case tvDense:
	return 0;
    case tvFull:
	return adjInfo->height;
	//return (2 + 2) * tg->heightPer;
	//return (adj->level + 2) * tg->heightPer;
    default:
	return 0;

    }
}

void adjacencyLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
{
struct adjacencyInfo *adjInfo = tg->items;
int y = yOff;
for(; adjInfo; adjInfo = adjInfo->next)
    {
    y += adjInfo->height / 2;
    //struct adjacency *adj = adjInfo->list;
    hvGfxTextRight(hvg, xOff, y, width - 1,
	tg->heightPer, color, font,adjInfo->list->cycle);
    y += adjInfo->height / 2;
    }
}

static void adjacencyMapItem(struct track *tg, struct hvGfx *hvg, void *item,
				char *itemName, char *mapItemName, int start, int end,
				int x, int y, int width, int height)
/* Special mouseover text from item->label. (derived from genericMapItem) */
{
}

void adjacencyMethods(struct track *track)
{
bedMethods(track);
track->drawItems = adjacencyDraw;
track->drawItemAt = adjacencyDrawAt;
track->totalHeight = adjacencyHeight; 
track->itemHeight = adjacencyItemHeight;
track->drawLeftLabels = adjacencyLeftLabels;
track->mapItem = adjacencyMapItem;

// Disinherit next/prev flag and methods since we don't support next/prev:
track->nextExonButtonable = FALSE;
track->nextPrevExon = NULL;
track->nextPrevItem = NULL;
track->loadItems = adjacencyLoadItems;
track->canPack = TRUE;
}

