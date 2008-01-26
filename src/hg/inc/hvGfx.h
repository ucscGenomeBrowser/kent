/* hvGfx - browser graphics interface.  This is a thin layer on top of vGfx
 * that adds wraps it with an object providing genome browser-specific
 * features. */
#ifndef BRGFX_H
#define BRGFX_H
#include "vGfx.h"

struct hvGfx
/* browser graphics interface */
{
    struct hvGfx *next;	/* Next in list. */
    struct vGfx *vg;    /* virtual graphics object */
    boolean rc;         /* revese-complement display (negative strand) */
    int leftMargin;     /* region to left that is not in track display region */
    int trackWidth;     /* track-region width */
    /* these are ease of use fields, copied from vGfx object */
    boolean pixelBased; /* Real pixels, not PostScript or something. */
    int width, height;  /* Virtual pixel dimensions. */
    int clipMinX;       /* X clipping region */
    int clipMaxX;
};

INLINE int hvGfxClipX(struct hvGfx *hvg, int x)
/* clip X to be in cliping region bounds */
{
if (x < hvg->clipMinX)
    x = hvg->clipMinX;
if (x > hvg->clipMaxX)
    x = hvg->clipMaxX;
return x;
}

INLINE int hvGfxAdjXW(struct hvGfx *hvg, int x, int *widthPtr)
/* return X pixel start location and update width for a range, handing
 * reverse-complement mode.  If widthPtr is NULL, a width of 1 pixel is
 * assumed.  -1 is return if all out of range */
{
int width = (widthPtr != NULL) ? *widthPtr : 1;
if (x+width <= hvg->leftMargin)
    return x;     // in label area
else if (hvg->rc)
    {
    // clip, then rc
    int xs = hvGfxClipX(hvg, x);
    int xe = hvGfxClipX(hvg, x+width);
    if (xs >= xe)
        return -1;
    if (widthPtr != NULL)
        *widthPtr = xe - xs;
    // must RC bases on width of track area
    return ((hvg->trackWidth-(xe-hvg->leftMargin))+hvg->leftMargin);
    }
else
    return x;
}

INLINE int hvGfxAdjXX(struct hvGfx *hvg, int x1, int *x2Ptr)
/* return X pixel start location and update end location, handing
 * reverse-complement mode.  -1 is return if all out of range */
{
int width = *x2Ptr - x1;
int x =  hvGfxAdjXW(hvg, x1, &width);
if (x < 0)
    return -1;
else
    {
    *x2Ptr = x + width;
    return x;
    }
}

struct hvGfx *hvGfxOpenGif(int width, int height, int leftMargin, char *fileName);
/* Open up something that we'll write out as a gif someday. */

struct hvGfx *hvGfxOpenPostScript(int width, int height, int leftMargin, char *fileName);
/* Open up something that will someday be a PostScript file. */

void hvGfxClose(struct hvGfx **pHvg);
/* Close down virtual graphics object, and finish writing it to file. */

INLINE void hvGfxDot(struct hvGfx *hvg, int x, int y, int colorIx)
/* Draw a single pixel.  Try to work at a higher level when possible! */
{
vgDot(hvg->vg, hvGfxAdjXW(hvg, x, NULL), y, colorIx);
}

#if 0 // FIXME: drop this
INLINE int hvGfxGetDot(struct hvGfx *hvg, int x, int y)
/* Fetch a single pixel.  Please do not use this, this is special
 * for verticalText only. */
{
return vgGetDot(hvg->vg, hvGfxAdjXW(hvg, x, NULL), y);
}
#endif

INLINE void hvGfxBox(struct hvGfx *hvg, int x, int y, 
                  int width, int height, int colorIx)
/* Draw a box. */
{
x = hvGfxAdjXW(hvg, x, &width);
if (x >= 0)
    vgBox(hvg->vg, x, y, width, height, colorIx);
}

INLINE void hvGfxLine(struct hvGfx *hvg, 
                   int x1, int y1, int x2, int y2, int colorIx)
/* Draw a line from one point to another. */
{
x1  = hvGfxAdjXX(hvg, x1, &x2);
if (x1 >= 0)
    vgLine(hvg->vg, x1, y1, x2, y2, colorIx);
}

INLINE void hvGfxText(struct hvGfx *hvg, int x, int y, int colorIx,
                   void *font, char *text)
/* Draw a line of text with upper left corner x,y. */
{
vgText(hvg->vg, hvGfxAdjXW(hvg, x, NULL), y, colorIx, font, text);
}

INLINE void hvGfxTextRight(struct hvGfx *hvg, int x, int y, int width, int height,
                      int colorIx, void *font, char *text)
/* Draw a line of text with upper right corner x,y. */
{
x = hvGfxAdjXW(hvg, x, &width);
if (x >= 0)
    vgTextRight(hvg->vg, x, y, width, height, colorIx, font, text);
}

