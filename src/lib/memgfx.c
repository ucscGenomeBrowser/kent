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



Color multiply(Color src, Color new)
{
#ifdef COLOR32
unsigned char rs = (src >> 0) & 0xff;
unsigned char gs = (src >> 8) & 0xff;
unsigned char bs = (src >> 16) & 0xff;
unsigned char rn = (new >> 0) & 0xff;
unsigned char gn = (new >> 8) & 0xff;
unsigned char bn = (new >> 16) & 0xff;

unsigned char ro = ((unsigned) rn * rs) / 255;
unsigned char go = ((unsigned) gn * gs) / 255;
unsigned char bo = ((unsigned) bn * bs) / 255;
return MAKECOLOR_32(ro, go, bo);
#else
/* no multiply write mode in 8 bit */
return new;
#endif /* COLOR32 */
}


#ifndef min3
#define min3(x,y,z) (min(x,min(y,z)))
/* Return min of x,y, and z. */
#endif

#ifndef max3
#define max3(x,y,z) (max(x,max(y,z)))
/* Return max of x,y, and z. */
#endif

void _mgPutDotMultiply(struct memGfx *mg, int x, int y,Color color)
{
Color src = *_mgPixAdr(mg,x,y);
*_mgPixAdr(mg,x,y) = multiply(src, color);
}


static void mgSetDefaultColorMap(struct memGfx *mg)
/* Set up default color map for a memGfx. */
{
#ifdef COLOR32
    return;
#else

/* Note dependency in order here and in MG_WHITE, MG_BLACK, etc. */
int i;
for (i=0; i<ArraySize(mgFixedColors); ++i)
    {
    struct rgbColor *c = &mgFixedColors[i];
    mgAddColor(mg, c->r, c->g, c->b);
    }
#endif
}



void mgSetWriteMode(struct memGfx *mg, unsigned int writeMode)
/* Set write mode */
{
mg->writeMode = writeMode;
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
/* Return new memGfx. Note new pixel memory is uninitialized */
{
struct memGfx *mg;

mg = needMem(sizeof(*mg));
mg->width = width;
mg->height = height;
mg->pixels = needLargeMem(width*height*sizeof(Color));
#ifndef COLOR32
mg->colorHash = colHashNew();
#endif
mgSetDefaultColorMap(mg);
mgUnclip(mg);
return mg;
}

void mgClearPixels(struct memGfx *mg)
/* Set all pixels to background. */
{
#ifdef COLOR32
memset((unsigned char *)mg->pixels, 0xff, mg->width*mg->height*sizeof(unsigned int));
#else
zeroBytes(mg->pixels, mg->width*mg->height);
#endif
}

void mgClearPixelsTrans(struct memGfx *mg)
/* Set all pixels to transparent. */
{
#ifdef COLOR32
unsigned int *ptr = mg->pixels;
unsigned int *lastPtr = &mg->pixels[mg->width * mg->height];
for(; ptr < lastPtr; ptr++)
#ifdef MEMGFX_BIGENDIAN
    *ptr = 0xffffff00;
#else
    *ptr = 0x00ffffff;  // transparent white
#endif

#else
zeroBytes(mg->pixels, mg->width*mg->height);
#endif
}

Color mgFindColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b)
/* Returns closest color in color map to rgb values.  If it doesn't
 * already exist in color map and there's room, it will create
 * exact color in map. */
{
#ifdef COLOR32
return MAKECOLOR_32(r,g,b);
#else
struct colHashEl *che;
if ((che = colHashLookup(mg->colorHash, r, g, b)) != NULL)
    return che->ix;
if (mgColorsFree(mg))
    return mgAddColor(mg, r, g, b);
return mgClosestColor(mg, r, g, b);
#endif
}


struct rgbColor mgColorIxToRgb(struct memGfx *mg, int colorIx)
/* Return rgb value at color index. */
{
#ifdef COLOR32
static struct rgbColor rgb;
#ifdef MEMGFX_BIGENDIAN
rgb.r = (colorIx >> 24) & 0xff;
rgb.g = (colorIx >> 16) & 0xff;
rgb.b = (colorIx >> 8) & 0xff;
#else
rgb.r = (colorIx >> 0) & 0xff;
rgb.g = (colorIx >> 8) & 0xff;
rgb.b = (colorIx >> 16) & 0xff;
#endif

return rgb;
#else
return mg->colorMap[colorIx];
#endif
}

