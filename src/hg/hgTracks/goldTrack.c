/* goldTrack - also known as assembly track. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "agpFrag.h"
#include "agpGap.h"

static int cmpAgpFrag(const void *va, const void *vb)
/* Compare two agpFrags by chromStart. */
{
const struct agpFrag *a = *((struct agpFrag **)va);
const struct agpFrag *b = *((struct agpFrag **)vb);
return a->chromStart - b->chromStart;
}

static void goldLoad(struct track *tg)
/* Load up golden path from database table to track items. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
struct agpFrag *fragList = NULL, *frag;
struct agpGap *gapList = NULL, *gap;
int rowOffset;

/* Get the frags and load into tg->items. */
sr = hRangeQuery(conn, "gold", chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    frag = agpFragLoad(row+rowOffset);
    slAddHead(&fragList, frag);
    }
slSort(&fragList, cmpAgpFrag);
sqlFreeResult(&sr);
tg->items = fragList;

/* Get the gaps into tg->customPt. */
sr = hRangeQuery(conn, "gap", chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gap = agpGapLoad(row+rowOffset);
    slAddHead(&gapList, gap);
    }
slReverse(&gapList);
sqlFreeResult(&sr);
tg->customPt = gapList;
hFreeConn(&conn);
}

static void goldFree(struct track *tg)
/* Free up goldTrackGroup items. */
{
agpFragFreeList((struct agpFrag**)&tg->items);
agpGapFreeList((struct agpGap**)&tg->customPt);
}

static char *goldName(struct track *tg, void *item)
/* Return name of gold track item. */
{
struct agpFrag *frag = item;
return frag->frag;
}

static void goldDrawDense(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw golden path items. */
{
int baseWidth = seqEnd - seqStart;
struct agpFrag *frag;
struct agpGap *gap;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
int midLineOff = heightPer/2;
boolean isFull = (vis == tvFull);
Color brown = color;
Color gold = tg->ixAltColor;
Color pink = 0;
Color pink1 = hvGfxFindColorIx(hvg, 240, 140, 140);
Color pink2 = hvGfxFindColorIx(hvg, 240, 100, 100);
int ix = 0;
double scale = scaleForPixels(width);

/* Draw gaps if any. */
if (!isFull)
    {
    int midY = y + midLineOff;
    for (gap = tg->customPt; gap != NULL; gap = gap->next)
	{
	if (!sameWord(gap->bridge, "no"))
	    {
	    drawScaledBox(hvg, gap->chromStart, gap->chromEnd, scale, xOff, midY, 1, brown);
	    }
	}
    }

for (frag = tg->items; frag != NULL; frag = frag->next)
    {
    x1 = round((double)((int)frag->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)frag->chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    color =  ((ix&1) ? gold : brown);
    pink = ((ix&1) ? pink1 : pink2);
    if (w < 1)
	w = 1;
    if (sameString(frag->type, "A")) color = pink;
    hvGfxBox(hvg, x1, y, w, heightPer, color);
    if (isFull)
	y += lineHeight;
    else if (baseWidth < 10000000)
	{
	char status[256];
	sprintf(status, "%s:%d-%d %s %s:%d-%d", 
	    frag->frag, frag->fragStart, frag->fragEnd,
	    frag->strand,
	    frag->chrom, frag->chromStart, frag->chromEnd);

	mapBoxHc(hvg, frag->chromStart, frag->chromEnd, x1,y,w,heightPer, tg->mapName, 
	    frag->frag, status);
	}
    ++ix;
    }
}

static void goldDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw golden path items. */
{
if (vis == tvDense)
    goldDrawDense(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
    	font, color, vis);
else
    genericDrawItems(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
    	font, color, vis);
}

static Color goldColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw known gene in. */
{
struct agpFrag *frag = item;
Color pink = hvGfxFindColorIx(hvg, 240, 140, 140);
Color color = (sameString(frag->type, "A") ? pink : tg->ixColor);

return color;
}

void goldMethods(struct track *tg)
/* Make track for golden path (assembly track). */
{
tg->loadItems = goldLoad;
tg->freeItems = goldFree;
tg->drawItems = goldDraw;
tg->drawItemAt = bedDrawSimpleAt;
tg->itemName = goldName;
tg->mapItemName = goldName;
tg->itemColor = goldColor;
}

