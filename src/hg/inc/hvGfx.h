/* hvGfx - browser graphics interface.  This is a thin layer on top of vGfx
 * that divides an image up into panes and providing genome browser-specific
 * features. */
#ifndef BRGFX_H
#define BRGFX_H
#include "vGfx.h"

enum hvGfxPaneId
/* set of integer ids to identify panes */
{
    hvGfxPaneLabels,
    hvGfxPaneTracks
};

struct hvGfxPane
/* the hvGfx window is divided into panes, each of which has its own
 * clipping region.  Currently only a horizontal layout of panes is
 * supported.  This object can be used to render to an image without
 * needing computer offsets into of the pixels.  This object handles
 * its own clipping, separate from vGfx.
 */
{
    struct hvGfxPane *next;  /* Next pane */
    int id;                  /* numeric id used to find a pane in the list */
    struct hvGfx *hvg;       /* containing hvGfx object */
    struct vGfx *vg;         /* virtual graphics object */
    boolean rev;             /* reverse display (negative strand) */
    int xOff;                /* X offset into image */
    int width, height;       /* Virtual pixel dimensions. */
    int clipMinX;            /* X clipping region */
    int clipMaxX;
    int clipMinY;
    int clipMaxY;
};


struct hvGfx
/* browser graphics interface */
{
    struct hvGfx *next;	     /* Next in list. */
    struct vGfx *vg;         /* virtual graphics object */
    boolean rev;             /* reverse display (negative strand) */
    struct hvGfxPane *panes; /* list of panes */
};

struct hvGfx *hvGfxOpenGif(int width, int height, char *fileName);
/* Open up something that we'll write out as a gif someday. */

struct hvGfx *hvGfxOpenPostScript(int width, int height, char *fileName);
/* Open up something that will someday be a PostScript file. */

void hvGfxClose(struct hvGfx **pHvg);
/* Close down virtual graphics object, and finish writing it to file. */

INLINE int hvGfxPaneAdjX(struct hvGfxPane *hvgp, int x)
/* return x, handling reverse mode, or -1 if outside of cliping region */
{
// >= need due to [0..n) clipping box
if ((x < hvgp->clipMinX) || (x >= hvgp->clipMaxX))
    return -1;
else
    {
    if (hvgp->rev)
        x = hvgp->width - (x+1);
    return x;
    }
}

INLINE int hvGfxPaneAdjXX(struct hvGfxPane *hvgp, int x1, int *x2Ptr)
/* return X pixel start location and update end location, handling clipping
 * and reverse mode.  -1 is return if all out of range */
{
int x2 = *x2Ptr;
x1 = min(x1, hvgp->clipMinX);
x2 = max(x2, hvgp->clipMaxX);
if (x1 >= x2)
    return -1;
else
    {
    if (hvgp->rev)
        {
        x1 = hvgp->width - x2;
        x2 = hvgp->width - x1;
        }
    *x2Ptr = x2;
    return x1;
    }
}

INLINE int hvGfxPaneAdjXW(struct hvGfxPane *hvgp, int x, int *widthPtr)
/* return X pixel start location and update width for a range, handling *
 * clipping and reverse-complement mode.  -1 is return if all out of range */
{
int x2 = x + *widthPtr;
x = hvGfxPaneAdjXX(hvgp, x, &x2);
if (x < 0)
    return -1;
*widthPtr = x2 - x;
return x;
}

INLINE int hvGfxPaneClipY(struct hvGfxPane *hvgp, int y)
/* clip Y to be in cliping region bounds, return  -1 if out of bounds */
{
// >= need due to [0..n) clipping box
if ((y < hvgp->clipMinY) || (y >= hvgp->clipMaxY))
    return -1;
else
    return y;
}

INLINE int hvGfxPaneClipYY(struct hvGfxPane *hvgp, int y1, int *y2Ptr)
/* clip Y range to be in cliping region bounds, return -1 if out of bounds */
{
int y2 = *y2Ptr;
y1 = min(y1, hvgp->clipMinY);
y2 = max(y2, hvgp->clipMaxY);
if (y1 >= y2)
    return -1;
else
    {
    *y2Ptr = y2;
    return y1;
    }
}

INLINE int hvGfxPaneClipYH(struct hvGfxPane *hvgp, int y, int *heightPtr)
/* clip Y and height to be in cliping region bounds, return -1 if out of
 * bounds */
{
int y2 = y + *heightPtr;
y = hvGfxPaneClipYY(hvgp, y, &y2);
if (y < 0)
    return -1;
else
    {
    *heightPtr = y2 - y;
    return y;
    }
}

