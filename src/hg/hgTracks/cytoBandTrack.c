/* cytoBandTrack - also known as chromosome band track. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "hCommon.h"
#include "cytoBand.h"

static char *abbreviatedBandName(struct track *tg, struct cytoBand *band, 
	MgFont *font, int width)
/* Return a string abbreviated enough to fit into space. */
{
int textWidth;
static char string[32];

/* If have enough space, return original chromosome/band. */
sprintf(string, "%s%s", skipChr(band->chrom), band->name);
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
int len;

sprintf(buf, "%s%s", skipChr(band->chrom), band->name);
return buf;
}


static Color cytoBandColor(struct track *tg, void *item, struct vGfx *vg)
/* Figure out color of band. */
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

static void cytoBandDrawAt(struct track *tg, void *item,
	struct vGfx *vg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw cytoBand items. */
{
struct cytoBand *band = item;
int heightPer = tg->heightPer;
int x1,x2,w;
int midLineOff = heightPer/2;
Color col, textCol;
char *s;

x1 = round((double)((int)band->chromStart-winStart)*scale) + xOff;
x2 = round((double)((int)band->chromEnd-winStart)*scale) + xOff;
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
mapBoxHc(band->chromStart, band->chromEnd, x1,y,w,heightPer, tg->mapName, 
    band->name, band->name);
}


static void loadCytoBands(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, "cytoBand", (ItemLoader)cytoBandLoad);
}

static void freeCytoBands(struct track *tg)
/* Free up isochore items. */
{
cytoBandFreeList((struct cytoBand**)&tg->items);
}

void cytoBandMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadCytoBands;
tg->freeItems = freeCytoBands;
tg->drawItems = genericDrawItems;
tg->drawItemAt = cytoBandDrawAt;
tg->itemColor = cytoBandColor;
tg->itemName = cytoBandName;
tg->drawName = TRUE;
}

