 /* hvGfx - browser graphics interface.  This is a thin layer on top of vGfx
 * providing genome browser-specific features.*/

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef BRGFX_H
#define BRGFX_H
#include "vGfx.h"
#include "dnautil.h"

struct hvGfx
/* browser graphics interface */
{
    struct hvGfx *next;	     /* Next in list. */
    struct vGfx *vg;         /* virtual graphics object */
    boolean rc;              /* reverse-complement display (negative strand) */
    boolean pixelBased;      /* Real pixels, not PostScript or something. */
    int width, height;       /* Virtual pixel dimensions. */
    int clipMinX;            /* X clipping region, before reverse */
    int clipMaxX;
    int clipMinY;
    int clipMaxY;
};

struct hvGfx *hvGfxOpenPng(int width, int height, char *fileName, boolean useTransparency);
/* Open up something that we'll write out as a PNG someday.
 * If useTransparency, then the first color in memgfx's colormap/palette is
 * assumed to be the image background color, and pixels of that color
 * are made transparent. */

struct hvGfx *hvGfxOpenPostScript(int width, int height, char *fileName);
/* Open up something that will someday be a PostScript file. */

void hvGfxClose(struct hvGfx **pHvg);
/* Close down virtual graphics object, and finish writing it to file. */

INLINE int hvGfxAdjX(struct hvGfx *hvg, int x)
/* Return an X position, updating if reverse-complement mode */
{
if (hvg->rc)
    return hvg->width - (x+1);
else
    return x;
}

INLINE int hvGfxAdjXX(struct hvGfx *hvg, int x1, int *x2Ptr,
                      int *y1Ptr, int *y2Ptr)
/* Update a pair of coordinates if reverse-coordinates mode */
{
if (hvg->rc)
    {
    reverseIntRange(&x1, x2Ptr, hvg->width);
    int hold = *y1Ptr;
    *y1Ptr = *y2Ptr;
    *y2Ptr = hold;
    }
return x1;
}

INLINE int hvGfxAdjXW(struct hvGfx *hvg, int x, int width)
/* Update a X position and width if reverse-complement mode */
{
if (hvg->rc)
    {
    int x2 = (x + width);
    reverseIntRange(&x, &x2, hvg->width);
    }
return x;
}

INLINE void hvGfxCircle(struct hvGfx *hvg, int xCen, int yCen, int rad, int colorIx,  boolean filled)
/* Draw a circle. */
{
vgCircle(hvg->vg, hvGfxAdjX(hvg, xCen), yCen, rad, colorIx, filled);
}

INLINE void hvGfxDot(struct hvGfx *hvg, int x, int y, int colorIx)
/* Draw a single pixel.  Try to work at a higher level when possible! */
{
vgDot(hvg->vg, hvGfxAdjX(hvg, x), y, colorIx);
}

INLINE void hvGfxBox(struct hvGfx *hvg, int x, int y, 
                  int width, int height, int colorIx)
/* Draw a box. */
{
vgBox(hvg->vg, hvGfxAdjXW(hvg, x, width), y, width, height, colorIx);
}

INLINE void hvGfxOutlinedBox(struct hvGfx *hvg, int x, int y,
                  int width, int height, int fillColorIx, int lineColorIx)
/* Draw a box in fillColor outlined by lineColor */
{
/* If nothing to draw bail out early. */
if (width <= 0 || height <= 0)
    return;

/* Draw outline first, it may be all we need. We may not even need all 4 lines of it. */
hvGfxBox(hvg, x, y, width, 1, lineColorIx);
if (height == 1)
    return;
hvGfxBox(hvg, x, y+1, 1, height-1, lineColorIx);
if (width == 1)
    return;
hvGfxBox(hvg, x+width-1, y+1, 1, height-1, lineColorIx);
if (width == 2)
    return;
hvGfxBox(hvg, x+1, y+height-1, width-2, 1, lineColorIx);
if (height == 2)
    return;

/* Can draw fill with a single  */
hvGfxBox(hvg, x+1, y+1, width-2, height-2, fillColorIx);
}

