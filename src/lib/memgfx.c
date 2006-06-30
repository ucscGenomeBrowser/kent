/* memgfx - routines for drawing on bitmaps in memory.
 * Currently limited to 256 color bitmaps. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "memgfx.h"
#include "gemfont.h"
#include "localmem.h"
#include "vGfx.h"
#include "vGfxPrivate.h"
#include "colHash.h"

static char const rcsid[] = "$Id: memgfx.c,v 1.45 2006/06/30 00:56:06 galt Exp $";

static void mgSetDefaultColorMap(struct memGfx *mg)
/* Set up default color map for a memGfx. */
{
/* Note dependency in order here and in MG_WHITE, MG_BLACK, etc. */
int i;
for (i=0; i<ArraySize(mgFixedColors); ++i)
    {
    struct rgbColor *c = &mgFixedColors[i];
    mgAddColor(mg, c->r, c->g, c->b);
    }
}


void mgSetClip(struct memGfx *mg, int x, int y, int width, int height)
/* Set clipping rectangle. */
{
int x2, y2;
if (x < 0)
    x = 0;
if (y < 0)
    y = 0;
x2 = x + width;
if (x2 > mg->width)
    x2 = mg->width;
y2 = y + height;
if (y2 > mg->height)
    y2 = mg->height;
mg->clipMinX = x;
mg->clipMaxX = x2;
mg->clipMinY = y;
mg->clipMaxY = y2;
}

void mgUnclip(struct memGfx *mg)
/* Set clipping rect cover full thing. */
{
mgSetClip(mg, 0,0,mg->width, mg->height);
}

struct memGfx *mgNew(int width, int height)
/* Return new memGfx. */
{
struct memGfx *mg;

mg = needMem(sizeof(*mg));
mg->pixels = needLargeMem(width*height);
mg->width = width;
mg->height = height;
mg->colorHash = colHashNew();
mgSetDefaultColorMap(mg);
mgUnclip(mg);
mgClearPixels(mg);
return mg;
}

void mgClearPixels(struct memGfx *mg)
/* Set all pixels to background. */
{
zeroBytes(mg->pixels, mg->width*mg->height);
}

Color mgFindColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b)
/* Returns closest color in color map to rgb values.  If it doesn't
 * already exist in color map and there's room, it will create
 * exact color in map. */
{
struct colHashEl *che;
if ((che = colHashLookup(mg->colorHash, r, g, b)) != NULL)
    return che->ix;
if (mgColorsFree(mg))
    return mgAddColor(mg, r, g, b);
return mgClosestColor(mg, r, g, b);
}


struct rgbColor mgColorIxToRgb(struct memGfx *mg, int colorIx)
/* Return rgb value at color index. */
{
return mg->colorMap[colorIx];
}

Color mgClosestColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b)
/* Returns closest color in color map to r,g,b */
{
struct rgbColor *c = mg->colorMap;
int closestDist = 0x7fffffff;
int closestIx = -1;
int dist, dif;
int i;
for (i=0; i<mg->colorsUsed; ++i)
    {
    dif = c->r - r;
    dist = dif*dif;
    dif = c->g - g;
    dist += dif*dif;
    dif = c->b - b;
    dist += dif*dif;
    if (dist < closestDist)
        {
        closestDist = dist;
        closestIx = i;
        }
    ++c;
    }
return closestIx;
}


Color mgAddColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b)
/* Adds color to end of color map if there's room. */
{
int colIx = mg->colorsUsed;
if (colIx < 256)
    {
    struct rgbColor *c = mg->colorMap + mg->colorsUsed;
    c->r = r;
    c->g = g;
    c->b = b;
    mg->colorsUsed += 1;
    colHashAdd(mg->colorHash, r, g, b, colIx);
    }
return (Color)colIx;
}

int mgColorsFree(struct memGfx *mg)
/* Returns # of unused colors in color map. */
{
return 256-mg->colorsUsed;
}

void mgFree(struct memGfx **pmg)
{
struct memGfx *mg = *pmg;
if (mg != NULL)
    {
    if (mg->pixels != NULL)
	freeMem(mg->pixels);
    colHashFree(&mg->colorHash);
    zeroBytes(mg, sizeof(*mg));
    freeMem(mg);
    }
*pmg = NULL;
}

static void nonZeroCopy(Color *d, Color *s, int width)
/* Copy non-zero colors. */
{
Color c;
int i;
for (i=0; i<width; ++i)
    {
    if ((c = s[i]) != 0)
        d[i] = c;
    }
}

