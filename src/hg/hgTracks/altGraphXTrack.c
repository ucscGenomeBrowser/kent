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
    return altGraphXMaxRows;
/* Have to pretend we have a wider screen to do all the exons,
   even if they aren't visable still want to have links to them. */
extendedWidth = (maxEnd - minStart) * scale;
winStartOffset = max(0,winStart - minStart);
altGraphXLayout(tg->items, winStart, winEnd, scale, maxRows,
		&ssList, &heightHash, &rowCount);
tg->ss = ssList;
tg->customPt = heightHash;
return rowCount;
}

void altGraphXMapItem(struct track *tg, struct hvGfx *hvg, void *item, char *itemName, char *mapItemName, int start, int end, 
		    int x, int y, int width, int height)
/* Create a link to hgc for altGraphX track */
{
struct altGraphX *ag = item;
char buff[32];
snprintf(buff, sizeof(buff), "%d", ag->id);
mapBoxHc(hvg, start, end, x, y, width, height, tg->mapName, buff, "altGraphX Details");
}

void altGraphXMap(char *tableName, struct altGraphX *ag, struct hvGfx *hvg, int start, int end, 
		  int x, int y, int width, int height)
/* Create a link to hgc for altGraphX track without using a track
 * structure. */
{
char buff[32];
snprintf(buff, sizeof(buff), "%d", ag->id);
mapBoxHc(hvg, start, end, x, y, width, height, tableName, buff, "altGraphX Details");
}

static void altGraphXDrawPackTrack(struct track *tg, int seqStart, int seqEnd,         
			 struct hvGfx *hvg, int xOff, int yOff, int width, 
			 MgFont *font, Color color, enum trackVisibility vis)
/* Draws the blocks for an alt-spliced gene and the connections */
{
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
double scale = scaleForPixels(width);
if(vis == tvFull) 
    {
    hvGfxSetClip(hvg, insideX, yOff, insideWidth, tg->height);
    altGraphXDrawPack(tg->items, tg->ss, hvg, xOff, yOff, width, heightPer, lineHeight,
		      winStart, winEnd, scale, font, color, shadesOfGray,
		      tg->mapName, altGraphXMap);
    hvGfxUnclip(hvg);
    }
}