INLINE void hvGfxLine(struct hvGfx *hvg, 
                   int x1, int y1, int x2, int y2, int colorIx)
/* Draw a line from one point to another. */
{
x1 = hvGfxAdjXX(hvg, x1, &x2, &y1, &y2);
vgLine(hvg->vg, x1, y1, x2, y2, colorIx);
}

INLINE void hvGfxText(struct hvGfx *hvg, int x, int y, int colorIx,
                      void *font, char *text)
/* Draw a line of text with upper left corner x,y. */
{
if (hvg->rc)
    {
    // FIXME: test
    // move x,y to lower-left corner
    int width = vgGetFontStringWidth(hvg->vg, font, text);
    int height = vgGetFontPixelHeight(hvg->vg, font);
    vgTextRight(hvg->vg, hvGfxAdjXW(hvg, x, width), y, width, height, colorIx, font, text);
    }
else
    vgText(hvg->vg, x, y, colorIx, font, text);
}

INLINE void hvGfxTextRight(struct hvGfx *hvg, int x, int y, int width, int height,
                      int colorIx, void *font, char *text)
/* Draw a line of text with upper right corner x,y. */
{
if (hvg->rc)
    {
    // move x,y to upper right corner
    int fHeight = vgGetFontPixelHeight(hvg->vg, font);
    // this is what mgTextRight does,  not sure why
    y += (height - fHeight)/2 + ((font == mgSmallFont()) ?  1 : 0);
    vgText(hvg->vg, hvGfxAdjXW(hvg, x, width), y, colorIx, font, text);
    }
else
    vgTextRight(hvg->vg, x, y, width, height, colorIx, font, text);
}

INLINE void hvGfxTextCentered(struct hvGfx *hvg, int x, int y, int width, int height,
                           int colorIx, void *font, char *text)
/* Draw a line of text in middle of box. */
{
vgTextCentered(hvg->vg, hvGfxAdjXW(hvg, x, width), y, width, height, colorIx, font, text);
}

INLINE void hvGfxSetWriteMode(struct hvGfx *hvg, int writeMode)
/* set write mode */
{
vgSetWriteMode(hvg->vg, writeMode);
}

INLINE void hvGfxSetClip(struct hvGfx *hvg, int x, int y, int width, int height)
/* Set clipping rectangle. */
{
// this should not be adjusted for RC
hvg->clipMinX = x;
hvg->clipMaxX = x + width;
hvg->clipMinY = y;
hvg->clipMaxY = y + height;
// clipping region is reversed in actual vGfx
if (hvg->rc)
    x = hvg->width - (x + width);
vgSetClip(hvg->vg, x, y, width, height);
}

INLINE void hvGfxGetClip(struct hvGfx *hvg, int *retX, int *retY, int *retWidth, int *retHeight)
/* Get clipping rectangle. */
{
if (retX != NULL)
    *retX = hvg->clipMinX;
if (retY != NULL)
    *retY = hvg->clipMinY;
if (retWidth != NULL)
    *retWidth = (hvg->clipMaxX - hvg->clipMinX);
if (retHeight != NULL)
    *retHeight = (hvg->clipMaxY - hvg->clipMinY);
}

INLINE void hvGfxUnclip(struct hvGfx *hvg)
/* Set clipping rect cover full thing. */
{
hvg->clipMinX = 0;
hvg->clipMaxX = hvg->width;
hvg->clipMinY = 0;
hvg->clipMaxY = hvg->height;
vgUnclip(hvg->vg);
}

INLINE void hvGfxVerticalSmear(struct hvGfx *hvg,
                               int xOff, int yOff, int width, int height, 
                               Color *dots, boolean zeroClear)
/* Put a series of one '8-bit pixel' width vertical lines. */
{
vgVerticalSmear(hvg->vg, hvGfxAdjXW(hvg, xOff, width), yOff, width, height, dots, zeroClear);
}

