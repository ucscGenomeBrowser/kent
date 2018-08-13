/* pscmGfx - routines for making postScript output seem a
 * lot like 256 color bitmap output. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef PSCMGFX_H
#define PSCMGFX_H

#include "memgfx.h"

struct pscmGfx 
/* Structure to simululate 256 color image
 * in postScript - so to make it easier to
 * swap between memGfx/gif output and PostScript
 * output */
    {
    struct pscmGfx *next;
    struct psGfx *ps;	  /* Underlying postScript object. */
    int colAlloc;	  /* Colors allocated. */
    int colUsed;	  /* Colors used. */
    void *curFont;	  /* Current font. */
    int curColor;	  /* Current color. */
    struct colHash *colorHash;	/* Hash for fast look up of color. */
    struct rgbColor colorMap[256]; /* The color map. */
    int colorsUsed;		/* Number of colors actually used. */
    int clipMinX, clipMaxX;     /* Clipping region upper left corner. */
    int clipMinY, clipMaxY;     /* lower right, not inclusive */
    struct hash *hints;   /* Hints to guide behavior */
    int writeMode;        /* current write mode */
    };

struct pscmGfx *pscmOpen(int width, int height, char *file);
/* Return new pscmGfx. */

void pscmClose(struct pscmGfx **ppscm);
/* Finish writing out and free structure. */

void pscmSetClip(struct pscmGfx *pscm, int x, int y, int width, int height);
/* Set clipping rectangle. */

void pscmUnclip(struct pscmGfx *pscm);
/* Set clipping rect cover full thing. */

int pscmFindColorIx(struct pscmGfx *pscm, int r, int g, int b);
/* Find color index for rgb. */

void pscmSetColor(struct pscmGfx *pscm, Color color);
/* Set current color to Color. */

void pscmBox(struct pscmGfx *pscm, int x, int y, 
	int width, int height, int colorIx);
/* Draw a box. */

void pscmLine(struct pscmGfx *pscm, 
	int x1, int y1, int x2, int y2, int colorIx);
/* Draw a line from one point to another. */

void pscmText(struct pscmGfx *pscm, int x, int y, int colorIx, 
	MgFont *font, char *text);
/* Draw a line of text with upper left corner x,y. */

void pscmTextRight(struct pscmGfx *pscm, int x, int y, int width, int height,
	int color, MgFont *font, char *text);
/* Draw a line of text right justified in box defined by x/y/width/height */

void pscmTextCentered(struct pscmGfx *pscm, int x, int y, 
	int width, int height, int color, MgFont *font, char *text);
/* Draw a line of text centered in box defined by x/y/width/height */

void pscmEllipse(struct pscmGfx *pscm, int x1, int y1, int x2, int y2, Color color, 
                        int mode, boolean isDashed);
/* Draw an ellipse specified as a rectangle. Args are left-most and top-most points.
 * Optionally draw half-ellipse (top or bottom) */

void pscmCurve(struct pscmGfx *pscm, int x1, int y1, int x2, int y2, int x3, int y3, Color color,
                        boolean isDashed);
/* Draw Bezier curve specified by 3 points (quadratic Bezier).
 * The points are: first (p1) and last (p3), and 1 control points (p2). */

#endif /* PSCMGFX_H */
