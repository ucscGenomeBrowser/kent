/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Memgfx - stuff to do graphics in memory buffers.
 * Typically will just write these out as .gif files.
 * This stuff is byte-a-pixel for simplicity.
 * It can do 256 colors.
 */
#ifndef MEMGFX_H
#define MEMGFX_H

#define MG_WHITE 0
#define MG_BLACK 1
#define MG_RED 2
#define MG_GREEN 3
#define MG_BLUE 4
#define MG_CYAN 5
#define MG_MAGENTA 6
#define MG_YELLOW 7
#define MG_GRAY 8
#define MG_FREE_COLORS_START 9


typedef unsigned char Color;

struct rgbColor
    {
    unsigned char r, g, b;
    };

extern struct rgbColor mgFixedColors[9];  /* Contains MG_WHITE - MG_GRAY */

struct memGfx
    {
    Color *pixels;
    int width, height;
    struct rgbColor colorMap[256];
    int colorsUsed;
    int clipMinX, clipMaxX;
    int clipMinY, clipMaxY;
    struct colHash *colorHash;	/* Hash for fast look up of color. */
    };

struct memGfx *mgNew(int width, int height);
/* Get a new thing to draw on in memory. */

void mgFree(struct memGfx **pmg);
/* Free up memory raster. */

void mgClearPixels(struct memGfx *mg);
/* Set all pixels to background. */

void mgSetClip(struct memGfx *mg, int x, int y, int width, int height);
/* Set clipping rectangle. */

void mgUnclip(struct memGfx *mg);
/* Set clipping rect cover full thing. */

Color mgFindColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b);
/* Returns closest color in color map to rgb values.  If it doesn't
 * already exist in color map and there's room, it will create
 * exact color in map. */

Color mgClosestColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b);
/* Returns closest color in color map to r,g,b */

Color mgAddColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b);
/* Adds color to end of color map if there's room. */

int mgColorsFree(struct memGfx *mg);
/* Returns # of unused colors in color map. */


#define _mgBpr(mg) ((mg)->width)
/* Get what to add to get to next line */

#define _mgPixAdr(mg,x,y) ((mg)->pixels+_mgBpr(mg) * (y) + (x))
/* Get pixel address */

#define _mgPutDot(mg, x, y, color) (*_mgPixAdr(mg,x,y) = (color))
/* Unclipped plot a dot */

#define mgPutDot(mg,x,y,color) if ((x)>=(mg)->clipMinX && (x) < (mg)->clipMaxX && (y)>=(mg)->clipMinY  && (y) < (mg)->clipMaxY) _mgPutDot(mg,x,y,color)
/* Clipped put dot */

void mgPutSeg(struct memGfx *mg, int x, int y, int width, Color *dots);
/* Put a series of dots starting at x, y and going to right width pixels. */

void mgPutSegZeroClear(struct memGfx *mg, int x, int y, int width, Color *dots);
/* Put a series of dots starting at x, y and going to right width pixels.
 * Don't put zero dots though. */

void mgDrawBox(struct memGfx *mg, int x, int y, int width, int height, Color color);
/* Draw a (horizontal) box */


void mgDrawLine(struct memGfx *mg, int x1, int y1, int x2, int y2, Color color);
/* Draw a line from one point to another. */


void mgDrawHorizontalLine(struct memGfx *mg, int y1, Color color);
/*special case of mgDrawLine*/

void mgSaveGif(struct memGfx *mg, char *name);
/* Save memory bitmap as a gif. */

boolean mgSaveToGif(FILE *gif_file, struct memGfx *screen);
/* Save GIF to an already open file. */

struct memGfx *mgLoadGif(char *name);
/* Create memory image based on gif file. 
 * Note this is based on a very old gif reader
 * that only handles the GIF87a version. 
 * This is the same that mgSaveGif creates at
 * least.  This version of gif was always
 * color mapped. */

typedef void (*TextBlit)(int bitWidth, int bitHeight, int bitX, int bitY,
	unsigned char *bitData, int bitDataRowBytes, 
	struct memGfx *dest, int destX, int destY, 
	Color color, Color backgroundColor);
/* This defines the type of a function that takes a rectangular
 * area of a bitplane and expands it into a rectangular area
 * of a full color screen. */

void mgTextBlit(int bitWidth, int bitHeight, int bitX, int bitY,
	unsigned char *bitData, int bitDataRowBytes, 
	struct memGfx *dest, int destX, int destY, 
	Color color, Color backgroundColor);
/* This function leaves the background as it was. */

void mgTextBlitSolid(int bitWidth, int bitHeight, int bitX, int bitY,
	unsigned char *bitData, int bitDataRowBytes, 
	struct memGfx *dest, int destX, int destY, 
	Color color, Color backgroundColor);
/* This function sets the background to the background color. */

typedef struct font_hdr MgFont;
/* Type of our font.  */

MgFont *mgTinyFont();
MgFont *mgSmallFont();
MgFont *mgSmallishFont();
MgFont *mgMediumFont();
MgFont *mgLargeFont();
MgFont *mgHugeFont();

void mgText(struct memGfx *mg, int x, int y, Color color, 
	MgFont *font, char *text);
/* Draw a line of text with upper left corner x,y. */

void mgTextCentered(struct memGfx *mg, int x, int y, int width, int height, 
	Color color, MgFont *font, char *text);
/* Draw a line of text centered in box defined by x/y/width/height */

void mgTextRight(struct memGfx *mg, int x, int y, int width, int height, 
	Color color, MgFont *font, char *text);
/* Draw a line of text right justified in box defined by x/y/width/height */

int mgFontPixelHeight(MgFont *font);
/* How high in pixels is font? */

int mgFontLineHeight(MgFont *font);
/* How many pixels to next line ideally? */

int mgFontWidth(MgFont *font, char *chars, int charCount);
/* How wide are a couple of letters? */

int mgFontStringWidth(MgFont *font, char *string);
/* How wide is a string? */

int mgFontCharWidth(MgFont *font, char c);
/* How wide is a character? */

void mgFillUnder(struct memGfx *mg, int x1, int y1, int x2, int y2, 
	int bottom, Color color);
/* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
 * it's top, a horizontal line at bottom at it's bottom,  and
 * vertical lines from the bottom to y1 on the left and bottom to
 * y2 on the right. */

#endif /* MEMGFX_H */
