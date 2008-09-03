/* cytoBandTrack - also known as chromosome band track. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "hCommon.h"
#include "cytoBand.h"
#include "hCytoBand.h"

static Color cytoBandItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Figure out color of band. */
{
return hCytoBandColor(item, hvg, hCytoBandDbIsDmel(database),
	tg->ixColor, tg->ixAltColor, shadesOfGray, maxShade);
}

static char *cytoBandItemName(struct track *tg, void *item)
/* Return name of cytoBand track item. */
{
return hCytoBandName(item, hCytoBandDbIsDmel(database));
}

int cytoBandStart(struct track *tg, void *item)
/* Return the start position of a cytoBand. */
{
return ((struct cytoBand *)item)->chromStart;
}

int cytoBandEnd(struct track *tg, void *item)
/* Return the end position of a cytoBand. */
{
return ((struct cytoBand *)item)->chromEnd;
}

int cytoBandIdeoStart(struct track *tg, void *item)
/* Return the start position of a cytoBand. */
{
return ((struct cytoBand *)item)->chromStart + winStart;
}

int cytoBandIdeoEnd(struct track *tg, void *item)
/* Return the end position of a cytoBand. */
{
return ((struct cytoBand *)item)->chromEnd + winStart;
}

static void cytoBandDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw cytoBand items. */
{
struct cytoBand *band = item;
int heightPer = tg->heightPer;
int x1,x2,w;

x1 = round((double)((int)tg->itemStart(tg,band) - winStart)*scale) + xOff;
x2 = round((double)((int)tg->itemEnd(tg,band) - winStart)*scale) + xOff;
/* Clip here so that text will tend to be more visible... */
if (x1 < xOff)
    x1 = xOff;
if (x2 > insideX + insideWidth)
    x2 = insideX + insideWidth;
w = x2-x1;
if (w < 1)
    w = 1;

hCytoBandDrawAt(band, hvg, x1, y, w, heightPer, hCytoBandDbIsDmel(database), font, 
	mgFontPixelHeight(font), tg->ixColor, tg->ixAltColor,
	shadesOfGray, maxShade);

if(tg->mapsSelf)
    tg->mapItem(tg, hvg, band, band->name, band->name, band->chromStart, band->chromEnd,
		x1, y, w, heightPer);
else
    mapBoxHc(hvg, band->chromStart, band->chromEnd, x1,y,w,heightPer, tg->mapName, 
	     band->name, band->name);
}


static void loadCytoBands(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, "cytoBand", (ItemLoader)cytoBandLoad);
}

static void loadCytoBandsIdeo(struct track *tg)
/* Load up cytoBandIdeo from database table to track items. */
{
char query[256];
safef(query, sizeof(query), 
      "select * from cytoBandIdeo where chrom like '%s'", chromName);
if(hTableExists(database, "cytoBandIdeo"))
    bedLoadItemByQuery(tg, "cytoBandIdeo", query, (ItemLoader)cytoBandLoad);
if(slCount(tg->items) == 0)
    {
    tg->limitedVisSet = TRUE;
    tg->limitedVis = tvHide;
    }
}

static void freeCytoBands(struct track *tg)
/* Free up isochore items. */
{
cytoBandFreeList((struct cytoBand**)&tg->items);
}

void cytoBandIdeoDraw(struct track *tg, 
		      int seqStart, int seqEnd,
		      struct hvGfx *hvg, int xOff, int yOff, int width, 
		      MgFont *font, Color color, enum trackVisibility vis)