INLINE void hvGfxPaneDot(struct hvGfxPane *hvgp, int x, int y, int colorIx)
/* Draw a single pixel.  Try to work at a higher level when possible! */
{
x = hvGfxPaneAdjX(hvgp, x);
y = hvGfxPaneClipY(hvgp, y);
if ((x >= 0) && (y >= 0))
    vgDot(hvgp->vg, x, y, colorIx);
}

INLINE void hvGfxPaneBox(struct hvGfxPane *hvgp, int x, int y, 
                  int width, int height, int colorIx)
/* Draw a box. */
{
x = hvGfxPaneAdjXW(hvgp, x, &width);
y = hvGfxPaneClipYH(hvgp, x, &height);
if ((x >= 0) && (y >= 0))
    vgBox(hvgp->vg, x, y, width, height, colorIx);
}

INLINE void hvGfxPaneLine(struct hvGfxPane *hvgp, 
                   int x1, int y1, int x2, int y2, int colorIx)
/* Draw a line from one point to another. */
{
x1  = hvGfxPaneAdjXX(hvgp, x1, &x2);
y1  = hvGfxPaneClipYY(hvgp, y1, &y2);
if ((x1 >= 0) && (y1 >= 0))
    vgLine(hvgp->vg, x1, y1, x2, y2, colorIx);
}

INLINE void hvGfxPaneText(struct hvGfxPane *hvgp, int x, int y, int colorIx,
                   void *font, char *text)
/* Draw a line of text with upper left corner x,y. */
{
x  = hvGfxPaneAdjX(hvgp, x);
y  = hvGfxPaneClipY(hvgp, y);
if ((x >= 0) && (y >= 0))
    vgText(hvgp->vg, x, y, colorIx, font, text);
}

INLINE void hvGfxPaneTextRight(struct hvGfxPane *hvgp, int x, int y, int width, int height,
                      int colorIx, void *font, char *text)
/* Draw a line of text with upper right corner x,y. */
{
x = hvGfxPaneAdjXW(hvgp, x, &width);
y = hvGfxPaneClipYH(hvgp, y, &height);
if ((x >= 0) && (y >= 0))
    vgTextRight(hvgp->vg, x, y, width, height, colorIx, font, text);
}

INLINE void hvGfxPaneTextCentered(struct hvGfxPane *hvgp, int x, int y, int width, int height,
                           int colorIx, void *font, char *text)
/* Draw a line of text in middle of box. */
{
x = hvGfxPaneAdjXW(hvgp, x, &width);
y = hvGfxPaneClipYH(hvgp, y, &height);
if ((x >= 0) && (y >= 0))
    vgTextCentered(hvgp->vg, x, y, width, height, colorIx, font, text);
}

INLINE void hvGfxPaneSetClip(struct hvGfxPane *hvgp, int x, int y, int width, int height)
/* Set clipping rectangle. */
{
// this should not be adjusted for RC
hvgp->clipMinX = x;
hvgp->clipMaxX = x + width;
hvgp->clipMinY = y;
hvgp->clipMaxY = y + height;
// build is reversed in actual vGfx
if (hvgp->rev)
    x = hvgp->width - (x + width);
vgSetClip(hvgp->vg, x, y, width, height);
}

INLINE void hvGfxPaneUnclip(struct hvGfxPane *hvgp)
/* Set clipping rect cover full thing. */
{
hvgp->clipMinX = 0;
hvgp->clipMaxX = hvgp->width;
hvgp->clipMinY = 0;
hvgp->clipMaxY = hvgp->height;
vgUnclip(hvgp->vg);
}

INLINE void hvGfxPaneVerticalSmear(struct hvGfxPane *hvgp,
                                   int xOff, int yOff, int width, int height, 
                                   unsigned char *dots, boolean zeroClear)
/* Put a series of one 'pixel' width vertical lines. */
{
xOff = hvGfxPaneAdjXW(hvgp, xOff, &width);
yOff = hvGfxPaneClipYH(hvgp, yOff, &height);
if ((xOff >= 0) && (yOff >= 0))
    vgVerticalSmear(hvgp->vg, xOff, yOff, width, height, dots, zeroClear);
}

INLINE void hvGfxPaneFillUnder(struct hvGfxPane *hvgp, int x1, int y1, int x2, int y2, 
                        int bottom, Color color)
/* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
 * it's top, a horizontal line at bottom at it's bottom,  and
 * vertical lines from the bottom to y1 on the left and bottom to
 * y2 on the right. */
{
x1  = hvGfxPaneAdjXX(hvgp, x1, &x2);
y1  = hvGfxPaneClipYY(hvgp, y1, &y2);
if ((x1 >= 0) && (y2 >= 0))
    vgFillUnder(hvgp->vg, x1, y1, x2, y2, bottom, color);
}

INLINE void hvGfxPaneRevPoly(struct hvGfxPane *hvgp, struct gfxPoly *poly)
/* reverse coordinates in polygons */
{
struct gfxPoint *pt = poly->ptList;
int i;
for (i = 0; i < poly->ptCount; i++, pt = pt->next)
    pt->x = hvGfxPaneAdjX(hvgp, pt->x);
}

INLINE void hvGfxPaneDrawPoly(struct hvGfxPane *hvgp, struct gfxPoly *poly, Color color, 
                       boolean filled)
/* Draw polygon, possibly filled in color. */
{
if (hvgp->rev)
    hvGfxPaneRevPoly(hvgp, poly);
vgDrawPoly(hvgp->vg, poly, color, filled);
if (hvgp->rev)
    hvGfxPaneRevPoly(hvgp, poly);  // restore
}

INLINE int hvGfxPaneFindColorIx(struct hvGfxPane *hvgp, int r, int g, int b)
/* Find color in map if possible, otherwise create new color or
 * in a pinch a close color. */
{
return vgFindColorIx(hvgp->vg, r, g, b);
}

INLINE struct rgbColor hvGfxPolyColorIxToRgb(struct hvGfxPane *hvgp, int colorIx)
/* Return rgb values for given color index. */
{
return vgColorIxToRgb(hvgp->vg, colorIx);
};

INLINE void hvGfxPaneSetHint(struct hvGfxPane *hvgp, char *hint, char *value)
/* Set hint */
{
vgSetHint(hvgp->vg,hint,value);
}

INLINE char* hvGfxPaneGetHint(struct hvGfxPane *hvgp, char *hint)
/* Get hint */
{
return vgGetHint(hvgp->vg, hint);
}

INLINE int hvGfxPaneFindRgb(struct hvGfxPane *hvgp, struct rgbColor *rgb)
/* Find color index corresponding to rgb color. */
{
return vgFindRgb(hvgp->vg, rgb);
}

INLINE Color hvGfxPaneContrastingColor(struct hvGfxPane *hvgp, int backgroundIx)
/* Return black or white whichever would be more visible over
 * background. */
{
return vgContrastingColor(hvgp->vg, backgroundIx);
}

void hvGfxPaneMakeColorGradient(struct hvGfxPane *hvgp, 
                            struct rgbColor *start, struct rgbColor *end,
                            int steps, Color *colorIxs);
/* Make a color gradient that goes smoothly from start to end colors in given
 * number of steps.  Put indices in color table in colorIxs */

void hvGfxPaneBarbedHorizontalLine(struct hvGfxPane *hvgp, int x, int y, 
	int width, int barbHeight, int barbSpacing, int barbDir, Color color,
	boolean needDrawMiddle);
/* Draw a horizontal line starting at xOff, yOff of given width.  Will
 * put barbs (successive arrowheads) to indicate direction of line.  
 * BarbDir of 1 points barbs to right, of -1 points them to left. */

void hvGfxPaneDrawRulerBumpText(struct hvGfxPane *hvgp, int xOff, int yOff, 
	int height, int width,
        Color color, MgFont *font,
        int startNum, int range, int bumpX, int bumpY);
/* Draw a ruler inside the indicated part of mg with numbers that start at
 * startNum and span range.  Bump text positions slightly. */

void hvGfxPaneDrawRuler(struct hvGfxPane *hvgp, int xOff, int yOff, int height, int width,
        Color color, MgFont *font,
        int startNum, int range);
/* Draw a ruler inside the indicated part of mg with numbers that start at
 * startNum and span range.  */

void hvGfxPaneNextItemButton(struct hvGfxPane *hvgp, int x, int y, int w, int h, 
		      Color color, Color hvgColor, boolean nextItem);
/* Draw a button that looks like a fast-forward or rewind button on */
/* a remote control. If nextItem is TRUE, it points right, otherwise */
/* left. color is the outline color, and hvgColor is the fill color. */

#endif 
