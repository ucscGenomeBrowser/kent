/* altGraphXTrack - Functions to display altGraphX tracks in browser.
   altGraphX is used to display alternative splicing in browser. */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "altGraphX.h"
#include "geneGraph.h"
#include "spaceSaver.h"

const static int altGraphXMaxRows = 20;

boolean altGraphXInEdges(struct ggEdge *edges, int v1, int v2)
/* Return TRUE if a v1-v2 edge is in the list FALSE otherwise. */
{
struct ggEdge *e = NULL;
for(e = edges; e != NULL; e = e->next)
    {
    if(e->vertex1 == v1 && e->vertex2 == v2)
	return TRUE;
    }
return FALSE;
}

int altGraphXLayoutTrack(struct track *tg, int maxRows)
/* Calculate the number of rows necessary to display the altGraphX
   items in this track. The full layout is expesive computationally
   and ugly visually if you zoom too far out, return altGraphXMaxRows
   to indicate switch to more dense mode if necessary. */
{
struct altGraphX *ag = NULL;
struct spaceSaver *ssList = NULL;
struct hash *heightHash = NULL;
int maxEnd = 0, maxDiff = 0, minStart = BIGNUM;
int winStartOffset = 0;
int extendedWidth = 0;
int rowCount = 0;
double scale = (double)insideWidth/(winEnd - winStart);
spaceSaverFree(&tg->ss);
for(ag = tg->items; ag != NULL; ag = ag->next)
    {
    maxDiff = max(maxDiff, (ag->tEnd-ag->tStart));
    minStart = min(minStart, ag->tStart);
    maxEnd = max(maxEnd, ag->tEnd);
    }
if(maxDiff*scale < .3*insideWidth)
    {
    tg->limitedVis = tvDense;
    return altGraphXMaxRows;
    }
/* Have to pretend we have a wider screen to do all the exons,
   even if they aren't visable still want to have links to them. */
extendedWidth = (maxEnd - minStart) * scale;
winStartOffset = max(0,winStart - minStart);
altGraphXLayout(tg->items, minStart, extendedWidth, extendedWidth, scale, maxRows,
		&ssList, &heightHash, &rowCount);
tg->ss = ssList;
tg->customPt = heightHash;
return rowCount;
}

void altGraphXMapItem(struct track *tg, void *item, char *itemName, int start, int end, 
		    int x, int y, int width, int height)
/* Create a link to hgc for altGraphX track */
{
struct altGraphX *ag = item;
char buff[32];
snprintf(buff, sizeof(buff), "%d", ag->id);
mapBoxHc(start, end, x, y, width, height, tg->mapName, buff, "altGraphX Details");
}

void altGraphXMap(char *tableName, struct altGraphX *ag, int start, int end, 
		  int x, int y, int width, int height)
/* Create a link to hgc for altGraphX track without using a track
 * structure. */
{
char buff[32];
snprintf(buff, sizeof(buff), "%d", ag->id);
mapBoxHc(start, end, x, y, width, height, tableName, buff, "altGraphX Details");
}

static void altGraphXDrawPackTrack(struct track *tg, int seqStart, int seqEnd,         
			 struct vGfx *vg, int xOff, int yOff, int width, 
			 MgFont *font, Color color, enum trackVisibility vis)
/* Draws the blocks for an alt-spliced gene and the connections */
{
int baseWidth = seqEnd - seqStart;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
boolean isFull = (vis == tvFull);
double scale = scaleForPixels(width);
int maxLevels = 0;
if(vis == tvFull) 
    {
    vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
    altGraphXDrawPack(tg->items, tg->ss, xOff, yOff, width, heightPer, lineHeight,
		      winStart, winEnd, scale, baseWidth, vg, font, color, shadesOfGray,
		      tg->mapName, altGraphXMap);
    vgUnclip(vg);
    }
}



Color altGraphXColorForEdge(struct vGfx *vg, struct altGraphX *ag, int eIx)
/* Return the color of an edge given by confidence */
{
int confidence = altGraphConfidenceForEdge(ag, eIx);
Color c = shadesOfGray[maxShade/4];
struct geneGraph *gg = NULL;
struct ggEdge *edges = NULL;
if(confidence == 1) 
    c = shadesOfGray[maxShade/3];
else if(confidence == 2) 
    c = shadesOfGray[2*maxShade/3];
else
    c = shadesOfGray[maxShade];
/* Currently we're ignoring cassette exons or other interesting topology. */
/* if(ag->edgeTypes[eIx] == ggCassette) */
/*     { */
/*     if(!exprBedColorsMade) */
/* 	makeRedGreenShades(vg); */
/*     if(confidence == 1) c = shadesOfRed[(maxRGBShade - 6 > 0) ? maxRGBShade - 6 : 0]; */
/*     else if(confidence == 2) c = shadesOfRed[(maxRGBShade - 4 > 0) ? maxRGBShade - 4: 0]; */
/*     else if(confidence >= 3) c = shadesOfRed[(maxRGBShade - 4 > 0) ? maxRGBShade - 1: 0]; */
/*     } */
/* else */
/*     { */
/*     if(confidence == 1) c = shadesOfGray[maxShade/3]; */
/*     else if(confidence == 2) c = shadesOfGray[2*maxShade/3]; */
/*     else if(confidence >= 3) c = shadesOfGray[maxShade]; */
/*     } */
return c;
}