/* Draw the entire chromosome with a little red box around our
   current position. */
{
double scale = 0; 
int xBorder = 4;
int lineHeight = 0;
int heightPer = 0;
int x1, x2;
int yBorder = 0;
int chromSize = hChromSize(database, chromName);
struct cytoBand *cbList = NULL, *cb = NULL;
scale = (double) (width - (2 * xBorder)) / chromSize;

/* Subtrack 10 for the 5 pixels buffer on either side. */
tg->heightPer -= 11;
tg->lineHeight -= 11;
lineHeight = tg->lineHeight;
heightPer = tg->heightPer;
yOff = yOff;

/* Time to draw the bands. */
hvGfxSetClip(hvg, xOff, yOff, insideWidth, tg->height); 
genericDrawItems(tg, 0, chromSize, hvg, xOff+xBorder, yOff+5, width-(2*xBorder), font, color, tvDense);

x1 = round((winStart)*scale) + xOff + xBorder -1;
x2 = round((winEnd)*scale) + xOff + xBorder -1;
if(x1 >= x2)
    x2 = x1+1;
yBorder = tg->heightPer + 7 + 1;


/* Draw an outline around chromosome for visualization purposes. Helps
 to make the chromosome look better. */
hvGfxLine(hvg, xOff+xBorder, yOff+4, xOff+width-xBorder, yOff+4, MG_BLACK);
hvGfxLine(hvg, xOff+xBorder, yOff+yBorder-3, xOff+width-xBorder, yOff+yBorder-3, MG_BLACK);
hvGfxLine(hvg, xOff+xBorder, yOff+4, xOff+xBorder, yOff+yBorder-3, MG_BLACK);
hvGfxLine(hvg, xOff+width-xBorder, yOff+4, xOff+width-xBorder, yOff+yBorder-3, MG_BLACK);

/* Find and draw the centromere which is defined as the
 two bands with gieStain "acen" */
cbList = tg->items;
for(cb = cbList; cb != NULL; cb = cb->next)
    {
    /* If centromere do some drawing. */
    if(sameString(cb->gieStain, "acen"))
	{
	int cenLeft, cenRight, cenTop, cenBottom;
	
	/* Get the coordinates of the edges of the centromere. */
	cenLeft = round((cb->chromStart)*scale) + xOff + xBorder;
	cenRight = round((cb->next->chromEnd)*scale) + xOff + xBorder;
	cenTop = yOff+4;
	cenBottom = yOff + yBorder - 3;

	/* Draw centromere itself. */
	hCytoBandDrawCentromere(hvg, cenLeft, cenTop, cenRight - cenLeft, 
	     cenBottom-cenTop+1, MG_WHITE, hCytoBandCentromereColor(hvg));
	break;
	}
    }

/* Draw a red box around position in current browser window.  double
 thick so two lines each. */
hvGfxBox(hvg, x1, yOff+1, x2-x1, 2, MG_RED);
hvGfxBox(hvg, x1, yOff + yBorder - 1, x2-x1, 2, MG_RED);
hvGfxBox(hvg, x1, yOff+1, 2, yBorder, MG_RED);
hvGfxBox(hvg, x2, yOff+1, 2, yBorder, MG_RED);
hvGfxUnclip(hvg);

/* Put back the lineHeight for the track
   now that we are done spoofing tgDrawItems(). */
tg->heightPer += 11;
tg->lineHeight += 11;
}

void cytoBandIdeoMapItem(struct track *tg, struct hvGfx *hvg, void *item, 
			    char *itemName, char *mapItemName, int start, int end, 
			    int x, int y, int width, int height)
/* Print out a box to jump to band in browser window .*/
{
struct cytoBand *cb = item;
mapBoxJumpTo(hvg, x, y, width, height, cb->chrom, cb->chromStart, cb->chromEnd, cb->name);
}

int cytoBandIdeoTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return the (nonstandard) height of the cytoBandIdeo track. */
{
tg->heightPer = tl.fontHeight + 11;
tg->lineHeight = tl.fontHeight + 11;
tg->limitedVisSet = TRUE;
if(vis != tvHide && slCount(tg->items) > 0)
    {
    tg->limitedVis = tvDense;
    tg->height = tg->lineHeight;
    }
else
    {
    tg->limitedVis = tvHide;
    tg->height = 0;
    }
return tg->height;
}

void cytoBandIdeoMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->totalHeight = cytoBandIdeoTotalHeight;
tg->mapItem = cytoBandIdeoMapItem;
tg->mapsSelf = TRUE;
tg->itemStart = cytoBandIdeoStart;
tg->itemEnd = cytoBandIdeoEnd;
tg->loadItems = loadCytoBandsIdeo;
tg->freeItems = freeCytoBands;
tg->drawItems = cytoBandIdeoDraw;
tg->drawItemAt = cytoBandDrawAt;
tg->itemColor = cytoBandItemColor;
tg->itemName = cytoBandItemName;
tg->drawName = TRUE;
}

void cytoBandMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->itemStart = cytoBandStart;
tg->itemEnd = cytoBandEnd;
tg->loadItems = loadCytoBands;
tg->freeItems = freeCytoBands;
tg->drawItems = genericDrawItems;
tg->drawItemAt = cytoBandDrawAt;
tg->itemColor = cytoBandItemColor;
tg->itemName = cytoBandItemName;
tg->drawName = TRUE;
}