Color altGraphXColorForEdge(struct hvGfx *hvg, struct altGraphX *ag, int eIx)
/* Return the color of an edge given by confidence */
{
int confidence = altGraphConfidenceForEdge(ag, eIx);
Color c = shadesOfGray[maxShade/4];
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
/* 	makeRedGreenShades(bg); */
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

static boolean isExon(struct altGraphX *ag, int edge)
/* Is this edge an exon. */
{
boolean exon = FALSE;
if( (ag->vTypes[ag->edgeStarts[edge]] == ggHardStart || ag->vTypes[ag->edgeStarts[edge]] == ggSoftStart)  
    && (ag->vTypes[ag->edgeEnds[edge]] == ggHardEnd || ag->vTypes[ag->edgeEnds[edge]] == ggSoftEnd))
    exon = TRUE;
return exon;
}

static void altGraphXDrawAt(struct track *tg, void *item, struct hvGfx *hvg, 
			    int xOff, int yOff, double scale, 
			    MgFont *font, Color color, enum trackVisibility vis)
/* Draw an altGraphX at the specified location. */
{
int i = 0;
int s =0, e=0;
int heightPer = tg->heightPer;
int start = 0, end = 0;
struct altGraphX *ag = item;
int width = 0;
int x1, x2;

/* Create a link to hgc. */
if(tg->mapsSelf && tg->mapItem)
    {
    char name[256];
    int nameWidth = 0;
    int textX = 0;
    start = max(winStart, ag->tStart);
    end = min(winEnd, ag->tEnd);
    width = (end - start) * scale;
    x1 = round((double)((int) start - winStart)*scale) + xOff;
    textX = x1;
    if(width == 0)
	width = 1;
    /* If there isn't enough room on before the left edge snap the
       label to the left edge. */
    if(withLeftLabels && tg->limitedVis == tvPack)
	{
	safef(name, sizeof(name), "%s", tg->itemName(tg, ag));
	nameWidth = mgFontStringWidth(font, name);
	textX = textX - (nameWidth + tl.nWidth/2);
	if(textX < insideX)
	    textX = insideX - nameWidth;
	width = width + (x1 - textX);
	}
    tg->mapItem(tg, hvg, ag, "notUsed", "notUsed", ag->tStart, ag->tEnd, textX, yOff, width, heightPer);
    }

/* Draw the edges (exons and introns). */
for(i= 0; i <  ag->edgeCount; i++)
    {
    Color color2;
    s = ag->vPositions[ag->edgeStarts[i]];
    e = ag->vPositions[ag->edgeEnds[i]];
    color2 = MG_BLACK;
/*  If you want to shade by number of transcripts uncomment next line. */
/* 	color2 = altGraphXColorForEdge(hvg, ag, i); */
    if(isExon(ag, i))
	{
	if(vis == tvPack)
	    drawScaledBox(hvg, s, e, scale, xOff, yOff+heightPer/2, heightPer/2, color2);
	else
	    drawScaledBox(hvg, s, e, scale, xOff, yOff, heightPer, color2);
	}
    else 
	{
	int midX;   
	s = ag->vPositions[ag->edgeStarts[i]];
	e = ag->vPositions[ag->edgeEnds[i]];
	x1 = round((double)((int) s - winStart)*scale) + xOff;
	x2 = round((double)((int) e - winStart)*scale) + xOff;
	if(vis == tvPack)
	    {
	    midX = (x1+x2)/2;
	    hvGfxLine(hvg, x1, yOff+heightPer/2, midX, yOff, color2);
	    hvGfxLine(hvg, midX, yOff, x2, yOff+heightPer/2, color2);
	    }
	else
	    hvGfxLine(hvg, x1, yOff+heightPer/2, x2, yOff+heightPer/2, color2);
	}
    }
}

void altGraphXLoadItems(struct track *tg)
/* Load the altGraphX data to a track. */
{
struct sqlConnection *conn = hAllocConn(database);
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

static int altGraphXFixedTotalHeight(struct track *tg, enum trackVisibility vis)
/* Used to calculate total height when in pack or squish modes. */
{
if(vis == tvPack)
    return tgFixedTotalHeightOptionalOverflow(tg,vis, tg->lineHeight, tg->heightPer, FALSE);
else
    return tgFixedTotalHeightNoOverflow(tg,vis);
}

static int altGraphXCalcHeight(struct track *tg, enum trackVisibility vis)
/* Calculate the height of the track. Have to determine if we can
 * display things in elaborate with altGraphXDrawPackTrack() or just
 * plain old altGraphXDraw() and switch some function pointers around.
 */
{
int rows = 0;
boolean tooMany = FALSE;

if(vis == tvFull)
    {
    tg->lineHeight  = 2 * tl.fontHeight;
    tg->heightPer = tg->lineHeight -1;
    /* Try to layout the track in ultra full mode. */
    rows = altGraphXLayoutTrack(tg, 3*maxItemsInFullTrack+1);
    tg->height = rows * tg->lineHeight;
    /* If there are too many rows, spoof like we were
       called in pack mode. */
    if(rows >= altGraphXMaxRows) 
	{
	tooMany = TRUE;
	tg->visibility = tvPack;
	}
    }
if(tooMany || vis != tvFull)
    {
    tg->lineHeight  = 2 * tl.fontHeight;
    tg->heightPer = tg->lineHeight -1;
    tg->drawItemAt = altGraphXDrawAt;
    tg->drawItems = genericDrawItems;
    tg->totalHeight = altGraphXFixedTotalHeight;
    tg->itemHeight = tgFixedItemHeight;
    limitVisibility(tg);
    }
/* If we're pretending to be in pack mode, remember
   that we're really in full. */
if(tooMany)
    tg->visibility = tvFull;
return tg->height;
}

static void altGraphXLoadItemsPack(struct track *tg)
/* load the altGraphX data to a track and caclulate full height. */
{
struct sqlConnection *conn = hAllocConn(database);
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
altGraphXCalcHeight(tg, tg->visibility);
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
    safef(key, sizeof(key), "%d", slIxFromElement(tg->items, item));
    return (hashIntVal((struct hash*)tg->customPt, key)) * tg->lineHeight;
    }
else
    return tg->heightPer;
}

void altGraphXMethods(struct track *tg)
/* setup special methods for altGraphX track */
{
bedMethods(tg);
tg->drawItems = altGraphXDrawPackTrack;
tg->loadItems = altGraphXLoadItemsPack;
tg->freeItems = altGraphXFreeItems;
tg->itemHeight = altGraphXItemHeight;
tg->totalHeight = altGraphXCalcHeight;
tg->itemName = altGraphXItemName;
tg->mapItem = altGraphXMapItem;
}