static void altGraphXDraw(struct track *tg, int seqStart, int seqEnd,         
			 struct vGfx *vg, int xOff, int yOff, int width, 
			 MgFont *font, Color color, enum trackVisibility vis)
/* Draws the blocks for an alt-spliced gene and the connections. This
 * is more compact for both the dense an full modes than
 * altGraphXDrawPackTrack() but will overwrite on exon on top of
 * another so you may not see alternative 5' splice sites. If we're in close we
 * use altGraphXDrawPackTrack() otherwise we default to this. */
{
int baseWidth = seqEnd - seqStart;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2;
boolean isFull = (vis == tvFull);
double scale = scaleForPixels(width);
int i;
double y1, y2;
int midLineOff = heightPer/2;
struct altGraphX *ag=NULL, *agList = NULL;
int s =0, e=0;
agList = tg->items;
for(ag = agList; ag != NULL; ag = ag->next)
    {	   
    x1 = round((double)((int)ag->tStart-winStart)*scale) + xOff;
    x2 = round((double)((int)ag->tEnd-winStart)*scale) + xOff;
    if(tg->mapsSelf && tg->mapItem)
	{
	tg->mapItem(tg, ag, "notUsed", ag->tStart, ag->tEnd, xOff, y, width, heightPer);
	}
    if(!isFull && (x2-x1 > 0))
	{
	vgBox(vg, x1, yOff+tg->heightPer/2, x2-x1, 1, MG_BLACK);
	}
    for(i= ag->edgeCount -1; i >= 0; i--)   // for(i=0; i< ag->edgeCount; i++)
	{
	char buff[16];
	int textWidth;
	int sx1 = 0;
	int sx2 = 0;
	int sw = 0;
	s = ag->vPositions[ag->edgeStarts[i]];
	e = ag->vPositions[ag->edgeEnds[i]];
	sx1 = roundingScale(s-winStart, width, baseWidth)+xOff;
	sx2 = roundingScale(e-winStart, width, baseWidth)+xOff;
	sw = sx2 - sx1;
	snprintf(buff, sizeof(buff), "%d-%d", ag->edgeStarts[i], ag->edgeEnds[i]);        /* draw exons as boxes */
	if( (ag->vTypes[ag->edgeStarts[i]] == ggHardStart || ag->vTypes[ag->edgeStarts[i]] == ggSoftStart)  
	    && (ag->vTypes[ag->edgeEnds[i]] == ggHardEnd || ag->vTypes[ag->edgeEnds[i]] == ggSoftEnd)) 
	    {
	    Color color2;
	    if(sameString(tg->mapName, "altGraphXCon"))
		color2 = MG_BLACK;
	    else
		color2 = altGraphXColorForEdge(vg, ag, i);
	    if(isFull)
		{
		drawScaledBox(vg, s, e, scale, xOff, y+(heightPer/2), heightPer/2, color2);
		textWidth = mgFontStringWidth(font, buff);
		if (textWidth <= sw + 2 && 0 )
		    vgTextCentered(vg, sx2-textWidth-2, y+(heightPer/2), textWidth+2, heightPer/2, MG_WHITE, font, buff);
		}
	    else
		drawScaledBox(vg, s, e, scale, xOff, y, heightPer, color2);

	    }
	if(isFull)
	    {
	    /* draw introns as arcs */

	    if( (ag->vTypes[ag->edgeStarts[i]] == ggHardEnd || ag->vTypes[ag->edgeStarts[i]] == ggSoftEnd) 
		&& (ag->vTypes[ag->edgeEnds[i]] == ggHardStart || ag->vTypes[ag->edgeEnds[i]] == ggSoftStart))
		{
		Color color2;
		int x1, x2;
		int midX;   
		int midY = y + heightPer/2;
		if(sameString(tg->mapName, "altGraphXCon"))
		    color2 = MG_BLACK;
		else
		    color2 = altGraphXColorForEdge(vg, ag, i);
		s = ag->vPositions[ag->edgeStarts[i]];
		e = ag->vPositions[ag->edgeEnds[i]];
		x1 = round((double)((int) s - winStart)*scale) + xOff;
		x2 = round((double)((int) e - winStart)*scale) + xOff;
		midX = (x1+x2)/2;
		vgLine(vg, x1, midY, midX, y, color2);
		vgLine(vg, midX, y, x2, midY, color2);
		textWidth = mgFontStringWidth(font, buff);
		if (textWidth <= sw && 0 )
		    vgTextCentered(vg, sx1, y+(heightPer/2), sw, heightPer/2, MG_BLACK, font, buff);
		}
	    }
	}
    if(isFull)
	y += lineHeight;
    }
}


