/* hCytoBand - stuff to help draw chromosomes where we have
 * banding data. */

#include "common.h"
#include "hvGfx.h"
#include "hCommon.h"
#include "hgColors.h"
#include "cytoBand.h"
#include "hCytoBand.h"

static char const rcsid[] = "$Id: hCytoBand.c,v 1.6 2008/09/17 18:10:13 kent Exp $";

static char *abbreviatedBandName(struct cytoBand *band, 
	MgFont *font, int width, boolean isDmel)
/* Return a string abbreviated enough to fit into space. */
{
int textWidth;
static char string[32];

/* If have enough space, return original chromosome/band. */
sprintf(string, "%s%s", (isDmel ? "" : skipChr(band->chrom)), band->name);
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

static Color cytoBandColorGiemsa(struct cytoBand *band, 
	struct hvGfx *hvg, Color aColor, Color bColor,
	Color *shades, int maxShade)
/* Figure out color of band based on gieStain field. */
{
char *stain = band->gieStain;
if (startsWith("gneg", stain))
    {
    return shades[1];
    }
else if (startsWith("gpos", stain))
    {
    int percentage = 100;
    stain += 4;	
    if (isdigit(stain[0]))
        percentage = atoi(stain);
    return shades[hGrayInRange(percentage, -30, 100, maxShade)];
    }
else if (startsWith("gvar", stain))
    {
    return shades[maxShade];
    }
else 
    {
    return bColor;
    }
}

static Color cytoBandColorDmel(struct cytoBand *band, 
	struct hvGfx *hvg, Color aColor, Color bColor,
	Color *shades, int maxShade)
/* Figure out color of band based on D. melanogaster band name: just toggle 
 * color based on subband letter before the number at end of name (number 
 * at end of name gives too much resolution!).  There must be a better way 
 * but the only online refs I've been able to get for these bands are books 
 * and too-old-for-online journal articles... */
{
char *lastAlpha = band->name + strlen(band->name) - 1;
int parity = 0;
while (isdigit(*lastAlpha) && lastAlpha > band->name)
    lastAlpha--;
parity = ((*lastAlpha) - 'A') & 0x1;
if (parity)
    return aColor;
else 
    return bColor;
}

Color hCytoBandColor(struct cytoBand *band, struct hvGfx *hvg, boolean isDmel,
	Color aColor, Color bColor, Color *shades, int maxShade)
/* Return appropriate color for band. */
{
if (isDmel)
     return cytoBandColorDmel(band, hvg, aColor, bColor, shades, maxShade);
else
     return cytoBandColorGiemsa(band, hvg, aColor, bColor, shades, maxShade);
}

char *hCytoBandName(struct cytoBand *band, boolean isDmel)
/* Return name of band.  Returns a static buffer, so don't free result. */
{
static char buf[32];
sprintf(buf, "%s%s", (isDmel ? "" : skipChr(band->chrom)), band->name);
return buf;
}

void hCytoBandDrawAt(struct cytoBand *band, struct hvGfx *hvg,
	int x, int y, int width, int height, boolean isDmel,
	MgFont *font, int fontPixelHeight, Color aColor, Color bColor,
	Color *shades, int maxShade)
/* Draw a single band in appropriate color at given position.  If there's
 * room put in band label. */
{
Color col = hCytoBandColor(band, hvg, isDmel, aColor, bColor, shades, maxShade);
hvGfxBox(hvg, x, y, width, height, col);
if (height >= fontPixelHeight+2)
    {
    char *s = abbreviatedBandName(band, font, width, isDmel);
    if (s != NULL)
	{
	Color textCol = hvGfxContrastingColor(hvg, col);
	hvGfxTextCentered(hvg, x, y, width, height, textCol, font, s);
	}
    }
}

Color hCytoBandCentromereColor(struct hvGfx *hvg)
/* Get the color used traditionally to draw centromere */
{
return hvGfxFindColorIx(hvg, 150, 50, 50);
}

void hCytoBandDrawCentromere(struct hvGfx *hvg, int x, int y, 
	int width, int height, Color bgColor, Color fgColor)
/* Draw the centromere. */
{
struct gfxPoly *poly;
int xCen = x+width/2, yCen = y+height/2;
int xEnd = x+width-1, yEnd = y+height-1;

/* Draw box over centromere, may be drawn already with lettering
 * which we don't want. */

hvGfxBox(hvg, x, y, width, height, bgColor);

char *savedHint = hvGfxGetHint(hvg,"fat");
hvGfxSetHint(hvg,"fat","on");

/* Make up triangle on left of centromere and draw. */
poly = gfxPolyNew();
gfxPolyAddPoint(poly, x, y);
gfxPolyAddPoint(poly, xCen, yCen);
gfxPolyAddPoint(poly, x, yEnd);
hvGfxDrawPoly(hvg, poly, fgColor, TRUE);
gfxPolyFree(&poly);

/* Make up triangle on right of centromere and draw. */
poly = gfxPolyNew();
gfxPolyAddPoint(poly, xEnd, y);
gfxPolyAddPoint(poly, xCen, yCen);
gfxPolyAddPoint(poly, xEnd, yEnd);
hvGfxDrawPoly(hvg, poly, fgColor, TRUE);
gfxPolyFree(&poly);

hvGfxSetHint(hvg,"fat",savedHint);
}

	