INLINE void hvGfxFillUnder(struct hvGfx *hvg, int x1, int y1, int x2, int y2, 
                        int bottom, Color color)
/* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
 * it's top, a horizontal line at bottom at it's bottom,  and
 * vertical lines from the bottom to y1 on the left and bottom to
 * y2 on the right. */
{
x1  = hvGfxAdjXX(hvg, x1, &x2, &y1, &y2);
vgFillUnder(hvg->vg, x1, y1, x2, y2, bottom, color);
}

INLINE void hvGfxRevPoly(struct hvGfx *hvg, struct gfxPoly *poly)
/* reverse coordinates in polygons */
{
/* reverse but don't clip; clipping requires working at the line level */
struct gfxPoint *pt = poly->ptList;
int i;
for (i = 0; i < poly->ptCount; i++, pt = pt->next)
    pt->x = hvg->width - (pt->x+1);
}

INLINE void hvGfxDrawPoly(struct hvGfx *hvg, struct gfxPoly *poly, Color color, 
                       boolean filled)
/* Draw polygon, possibly filled in color. */
{
if (hvg->rc)
    hvGfxRevPoly(hvg, poly);
vgDrawPoly(hvg->vg, poly, color, filled);
if (hvg->rc)
    hvGfxRevPoly(hvg, poly);  // restore
}

INLINE void hvGfxEllipseDraw(struct hvGfx *hvg, int x1, int y1, int x2, int y2, Color color, 
                                int mode, boolean isDashed)
/* Draw an ellipse (or limit to top or bottom) specified by rectangle.
 * Optionally, alternate dots.
 * Point 0 is left, point 1 is top of rectangle.
 */
{
x1 = hvGfxAdjXX(hvg, x1, &x2, &y1, &y2);
// NOTE: dashed mode may cause a hang, so disabling for now
vgEllipse(hvg->vg, x1, y1, x2, y2, color, mode, FALSE);
}

INLINE int hvGfxCurve(struct hvGfx *hvg, int x1, int y1, int x2, int y2, int x3, int y3,
                                Color color, boolean isDashed)
/* Draw a segment of an anti-aliased curve within 3 points (quadratic Bezier)
 * Return max y value. Optionally draw curve as dashed line.
 * Adapted trivially from code posted at http://members.chello.at/~easyfilter/bresenham.html
 * Author: Zingl Alois, 8/22/2016 */
{
x1 = hvGfxAdjXX(hvg, x1, &x2, &y1, &y2);
return vgCurve(hvg->vg, x1, y1, x2, y2, x3, y3, color, isDashed);
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

void hvGfxMakeColorGradient(struct hvGfx *hvg, 
                            struct rgbColor *start, struct rgbColor *end,
                            int steps, Color *colorIxs);
/* Make a color gradient that goes smoothly from start to end colors in given
 * number of steps.  Put indices in color table in colorIxs */

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

/*
void hvGfxEllipseDraw(struct hvGfx *hvg, int x0, int y0, int x1, int y1, Color color, 
                        int mode, boolean isDotted);
*/
/* Draw an ellipse (or limit to top or bottom) specified by rectangle, using Bresenham algorithm.
 * Optionally, alternate dots.
 * Point 0 is left, point 1 is top of rectangle
 * Adapted trivially from code posted at http://members.chello.at/~easyfilter/bresenham.html
 */

/*
int hvGfxCurve(struct hvGfx *hvg, int x0, int y0, int x1, int y1, int x2, int y2, 
                        Color color, boolean isDotted);
*/
/* Draw a segment of an anti-aliased curve within 3 points (quadratic Bezier)
 * Return max y value. Optionally alternate dots.
 * Adapted trivially from code posted at http://members.chello.at/~easyfilter/bresenham.html */
 /* Thanks to author  * @author Zingl Alois
 * @date 22.08.2016 */

void hvGfxDottedLine(struct hvGfx *hvg, int x1, int y1, int x2, int y2, Color color, boolean isDash);
/* Brezenham line algorithm, alternating dots, by 1 pixel or two (isDash true) */

#endif 
