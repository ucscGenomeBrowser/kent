/* cytoBandTrack - also known as chromosome band track. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "hCommon.h"
#include "cytoBand.h"

#define IS_DMEL (startsWith("dm", hGetDb()))

static char *abbreviatedBandName(struct track *tg, struct cytoBand *band, 
	MgFont *font, int width)
/* Return a string abbreviated enough to fit into space. */
{
int textWidth;
static char string[32];

/* If have enough space, return original chromosome/band. */
sprintf(string, "%s%s", (IS_DMEL ? "" : skipChr(band->chrom)), band->name);
textWidth = mgFontStringWidth(font, string);
if (textWidth <= width)
    return string;

/* Try leaving off chromosome  */
sprintf(string, "%s", band->name);
textWidth = mgFontStringWidth(font, string);
if (textWidth <= width)
    return string;

/* Try leaving off initial letter */
sprintf(string, "%s", band->name+1);
textWidth = mgFontStringWidth(font, string);
if (textWidth <= width)
    return string;

return NULL;
}

static char *cytoBandName(struct track *tg, void *item)
/* Return name of cytoBand track item. */
{
struct cytoBand *band = item;
static char buf[32];

sprintf(buf, "%s%s", (IS_DMEL ? "" : skipChr(band->chrom)), band->name);
return buf;
}


static Color cytoBandColorGiemsa(struct track *tg, void *item, struct vGfx *vg)
/* Figure out color of band based on gieStain field. */
{
struct cytoBand *band = item;
char *stain = band->gieStain;
if (startsWith("gneg", stain))
    {
    return shadesOfGray[1];
    }
else if (startsWith("gpos", stain))
    {
    int percentage = 100;
    stain += 4;	
    if (isdigit(stain[0]))
        percentage = atoi(stain);
    return shadesOfGray[grayInRange(percentage, -30, 100)];
    }
else if (startsWith("gvar", stain))
    {
    return shadesOfGray[maxShade];
    }
else 
    {
    return tg->ixAltColor;
    }
}

static Color cytoBandColorDmel(struct track *tg, void *item, struct vGfx *vg)
/* Figure out color of band based on D. melanogaster band name: just toggle 
 * color based on subband letter before the number at end of name (number 
 * at end of name gives too much resolution!).  There must be a better way 
 * but the only online refs I've been able to get for these bands are books 
 * and too-old-for-online journal articles... */
{
struct cytoBand *band = item;
char *lastAlpha = band->name + strlen(band->name) - 1;
int parity = 0;
while (isdigit(*lastAlpha) && lastAlpha > band->name)
    lastAlpha--;
parity = ((*lastAlpha) - 'A') & 0x1;
if (parity)
    return tg->ixColor;
else 
    return tg->ixAltColor;
}