static void mgPutSegMaybeZeroClear(struct memGfx *mg, int x, int y, int width, Color *dots, boolean zeroClear)
/* Put a series of dots starting at x, y and going to right width pixels.
 * Possibly don't put zero dots though. */
{
int x2;
Color *pt;
if (y < mg->clipMinY || y > mg->clipMaxY)
    return;
x2 = x + width;
if (x2 > mg->clipMaxX)
    x2 = mg->clipMaxX;
if (x < mg->clipMinX)
    {
    dots += mg->clipMinX - x;
    x = mg->clipMinX;
    }
width = x2 - x;
if (width > 0)
    {
    pt = _mgPixAdr(mg, x, y);
    if (zeroClear)
        nonZeroCopy(pt, dots, width);
    else
	memcpy(pt, dots, width);
    }
}

void mgVerticalSmear(struct memGfx *mg,
	int xOff, int yOff, int width, int height, 
	unsigned char *dots, boolean zeroClear)
/* Put a series of one 'pixel' width vertical lines. */
{
while (--height >= 0)
    {
    mgPutSegMaybeZeroClear(mg, xOff, yOff, width, dots, zeroClear);
    ++yOff;
    }
}

void mgDrawBox(struct memGfx *mg, int x, int y, int width, int height, Color color)
{
int i;
Color *pt;
int x2 = x + width;
int y2 = y + height;
int wrapCount;

if (x < mg->clipMinX)
    x = mg->clipMinX;
if (y < mg->clipMinY)
    y = mg->clipMinY;
if (x2 > mg->clipMaxX)
    x2 = mg->clipMaxX;
if (y2 > mg->clipMaxY)
    y2 = mg->clipMaxY;
width = x2-x;
height = y2-y;
if (width > 0 && height > 0)
    {
    pt = _mgPixAdr(mg,x,y);
    /*colorBin[x][color]++;  increment color count for this pixel */
    wrapCount = _mgBpr(mg) - width;
    while (--height >= 0)
	{
	i = width;
	while (--i >= 0)
	    *pt++ = color;
	pt += wrapCount;
	}
    }
}

void mgBrezy(struct memGfx *mg, int x1, int y1, int x2, int y2, Color color,
	int yBase, boolean fillFromBase)
/* Brezenham line algorithm.  Optionally fill in under line. */
{
if (x1 == x2)
    {
    int y,height;
    if (y1 > y2)
	{
	y = y2;
	height = y1-y2+1;
	}
    else
        {
	y = y1;
	height = y2-y1+1;
	}
    if (fillFromBase)
        {
	if (y < yBase)
	    mgDrawBox(mg, x1, y, 1, yBase-y, color);
	}
    else
        mgDrawBox(mg, x1, y, 1, height, color);
    }
else if (y1 == y2)
    {
    int x,width;
    if (x1 > x2)
        {
	x = x2;
	width = x1-x2+1;
	}
    else
        {
	x = x1;
	width = x2-x1+1;
	}
    if (fillFromBase)
        {
	if (y1 < yBase)
	    mgDrawBox(mg, x, y1, width, yBase - y1, color);
	}
    else
        {
	mgDrawBox(mg, x, y1, width, 1, color);
	}
    }
else
    {
    int duty_cycle;
    int incy;
    int delta_x, delta_y;
    int dots;
    delta_y = y2-y1;
    delta_x = x2-x1;
    if (delta_y < 0) 
	{
	delta_y = -delta_y;
	incy = -1;
	}
    else
	{
	incy = 1;
	}
    if (delta_x < 0) 
	{
	delta_x = -delta_x;
	incy = -incy;
	x1 = x2;
	y1 = y2;
	}
    duty_cycle = (delta_x - delta_y)/2;
    if (delta_x >= delta_y)
	{
	dots = delta_x+1;
	while (--dots >= 0)
	    {
	    if (fillFromBase)
		{
		if (y1 < yBase)
		    mgDrawBox(mg,x1,y1,1,yBase-y1,color);
		}
	    else
		mgPutDot(mg,x1,y1,color);
	    duty_cycle -= delta_y;
	    x1 += 1;
	    if (duty_cycle < 0)
		{
		duty_cycle += delta_x;	  /* update duty cycle */
		y1+=incy;
		}
	    }
	}
    else
	{
	dots = delta_y+1;
	while (--dots >= 0)
	    {
	    if (fillFromBase)
		{
		if (y1 < yBase)
		    mgDrawBox(mg,x1,y1,1,yBase-y1,color);
		}
	    else
		mgPutDot(mg,x1,y1,color);
	    duty_cycle += delta_x;
	    y1+=incy;
	    if (duty_cycle > 0)
		{
		duty_cycle -= delta_y;	  /* update duty cycle */
		x1 += 1;
		}
	    }
	}
    }
}