INLINE void hvGfxTextCentered(struct hvGfx *hvg, int x, int y, int width, int height,
                           int colorIx, void *font, char *text)
/* Draw a line of text in middle of box. */
{
x = hvGfxAdjXW(hvg, x, &width);
if (x >= 0)
    vgTextCentered(hvg->vg, x, y, width, height, colorIx, font, text);
}

INLINE int hvGfxFindColorIx(struct hvGfx *hvg, int r, int g, int b)
/* Find color in map if possible, otherwise create new color or
 * in a pinch a close color. */
{
return vgFindColorIx(hvg->vg, r, g, b);
}

INLINE struct rgbColor hvGfxColorIxToRgb(struct hvGfx *hvg, int colorIx)
/* Return rgb values for given color index. */
{
return vgColorIxToRgb(hvg->vg, colorIx);
};

INLINE void hvGfxSetClip(struct hvGfx *hvg, int x, int y, int width, int height)
/* Set clipping rectangle. */
{
// this should not be adjusted for RC
vgSetClip(hvg->vg, x, y, width, height);
}

INLINE void hvGfxUnclip(struct hvGfx *hvg)
/* Set clipping rect cover full thing. */
{
vgUnclip(hvg->vg);
hvg->clipMinX = 0;
hvg->clipMaxX = hvg->width;
}

INLINE void hvGfxVerticalSmear(struct hvGfx *hvg,
                            int xOff, int yOff, int width, int height, 
                            unsigned char *dots, boolean zeroClear)
/* Put a series of one 'pixel' width vertical lines. */
{
xOff = hvGfxAdjXW(hvg, xOff, &width);
if (xOff >= 0)
    vgVerticalSmear(hvg->vg, xOff, yOff, width, height, dots, zeroClear);
}

INLINE void hvGfxFillUnder(struct hvGfx *hvg, int x1, int y1, int x2, int y2, 
                        int bottom, Color color)
/* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
 * it's top, a horizontal line at bottom at it's bottom,  and
 * vertical lines from the bottom to y1 on the left and bottom to
 * y2 on the right. */
{
x1  = hvGfxAdjXX(hvg, x1, &x2);
if (x1 >= 0)
    vgFillUnder(hvg->vg, x1, y1, x2, y2, bottom, color);
}

INLINE void hvGfxDrawPoly(struct hvGfx *hvg, struct gfxPoly *poly, Color color, 
                       boolean filled)
/* Draw polygon, possibly filled in color. */
{
vgDrawPoly(hvg->vg, poly, color, filled);
}

INLINE void hvGfxSetHint(struct hvGfx *hvg, char *hint, char *value)
/* Set hint */
{
vgSetHint(hvg->vg,hint,value);
}

INLINE char* hvGfxGetHint(struct hvGfx *hvg, char *hint)
/* Get hint */
{
return vgGetHint(hvg->vg, hint);
}

INLINE int hvGfxFindRgb(struct hvGfx *hvg, struct rgbColor *rgb)
/* Find color index corresponding to rgb color. */
{
return vgFindRgb(hvg->vg, rgb);
}

INLINE Color hvGfxContrastingColor(struct hvGfx *hvg, int backgroundIx)
/* Return black or white whichever would be more visible over
 * background. */
{
return vgContrastingColor(hvg->vg, backgroundIx);
}

void hvGfxBarbedHorizontalLine(struct hvGfx *hvg, int x, int y, 
	int width, int barbHeight, int barbSpacing, int barbDir, Color color,
	boolean needDrawMiddle);
/* Draw a horizontal line starting at xOff, yOff of given width.  Will
 * put barbs (successive arrowheads) to indicate direction of line.  
 * BarbDir of 1 points barbs to right, of -1 points them to left. */

void hvGfxDrawRulerBumpText(struct hvGfx *hvg, int xOff, int yOff, 
	int height, int width,
        Color color, MgFont *font,
        int startNum, int range, int bumpX, int bumpY);
/* Draw a ruler inside the indicated part of mg with numbers that start at
 * startNum and span range.  Bump text positions slightly. */

void hvGfxDrawRuler(struct hvGfx *hvg, int xOff, int yOff, int height, int width,
        Color color, MgFont *font,
        int startNum, int range);
/* Draw a ruler inside the indicated part of mg with numbers that start at
 * startNum and span range.  */

void hvGfxNextItemButton(struct hvGfx *hvg, int x, int y, int w, int h, 
		      Color color, Color hvgColor, boolean nextItem);
/* Draw a button that looks like a fast-forward or rewind button on */
/* a remote control. If nextItem is TRUE, it points right, otherwise */
/* left. color is the outline color, and hvgColor is the fill color. */

void hvGfxMakeColorGradient(struct hvGfx *hvg, 
                            struct rgbColor *start, struct rgbColor *end,
                            int steps, Color *colorIxs);
/* Make a color gradient that goes smoothly from start
 * to end colors in given number of steps.  Put indices
 * in color table in colorIxs */

#endif 