Color mgClosestColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b)
/* Returns closest color in color map to r,g,b */
{
#ifdef COLOR32
return MAKECOLOR_32(r,g,b);
#else
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
#endif
}


Color mgAddColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b)
/* Adds color to end of color map if there's room. */
{
#ifdef COLOR32
return MAKECOLOR_32(r,g,b);
#else
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
#endif
}

int mgColorsFree(struct memGfx *mg)
/* Returns # of unused colors in color map. */
{
#ifdef COLOR32
return 1 << 23;
#else
return 256-mg->colorsUsed;
#endif
}

void mgFree(struct memGfx **pmg)
{
struct memGfx *mg = *pmg;
if (mg != NULL)
    {
    if (mg->pixels != NULL)
	freeMem(mg->pixels);
    if (mg->colorHash)
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
    if ((c = s[i]) != MG_WHITE)
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
        {
        width *= sizeof(Color);
        memcpy(pt, dots, width * sizeof(Color));
        }
    }
}

void mgVerticalSmear(struct memGfx *mg,
	int xOff, int yOff, int width, int height, 
	Color *dots, boolean zeroClear)
/* Put a series of one 'pixel' width vertical lines. */
{
while (--height >= 0)
    {
    mgPutSegMaybeZeroClear(mg, xOff, yOff, width, dots, zeroClear);
    ++yOff;
    }
}


void mgDrawBoxNormal(struct memGfx *mg, int x, int y, int width, int height, Color color)
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
        //Color src = *pt;
	i = width;
	while (--i >= 0)
	    *pt++ = color;
	pt += wrapCount;
	}
    }
}

void mgDrawBoxMultiply(struct memGfx *mg, int x, int y, int width, int height, Color color)
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
    wrapCount = _mgBpr(mg) - width;
    while (--height >= 0)
	{
        Color src = *pt;
	i = width;
	while (--i >= 0)
	    *pt++ = multiply(src, color);
	pt += wrapCount;
	}
    }
}