void mgDrawLine(struct memGfx *mg, int x1, int y1, int x2, int y2, Color color)
/* Draw a line from one point to another. */
{
mgBrezy(mg, x1, y1, x2, y2, color, 0, FALSE);
}

void mgFillUnder(struct memGfx *mg, int x1, int y1, int x2, int y2, 
	int bottom, Color color)
/* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
 * it's top, a horizontal line at bottom as it's bottom, and
 * vertical lines from the bottom to y1 on the left and bottom to
 * y2 on the right. */
{
mgBrezy(mg, x1, y1, x2, y2, color, bottom, TRUE);
}

void mgPutSeg(struct memGfx *mg, int x, int y, int width, Color *dots)
/* Put a series of dots starting at x, y and going to right width pixels. */
{
mgPutSegMaybeZeroClear(mg, x, y, width, dots, FALSE);
}

void mgPutSegZeroClear(struct memGfx *mg, int x, int y, int width, Color *dots)
/* Put a series of dots starting at x, y and going to right width pixels.
 * Don't put zero dots though. */
{
mgPutSegMaybeZeroClear(mg, x, y, width, dots, TRUE);
}


void mgDrawHorizontalLine(struct memGfx *mg, int y1, Color color)
/*special case of mgDrawLine, for horizontal line across entire window 
  at y-value y1.*/
{
mgDrawLine( mg, mg->clipMinX, y1, mg->clipMaxX, y1, color);
}

void mgLineH(struct memGfx *mg, int y, int x1, int x2, Color color)
/* Draw horizizontal line width pixels long starting at x/y in color */
{
if (y >= mg->clipMinY && y < mg->clipMaxY)
    {
    int w;
    if (x1 < mg->clipMinX)
        x1 = mg->clipMinX;
    if (x2 > mg->clipMaxX)
        x2 = mg->clipMaxX;
    w = x2 - x1;
    if (w > 0)
        {
	Color *pt = _mgPixAdr(mg,x1,y);
	while (--w >= 0)
	    *pt++ = color;
	}
    }
}


boolean mgClipForBlit(int *w, int *h, int *sx, int *sy,
	struct memGfx *dest, int *dx, int *dy)
{
/* Make sure we don't overwrite destination. */
int over;

if ((over = dest->clipMinX - *dx) > 0)
    {
    *w -= over;
    *sx += over;
    *dx = dest->clipMinX;
    }
if ((over = dest->clipMinY - *dy) > 0)
    {
    *h -= over;
    *sy += over;
    *dy = dest->clipMinY;
    }
if ((over = *w + *dx - dest->clipMaxX) > 0)
    *w -= over; 
if ((over = *h + *dy - dest->clipMaxY) > 0)
    *h -= over;
return (*h > 0 && *w > 0);
}

void mgTextBlit(int width, int height, int bitX, int bitY,
	unsigned char *bitData, int bitDataRowBytes, 
	struct memGfx *dest, int destX, int destY, 
	Color color, Color backgroundColor)
{
UBYTE *inLine;
UBYTE *outLine;
UBYTE inLineBit;

if (!mgClipForBlit(&width, &height, &bitX, &bitY, dest, &destX, &destY))
    return;

inLine = bitData + (bitX>>3) + bitY * bitDataRowBytes;
inLineBit = (0x80 >> (bitX&7));
outLine = _mgPixAdr(dest,destX,destY);
while (--height >= 0)
    {
    UBYTE *in = inLine;
    UBYTE *out = outLine;
    UBYTE inBit = inLineBit;
    UBYTE inByte = *in++;
    int i = width;
    while (--i >= 0)
	{
	if (inBit & inByte)
	    *out = color;
	++out;
	if ((inBit >>= 1) == 0)
	    {
	    inByte = *in++;
	    inBit = 0x80;
	    }
	}
    inLine += bitDataRowBytes;
    outLine += _mgBpr(dest);
    }
}

void mgTextBlitSolid(int width, int height, int bitX, int bitY,
	unsigned char *bitData, int bitDataRowBytes, 
	struct memGfx *dest, int destX, int destY, 
	Color color, Color backgroundColor)
{
UBYTE *inLine;
UBYTE *outLine;
UBYTE inLineBit;

if (!mgClipForBlit(&width, &height, &bitX, &bitY, dest, &destX, &destY))
    return;
inLine = bitData + (bitX>>3) + bitY * bitDataRowBytes;
inLineBit = (0x80 >> (bitX&7));
outLine = _mgPixAdr(dest,destX,destY);
while (--height >= 0)
    {
    UBYTE *in = inLine;
    UBYTE *out = outLine;
    UBYTE inBit = inLineBit;
    UBYTE inByte = *in++;
    int i = width;
    while (--i >= 0)
	{
	*out++ = ((inBit & inByte) ? color : backgroundColor);
	if ((inBit >>= 1) == 0)
	    {
	    inByte = *in++;
	    inBit = 0x80;
	    }
	}
    inLine += bitDataRowBytes;
    outLine += _mgBpr(dest);
    }
}