static Color cytoBandColor(struct track *tg, void *item, struct vGfx *vg)
/* Figure out color of band. */
{
if (IS_DMEL)
    return cytoBandColorDmel(tg, item, vg);
else
    return cytoBandColorGiemsa(tg, item, vg);
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
	struct vGfx *vg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw cytoBand items. */
{
struct cytoBand *band = item;
int heightPer = tg->heightPer;
int x1,x2,w;
Color col, textCol;
char *s;

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
col = cytoBandColor(tg, band, vg);
vgBox(vg, x1, y, w, heightPer, col);
if (vis != tvSquish)
    {
    textCol = contrastingColor(vg, col);
    s = abbreviatedBandName(tg, band, font, w);
    if (s != NULL)
	vgTextCentered(vg, x1, y, w, heightPer, textCol, font, s);
    }
if(tg->mapsSelf)
    tg->mapItem(tg, band, band->name, band->chromStart, band->chromEnd,
		x1, y, w, heightPer);
else
    mapBoxHc(band->chromStart, band->chromEnd, x1,y,w,heightPer, tg->mapName, 
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
		      struct vGfx *vg, int xOff, int yOff, int width, 
		      MgFont *font, Color color, enum trackVisibility vis)
/* Draw the entire chromosome with a little red box around our
   current position. */
{
double scale = 0; 
int xBorder = 4;
int lineHeight = 0;
int heightPer = 0;
int cenLeft = 0, cenMid = 0, cenRight = 0;
int x1, x2;
int yBorder = 0;
int chromSize = hChromSize(chromName);
struct cytoBand *cbList = NULL, *cb = NULL;
scale = (double) (width - (2 * xBorder)) / chromSize;

/* Subtrack 10 for the 5 pixels buffer on either side. */
tg->heightPer -= 11;
tg->lineHeight -= 11;
lineHeight = tg->lineHeight;
heightPer = tg->heightPer;
yOff = yOff;

/* Time to draw the bands. */
vgSetClip(vg, xOff, yOff, insideWidth, tg->height); 
genericDrawItems(tg, 0, chromSize, vg, xOff+xBorder, yOff+5, width-(2*xBorder), font, color, tvDense);

x1 = round((winStart)*scale) + xOff + xBorder -1;
x2 = round((winEnd)*scale) + xOff + xBorder -1;
if(x1 >= x2)
    x2 = x1+1;
yBorder = tg->heightPer + 7 + 1;


/* Draw an outline around chromosome for visualization purposes. Helps
 to make the chromosome look better. */
vgLine(vg, xOff+xBorder, yOff+4, xOff+width-xBorder, yOff+4, MG_BLACK);
vgLine(vg, xOff+xBorder, yOff+yBorder-3, xOff+width-xBorder, yOff+yBorder-3, MG_BLACK);
vgLine(vg, xOff+xBorder, yOff+4, xOff+xBorder, yOff+yBorder-3, MG_BLACK);
vgLine(vg, xOff+width-xBorder, yOff+4, xOff+width-xBorder, yOff+yBorder-3, MG_BLACK);

/* Find and draw the centromere which is defined as the
 two bands with gieStain "acen" */
cbList = tg->items;
for(cb = cbList; cb != NULL; cb = cb->next)
    {
    /* If centromere do some drawing. */
    if(sameString(cb->gieStain, "acen"))
	{
	int offSet;
	double leftSlope = 0, rightSlope = 0;
	int origLeft = 0, origRight = 0;
	Color col;
	
	/* Get the coordinates of the edges and midpoint of the centromere. */
	origLeft = cenLeft = round((cb->chromStart)*scale) + xOff + xBorder;
	cenMid = round((cb->chromEnd)*scale) + xOff + xBorder;
	origRight = cenRight = round((cb->next->chromEnd)*scale) + xOff + xBorder;

	/* Over draw any lettering. */
	col = cytoBandColor(tg, cb, vg);
	vgBox(vg, cenLeft, yOff+5, cenRight-cenLeft, heightPer, col);

	/* Cancel out the black outline. */
	vgLine(vg, cenLeft, yOff+4, cenRight, yOff+4, MG_WHITE);
	vgLine(vg, cenLeft, yOff+yBorder-3, cenRight, yOff+yBorder-3, MG_WHITE);

	/* Get the slope of a line through the midpoint of the 
	   centromere and use consecutively shorter lines as a poor
	   man's polygon fill. */
	leftSlope = (double)(cenMid - cenLeft)/(lineHeight/2);
	rightSlope = (double)(cenRight - cenMid)/(lineHeight/2);
	offSet = 0;
	while(offSet <= lineHeight/2)
	    {
	    vgLine(vg, cenLeft, yOff+4+offSet, cenMid, yOff+4+offSet, MG_WHITE);
	    vgLine(vg, cenLeft, yOff+yBorder-3-offSet, cenMid, yOff+yBorder-3-offSet, MG_WHITE);
	    vgLine(vg, cenMid, yOff+4+offSet, cenRight, yOff+4+offSet, MG_WHITE);
	    vgLine(vg, cenMid, yOff+yBorder-3-offSet, cenRight, yOff+yBorder-3-offSet, MG_WHITE);
	    offSet++;
	    cenLeft = round(offSet*leftSlope) + origLeft;
	    cenRight = origRight - round(offSet*rightSlope);
	    }
	break;
	}
    }
/* Draw a red box around position in current browser window.  double
 thick so two lines each. */
vgBox(vg, x1, yOff+1, x2-x1, 2, MG_RED);
vgBox(vg, x1, yOff + yBorder - 1, x2-x1, 2, MG_RED);
vgBox(vg, x1, yOff+1, 2, yBorder, MG_RED);
vgBox(vg, x2, yOff+1, 2, yBorder, MG_RED);
vgUnclip(vg);

/* Put back the lineHeight for the track
   now that we are done spoofing tgDrawItems(). */
tg->heightPer += 11;
tg->lineHeight += 11;
}

void cytoBandIdeoMapItem(struct track *tg, void *item, 
			    char *itemName, int start, int end, 
			    int x, int y, int width, int height)
/* Print out a box to jump to band in browser window .*/
{
struct cytoBand *cb = item;
mapBoxJumpTo(x, y, width, height, cb->chrom, cb->chromStart, cb->chromEnd, cb->name);
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
tg->itemColor = cytoBandColor;
tg->itemName = cytoBandName;
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
tg->itemColor = cytoBandColor;
tg->itemName = cytoBandName;
tg->drawName = TRUE;
}