void altGraphXLoadItems(struct track *tg)
/* Load the altGraphX data to a track. */
{
struct sqlConnection *conn = hAllocConn();
int rowOffSet;
char **row;
struct altGraphX *ag=NULL, *agList=NULL;
struct sqlResult *sr = hRangeQuery(conn, tg->mapName, chromName,
				   winStart, winEnd, NULL, &rowOffSet);
while((row = sqlNextRow(sr)) != NULL)
    {
    ag = altGraphXLoad(row + rowOffSet);
    slAddHead(&agList, ag);
    }
slReverse(&agList);
sqlFreeResult(&sr);
tg->items = agList;
}

void altGraphXFreeItems(struct track *tg)
/* Free up tha altGraphX items in tg->items. */
{
altGraphXFreeList((struct altGraphX**)(&tg->items));
}

static int altGraphXCalcHeight(struct track *tg, enum trackVisibility vis)
/* Calculate the height of the track. Have to determine if we can
 * display things in elaborate with altGraphXDrawPackTrack() or just
 * plain old altGraphXDraw() and switch some function pointers around.
 */
{
int rows = 0;
switch (vis)
    {
    case tvFull:
	tg->lineHeight  = 2 * tl.fontHeight+1;
	tg->heightPer = tg->lineHeight -1;
	/* Try to layout the track in ultra full mode. */
	rows = altGraphXLayoutTrack(tg, 3*maxItemsInFullTrack+1);
 	if(rows >= altGraphXMaxRows) 
	    {
	    struct hash *tmp = NULL;
	    rows = slCount(tg->items);
	    tg->limitedVis = tvDense;
	    tg->drawItems = altGraphXDraw;
	    tmp = tg->customPt;
	    freeHash(&tmp);
	    tg->customPt = NULL;
	    }
	break;
    case tvDense:
	tg->lineHeight  = tl.fontHeight+1;
	tg->heightPer = tg->lineHeight -1;
	tg->drawItems = altGraphXDraw;
	rows=1;
	break;
    }

tg->height = rows * tg->lineHeight;
return tg->height;
}

static void altGraphXLoadItemsPack(struct track *tg)
/* load the altGraphX data to a track and caclulate full height. */
{
struct sqlConnection *conn = hAllocConn();
int rowOffSet;
char **row;
struct altGraphX *ag=NULL, *agList=NULL;
struct sqlResult *sr = hRangeQuery(conn, tg->mapName, chromName,
				   winStart, winEnd, NULL, &rowOffSet);
while((row = sqlNextRow(sr)) != NULL)
    {
    ag = altGraphXLoad(row + rowOffSet);
    slAddHead(&agList, ag);
    }
slReverse(&agList);
tg->items = agList;
altGraphXCalcHeight(tg, tg->limitedVis);
sqlFreeResult(&sr);
tg->items = agList;
}


char *altGraphXNumCassette(struct track *tg, void *item)
/* returns the number of cassette exons as a string name */
{
char buff[32];
struct altGraphX *ag = item;
int count =0, i=0;
for(i=0; i<ag->edgeCount; i++)
    if(ag->edgeTypes[i] == ggExon)
	count++;
snprintf(buff, sizeof(buff), "%d", count );
return (cloneString(buff));
}

char *altGraphXItemName(struct track *tg, void *item)
/* Return the name of the altGraphX. */
{
return cloneString(((struct altGraphX *)item)->name);
}

int altGraphXItemHeight(struct track *tg, void *item)
/* Return how high an item is. If we're using altGraphXDrawPackTrack()
 * we have to look up how many rows an item takes in the associated
 * hash, otherwise it is just the heightPer. */
{
if(tg->limitedVis == tvDense || tg->customPt == NULL)
    return tg->lineHeight;
else if(tg->limitedVis == tvFull)
    {
    char key[128];
    struct spliceEdge *se = NULL;
    safef(key, sizeof(key), "%d", slIxFromElement(tg->items, item));
    se = hashMustFindVal((struct hash*)tg->customPt, key);
    return ((se->row+1) * tg->lineHeight);
    }
else
    return tg->heightPer;
}
	  

void altGraphXMethods(struct track *tg)
/* setup special methods for altGraphX track */
{
tg->drawItems = altGraphXDrawPackTrack;
tg->loadItems = altGraphXLoadItemsPack;
tg->freeItems = altGraphXFreeItems;
tg->itemHeight = altGraphXItemHeight;
tg->totalHeight = altGraphXCalcHeight;
tg->itemName = altGraphXItemName;
tg->mapsSelf = TRUE;
tg->mapItem = altGraphXMapItem;
}