void mgText(struct memGfx *mg, int x, int y, Color color, 
	MgFont *font, char *text)
/* Draw a line of text with upper left corner x,y. */
{
gfText(mg, font, text, x, y, color, mgTextBlit, MG_WHITE);
}

void mgTextCentered(struct memGfx *mg, int x, int y, int width, int height, 
	Color color, MgFont *font, char *text)
/* Draw a line of text centered in box defined by x/y/width/height */
{
int fWidth, fHeight;
int xoff, yoff;
fWidth = mgFontStringWidth(font, text);
fHeight = mgFontPixelHeight(font);
xoff = x + (width - fWidth)/2;
yoff = y + (height - fHeight)/2;
if (font == mgSmallFont())
    {
    xoff += 1;
    yoff += 1;
    }
mgText(mg, xoff, yoff, color, font, text);
}

void mgTextRight(struct memGfx *mg, int x, int y, int width, int height, 
	Color color, MgFont *font, char *text)
/* Draw a line of text right justified in box defined by x/y/width/height */
{
int fWidth, fHeight;
int xoff, yoff;
fWidth = mgFontStringWidth(font, text);
fHeight = mgFontPixelHeight(font);
xoff = x + width - fWidth - 1;
yoff = y + (height - fHeight)/2;
if (font == mgSmallFont())
    {
    xoff += 1;
    yoff += 1;
    }
mgText(mg, xoff, yoff, color, font, text);
}

int mgFontPixelHeight(MgFont *font)
/* How high in pixels is font? */
{
return font_cel_height(font);
}

int mgFontLineHeight(MgFont *font)
/* How many pixels to next line ideally? */
{
int celHeight = font_cel_height(font);
return celHeight + 1 + (celHeight/5);
}

int mgFontWidth(MgFont *font, char *chars, int charCount)
/* How wide are a couple of letters? */
{
return fnstring_width(font, (unsigned char *)chars, charCount);
}

int mgFontStringWidth(MgFont *font, char *string)
/* How wide is a string? */
{
return mgFontWidth(font, string, strlen(string));
}

int mgFontCharWidth(MgFont *font, char c)
/* How wide is a character? */
{
return mgFontWidth(font, &c, 1);
}

void mgSlowDot(struct memGfx *mg, int x, int y, int colorIx)
/* Draw a dot when a macro won't do. */
{
mgPutDot(mg, x, y, colorIx);
}

int mgSlowGetDot(struct memGfx *mg, int x, int y)
/* Fetch a dot when a macro won't do. */
{
return mgGetDot(mg, x, y);
}

struct memGfx *mgRotate90(struct memGfx *in)
/* Create a copy of input that is rotated 90 degrees clockwise. */
{
int iWidth = in->width, iHeight = in->height;
struct memGfx *out = mgNew(iHeight, iWidth);
Color *inCol, *outRow, *outRowStart;
int i,j;

memcpy(out->colorMap, in->colorMap, sizeof(out->colorMap));
outRowStart = out->pixels;
for (i=0; i<iWidth; ++i)
    {
    inCol = in->pixels + i;
    outRow = outRowStart;
    outRowStart += _mgBpr(out);
    j = iHeight;
    while (--j >= 0)
        {
	outRow[j] = *inCol;
	inCol += _mgBpr(in);
	}
    }
return out;
}




void vgMgMethods(struct vGfx *vg)
/* Fill in virtual graphics methods for memory based drawing. */
{
vg->pixelBased = TRUE;
vg->close = (vg_close)mgFree;
vg->dot = (vg_dot)mgSlowDot;
vg->getDot = (vg_getDot)mgSlowGetDot;
vg->box = (vg_box)mgDrawBox;
vg->line = (vg_line)mgDrawLine;
vg->text = (vg_text)mgText;
vg->textRight = (vg_textRight)mgTextRight;
vg->textCentered = (vg_textCentered)mgTextCentered;
vg->findColorIx = (vg_findColorIx)mgFindColor;
vg->colorIxToRgb = (vg_colorIxToRgb)mgColorIxToRgb;
vg->setClip = (vg_setClip)mgSetClip;
vg->unclip = (vg_unclip)mgUnclip;
vg->verticalSmear = (vg_verticalSmear)mgVerticalSmear;
vg->fillUnder = (vg_fillUnder)mgFillUnder;
vg->drawPoly = (vg_drawPoly)mgDrawPoly;
}