void mgDrawBox(struct memGfx *mg, int x, int y, int width, int height, Color color)
{
switch(mg->writeMode)
    {
    case MG_WRITE_MODE_NORMAL:
        {
        mgDrawBoxNormal(mg,x,y, width, height, color);
        }
        break;
    case MG_WRITE_MODE_MULTIPLY:
        {
        mgDrawBoxMultiply(mg,x,y, width, height, color);
        }
        break;
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
Color *outLine;
UBYTE inLineBit;

if (!mgClipForBlit(&width, &height, &bitX, &bitY, dest, &destX, &destY))
    return;

inLine = bitData + (bitX>>3) + bitY * bitDataRowBytes;
inLineBit = (0x80 >> (bitX&7));
outLine = _mgPixAdr(dest,destX,destY);
while (--height >= 0)
    {
    UBYTE *in = inLine;
    Color *out = outLine;
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
Color *outLine;
UBYTE inLineBit;

if (!mgClipForBlit(&width, &height, &bitX, &bitY, dest, &destX, &destY))
    return;
inLine = bitData + (bitX>>3) + bitY * bitDataRowBytes;
inLineBit = (0x80 >> (bitX&7));
outLine = _mgPixAdr(dest,destX,destY);
while (--height >= 0)
    {
    UBYTE *in = inLine;
    Color *out = outLine;
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

int mgGetFontPixelHeight(struct memGfx *mg, MgFont *font)
/* How high in pixels is font? */
{
return mgFontPixelHeight(font);
}

int mgFontLineHeight(MgFont *font)
/* How many pixels to next line ideally? */
{
return font_line_height(font);
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

int mgGetFontStringWidth(struct memGfx *mg, MgFont *font, char *string)
/* How wide is a string? */
{
return mgFontStringWidth(font, string);
}

int mgFontCharWidth(MgFont *font, char c)
/* How wide is a character? */
{
return mgFontWidth(font, &c, 1);
}

char *mgFontSizeBackwardsCompatible(char *size)
/* Given "size" argument that may be in old tiny/small/medium/big/huge format,
 * return it in new numerical string format. Do NOT free the return string*/
{
if (isdigit(size[0]))
    return size;
else if (sameWord(size, "tiny"))
    return "6";
else if (sameWord(size, "small"))
    return "8";
else if (sameWord(size, "medium"))
    return "14";
else if (sameWord(size, "large"))
    return "18";
else if (sameWord(size, "huge"))
    return "34";
else
    {
    errAbort("unknown font size %s", size);
    return NULL;
    }
}

MgFont *mgFontForSizeAndStyle(char *textSize, char *fontType)
/* Get a font of given size and style.  Abort with error message if not found.
 * The textSize should be 6,8,10,12,14,18,24 or 34.  For backwards compatibility
 * textSizes of "tiny" "small", "medium", "large" and "huge" are also ok.
 * The fontType should be "medium", "bold", or "fixed" */
{
textSize = mgFontSizeBackwardsCompatible(textSize);
MgFont *font = NULL;
if (sameString(fontType,"bold"))
    {
    if (sameString(textSize, "6"))
	 font = mgTinyBoldFont();
    else if (sameString(textSize, "8"))
	 font = mgHelveticaBold8Font();
    else if (sameString(textSize, "10"))
	 font = mgHelveticaBold10Font();
    else if (sameString(textSize, "12"))
	 font = mgHelveticaBold12Font();
    else if (sameString(textSize, "14"))
	 font = mgHelveticaBold14Font();
    else if (sameString(textSize, "18"))
	 font = mgHelveticaBold18Font();
    else if (sameString(textSize, "24"))
	 font = mgHelveticaBold24Font();
    else if (sameString(textSize, "34"))
	 font = mgHelveticaBold34Font();
    else
	 errAbort("unknown textSize %s", textSize);
    }
else if (sameString(fontType,"fixed"))
    {
    if (sameString(textSize, "6"))
	 font = mgTinyFixedFont();
    else if (sameString(textSize, "8"))
	 font = mgCourier8Font();
    else if (sameString(textSize, "10"))
	 font = mgCourier10Font();
    else if (sameString(textSize, "12"))
	 font = mgCourier12Font();
    else if (sameString(textSize, "14"))
	 font = mgCourier14Font();
    else if (sameString(textSize, "18"))
	 font = mgCourier18Font();
    else if (sameString(textSize, "24"))
	 font = mgCourier24Font();
    else if (sameString(textSize, "34"))
	 font = mgCourier34Font();
    else
	 errAbort("unknown textSize %s", textSize);
    }
else
    {
    if (sameString(textSize, "6"))
	 font = mgTinyFont();
    else if (sameString(textSize, "8"))
	 font = mgSmallFont();
    else if (sameString(textSize, "10"))
	 font = mgHelvetica10Font();
    else if (sameString(textSize, "12"))
	 font = mgHelvetica12Font();
    else if (sameString(textSize, "14"))
	 font = mgHelvetica14Font();
    else if (sameString(textSize, "18"))
	 font = mgHelvetica18Font();
    else if (sameString(textSize, "24"))
	 font = mgHelvetica24Font();
    else if (sameString(textSize, "34"))
	 font = mgHelvetica34Font();
    else
	 errAbort("unknown textSize %s", textSize);
    }
return font;
}

MgFont *mgFontForSize(char *textSize)
/* Get a font of given size and style.  Abort with error message if not found.
 * The textSize should be 6,8,10,12,14,18,24 or 34.  For backwards compatibility
 * textSizes of "tiny" "small", "medium", "large" and "huge" are also ok. */
{
return mgFontForSizeAndStyle(textSize, "medium");
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

void mgSetHint(char *hint)
/* dummy function */
{
return;
}

char *mgGetHint(char *hint)
/* dummy function */
{
return "";
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
vg->setWriteMode = (vg_setWriteMode)mgSetWriteMode;
vg->setClip = (vg_setClip)mgSetClip;
vg->unclip = (vg_unclip)mgUnclip;
vg->verticalSmear = (vg_verticalSmear)mgVerticalSmear;
vg->fillUnder = (vg_fillUnder)mgFillUnder;
vg->drawPoly = (vg_drawPoly)mgDrawPoly;
vg->setHint = (vg_setHint)mgSetHint;
vg->getHint = (vg_getHint)mgGetHint;
vg->getFontPixelHeight = (vg_getFontPixelHeight)mgGetFontPixelHeight;
vg->getFontStringWidth = (vg_getFontStringWidth)mgGetFontStringWidth;
}


struct hslColor mgRgbToHsl(struct rgbColor rgb)
/* Convert RGB to HSL colorspace (see http://en.wikipedia.org/wiki/HSL_and_HSV)
 * In HSL, Hue is the color in the range [0,360) with 0=red 120=green 240=blue,
 * Saturation goes from a shade of grey (0) to fully saturated color (1000), and
 * Lightness goes from black (0) through the hue (500) to white (1000). */
{
unsigned char rgbMax = max3(rgb.r, rgb.g, rgb.b);
unsigned char rgbMin = min3(rgb.r, rgb.g, rgb.b);
unsigned char delta = rgbMax - rgbMin;
unsigned short minMax = rgbMax + rgbMin;
int divisor;
struct hslColor hsl = { 0.0, 0, (1000*minMax+255)/(2*255) }; // round up

// if max=min then no saturation, and this is gray
if (rgbMax == rgbMin)
    return hsl;
else if (hsl.l <= 500)
    divisor = minMax;
else
    divisor = (2*255-minMax);
hsl.s = (1000*delta + divisor/2)/divisor; // round up

// Saturation so compute hue 0..360 degrees (same as for HSV)
if (rgbMax == rgb.r) // red is 0 +/- offset in blue or green direction
    {
    hsl.h = 0 + 60.0*(rgb.g - rgb.b)/delta;
    }
else if (rgbMax == rgb.g) // green is 120 +/- offset in blue or red direction
    {
    hsl.h = 120 + 60.0*(rgb.b - rgb.r)/delta;
    }
else // rgb_max == rgb.b // blue is 240 +/- offset in red or green direction
    {
    hsl.h = 240 + 60.0*(rgb.r - rgb.g)/delta;
    }
// normalize to [0,360)
if (hsl.h < 0.0)
    hsl.h += 360.0;
else if (hsl.h >= 360.0)
    hsl.h -= 360.0;
return hsl;
}


struct hsvColor mgRgbToHsv(struct rgbColor rgb) 
/* Convert RGB to HSV colorspace (see http://en.wikipedia.org/wiki/HSL_and_HSV)
 * In HSV, Hue is the color in the range [0,360) with 0=red 120=green 240=blue,
 * Saturation goes from white (0) to fully saturated color (1000), and
 * Value goes from black (0) through to the hue (1000). */
{
unsigned char rgbMax = max3(rgb.r, rgb.g, rgb.b);
unsigned char rgbMin = min3(rgb.r, rgb.g, rgb.b);
unsigned char delta = rgbMax - rgbMin;
struct hsvColor hsv = {0.0, 0, 1000*rgbMax/255};

if (hsv.v == 0) 
    return hsv;
hsv.s = 1000*delta/rgbMax;
// if no saturation, then this is gray
if (hsv.s == 0) 
    return hsv;
// Saturation so compute hue 0..360 degrees (same as for HSL)
if (rgbMax == rgb.r) // red is 0 +/- offset in blue or green direction
    {
    hsv.h = 0 + 60.0*(rgb.g - rgb.b)/delta;
    } 
else if (rgbMax == rgb.g) // green is 120 +/- offset in blue or red direction
    {
    hsv.h = 120 + 60.0*(rgb.b - rgb.r)/delta;
    } 
else // rgb_max == rgb.b // blue is 240 +/- offset in red or green direction
    {
    hsv.h = 240 + 60.0*(rgb.r - rgb.g)/delta;
    }
// normalize to [0,360)
if (hsv.h < 0.0)
    hsv.h += 360.0;
else if (hsv.h >= 360.0)
    hsv.h -= 360.0;
return hsv;
}


struct rgbColor mgHslToRgb(struct hslColor hsl)
/* Convert HSL to RGB colorspace http://en.wikipedia.org/wiki/HSL_and_HSV */
{
int p, q;
double r, g, b;
//unsigned short p, q, r, g, b;
double tR, tG, tB;

if( hsl.s == 0 ) // achromatic (grey)
    return (struct rgbColor) {(255*hsl.l+500)/1000, (255*hsl.l+500)/1000, (255*hsl.l+500)/1000};
if (hsl.l <= 500)
    q = hsl.l + (hsl.l*hsl.s+500)/1000;
else
    q = hsl.l + hsl.s - (hsl.l*hsl.s+500)/1000;
p = 2 * hsl.l - q;
hsl.h /= 360.0; // normalize h to 0..1
tR = hsl.h + 1/3.0;
tG = hsl.h;
tB = hsl.h - 1/3.0;
if (tR < 0.0)
    tR += 1.0;
else if (tR > 1.0)
    tR -= 1.0;
if (tG < 0.0)
    tG += 1.0;
else if (tG > 1.0)
    tG -= 1.0;
if (tB < 0.0)
    tB += 1.0;
else if (tB > 1.0)
    tB -= 1.0;
// Red
if (tR < 1/6.0)
    r = p + (q-p)*6*tR;
else if (tR < 0.5)
    r = q;
else if (tR < 2/3.0)
    r = p + (q-p)*6*(2/3.0 - tR);
else
    r = p;
// Green
if (tG < 1/6.0)
    g = p + (q-p)*6*tG;
else if (tG < 0.5)
    g = q;
else if (tG < 2/3.0)
    g = p + (q-p)*6*(2/3.0 - tG);
else
    g = p;
// Blue
if (tB < 1/6.0)
    b = p + (q-p)*6*tB;
else if (tB < 0.5)
    b = q;
else if (tB < 2/3.0)
    b = p + (q-p)*6*(2/3.0 - tB);
else
    b = p;
return (struct rgbColor) {(255*r+500)/1000, (255*g+500)/1000, (255*b+500)/1000}; // round up
}


struct rgbColor mgHsvToRgb(struct hsvColor hsv)
/* Convert HSV to RGB colorspace http://en.wikipedia.org/wiki/HSL_and_HSV */
{
int i;
double f;
unsigned short low, q, t, r, g, b;

if( hsv.s == 0 ) // achromatic (grey)
    return (struct rgbColor) {(255*hsv.v+500)/1000, (255*hsv.v+500)/1000, (255*hsv.v+500)/1000};
hsv.h /= 60.0; 
i = (int) hsv.h;                     // floor the floating point value, sector 0 to 5
f = hsv.h - i;                       // fractional part (distance) from hue 
// hsv.v is highest r,g, or b value
// low value is related to saturation
low = hsv.v - (hsv.v * hsv.s / 1000); // lowest r,g, or b value
t = low + (hsv.v - low) * f;         // scaled value from low..high
q = low + (hsv.v - low) * (1.0-f);   // scaled value from low..high

switch( i )
    {
    case 0:
	r = hsv.v;
        g = t;
        b = low;
        break;
    case 1:
        r = q;
        g = hsv.v;
        b = low;
        break;
    case 2:
        r = low;
        g = hsv.v;
        b = t;
        break;
    case 3:
        r = low;
        g = q;
        b = hsv.v;
        break;
    case 4:
        r = t;
        g = low;
        b = hsv.v;
        break;
    default:                // case 5:
        r = hsv.v;
        g = low;
        b = q;
        break;
    }
return (struct rgbColor) {(255*r+500)/1000, (255*g+500)/1000, (255*b+500)/1000};
}


struct rgbColor mgRgbTransformHsl(struct rgbColor in, double h, double s, double l)
/* Transform rgb 'in' value using
 *   hue shift 'h' (0..360 degrees), 
 *   saturation scale 's', and 
 *   lightness scale 'l'
 * Returns the transformed rgb value 
 * Use H=0, S=L=1 for identity transformation
 */
{
struct hslColor hsl = mgRgbToHsl(in);
hsl.h += h;
while (hsl.h < 0.0)
    hsl.h += 360.0;
while (hsl.h > 360.0)
    hsl.h -= 360.0;
hsl.s = min(max(hsl.s * s, 0), 1000);
hsl.l = min(max(hsl.l * l, 0), 1000);
return mgHslToRgb(hsl);
}


struct rgbColor mgRgbTransformHsv(struct rgbColor in, double h, double s, double v)
/* Transform rgb 'in' value using
 *   hue shift 'h' (0..360 degrees), 
 *   saturation scale 's', and 
 *   value scale 'v'
 * Returns the transformed rgb value 
 * Use H=0, S=V=1 for identity transformation
 */
{
struct hsvColor hsv = mgRgbToHsv(in);
hsv.h += h;
while (hsv.h < 0.0)
    hsv.h += 360.0;
while (hsv.h > 360.0)
    hsv.h -= 360.0;
hsv.s = min(max(hsv.s * s, 0), 1000);
hsv.v = min(max(hsv.v * v, 0), 1000);
return mgHsvToRgb(hsv);
}

