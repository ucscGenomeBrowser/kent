/* cairoGfx - a version of memgfx using Cairo for the actual drawing
 * license is hereby granted for all use - public, private or commercial. */

#ifdef USE_CAIRO

#ifndef CAIROGFX_H
#define CAIROGFX_H

#include "cairo/cairo.h"

#ifndef GFXPOLY_H
#include "gfxPoly.h"
#endif

struct memGfx *mgcNew(int width, int height, char *outFname);
/* Get a new thing to draw on in memory. */

void mgcFree(struct memGfx **pmg);
/* Free up memory raster. */

void mgcClearPixelsTrans(struct memGfx *mg);
/* Set all pixels to transparent. */

void mgcClearPixels(struct memGfx *mg);
/* Set all pixels to background. */

void mgcSetClip(struct memGfx *mg, int x, int y, int width, int height);
/* Set clipping rectangle. */

void mgcUnclip(struct memGfx *mg);
/* Set clipping rect cover full thing. */

Color mgcFindColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b);
/* Returns closest color in color map to rgb values.  If it doesn't
 * already exist in color map and there's room, it will create
 * exact color in map. */

Color mgcClosestColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b);
/* Returns closest color in color map to r,g,b */

Color mgcAddColor(struct memGfx *mg, unsigned char r, unsigned char g, unsigned char b);
/* Adds color to end of color map if there's room. */

int mgcColorsFree(struct memGfx *mg);
/* Returns # of unused colors in color map. */

void mgcPutSeg(struct memGfx *mg, int x, int y, int width, Color *dots);
/* Put a series of dots starting at x, y and going to right width pixels. */

void mgcPutSegZeroClear(struct memGfx *mg, int x, int y, int width, Color *dots);
/* Put a series of dots starting at x, y and going to right width pixels.
 * Don't put zero dots though. */

void mgcDrawBox(struct memGfx *mg, int x, int y, int width, int height, Color color);
/* Draw a (horizontal) box */

void mgcDrawLine(struct memGfx *mg, int x1, int y1, int x2, int y2, Color color);
/* Draw a line from one point to another. */

void mgcDrawHorizontalLine(struct memGfx *mg, int y1, Color color);
/*special case of mgDrawLine*/

void mgcLineH(struct memGfx *mg, int y, int x1, int x2, Color color);
/* Draw horizizontal line width pixels long starting at x/y in color */

void mgcSavePng(struct memGfx *mg, char *filename, boolean useTransparency);
/* Save memory bitmap to filename as a PNG.
 * If useTransparency, then the first color in memGfx's colormap/palette is
 * assumed to be the image background color, and pixels of that color
 * are made transparent. */

boolean mgcSaveToPng(FILE *png_file, struct memGfx *mg, boolean useTransparency);
/* Save PNG to an already open file.
 * If useTransparency, then the first color in memGfx's colormap/palette is
 * assumed to be the image background color, and pixels of that color
 * are made transparent. */

void mgcTextBlit(int bitWidth, int bitHeight, int bitX, int bitY,
	unsigned char *bitData, int bitDataRowBytes, 
	struct memGfx *dest, int destX, int destY, 
	Color color, Color backgroundColor);
/* This function leaves the background as it was. */

void mgcTextBlitSolid(int bitWidth, int bitHeight, int bitX, int bitY,
	unsigned char *bitData, int bitDataRowBytes, 
	struct memGfx *dest, int destX, int destY, 
	Color color, Color backgroundColor);
/* This function sets the background to the background color. */

void mgcText(struct memGfx *mg, int x, int y, Color color, 
	MgFont *font, char *text);
/* Draw a line of text with upper left corner x,y. */

void mgcTextCentered(struct memGfx *mg, int x, int y, int width, int height, 
	Color color, MgFont *font, char *text);
/* Draw a line of text centered in box defined by x/y/width/height */

void mgcTextRight(struct memGfx *mg, int x, int y, int width, int height, 
	Color color, MgFont *font, char *text);
/* Draw a line of text right justified in box defined by x/y/width/height */

int mgcFontPixelHeight(MgFont *font);
/* How high in pixels is font? */

int mgcFontLineHeight(MgFont *font);
/* How many pixels to next line ideally? */

int mgcFontWidth(MgFont *font, char *chars, int charCount);
/* How wide are a couple of letters? */

int mgcFontStringWidth(MgFont *font, char *string);
/* How wide is a string? */

int mgcFontCharWidth(MgFont *font, char c);
/* How wide is a character? */

char *mgcFontSizeBackwardsCompatible(char *size);
/* Given "size" argument that may be in old tiny/small/medium/big/huge format,
 * return it in new numerical string format. Do NOT free the return string*/

MgFont *mgcFontForSizeAndStyle(char *textSize, char *fontType);
/* Get a font of given size and style.  Abort with error message if not found.
 * The textSize should be 6,8,10,12,14,18,24 or 34.  For backwards compatibility
 * textSizes of "tiny" "small", "medium", "large" and "huge" are also ok.
 * The fontType should be "medium", "bold", or "fixed" */

MgFont *mgcFontForSize(char *textSize);
/* Get a font of given size and style.  Abort with error message if not found.
 * The textSize should be 6,8,10,12,14,18,24 or 34.  For backwards compatibility
 * textSizes of "tiny" "small", "medium", "large" and "huge" are also ok. */

void mgcFillUnder(struct memGfx *mg, int x1, int y1, int x2, int y2, 
	int bottom, Color color);
/* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
 * it's top, a horizontal line at bottom at it's bottom,  and
 * vertical lines from the bottom to y1 on the left and bottom to
 * y2 on the right. */

struct memGfx *mgcRotate90(struct memGfx *in);
/* Create a copy of input that is rotated 90 degrees clockwise. */

void mgcCircle(struct memGfx *mg, int xCen, int yCen, int rad, 
	Color color, boolean filled);
/* Draw a circle using a stepping algorithm.  Doesn't correct
 * for non-square pixels. */

void mgcDrawPoly(struct memGfx *mg, struct gfxPoly *poly, Color color,
	boolean filled);
/* Draw polygon, possibly filled in color. */

void vgMgcMethods(struct vGfx *vg);
/* Fill in virtual graphics methods for Cairo based drawing. */
#endif /* CAIROGFX_H */

#endif /* USE_CAIRO */
