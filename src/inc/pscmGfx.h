/* pscmGfx - routines for making postScript output seem a
 * lot like 256 color bitmap output. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef PSCMGFX_H
#define PSCMGFX_H

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
    struct rgbColor *colMap; /* Colors. */
    void *curFont;	  /* Current font. */
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

static void pscmVerticalSmear(struct pscmGfx *pscm,
	int xOff, int yOff, int width, int height, 
	Color *dots, boolean zeroClear);
/* Put a series of one 'pixel' width vertical lines. */

void pscmBox(struct pscmGfx *pscm, int x, int y, 
	int width, int height, int colorIx);
/* Draw a box. */

void pscmLine(struct pscmGfx *pscm, 
	int x1, int y1, int x2, int y2, int colorIx);
/* Draw a line from one point to another. */

void pscmText(struct pscmGfx *pscm, int x, int y, int colorIx, 
	MgFont *font, char *text);
/* Draw a line of text with upper left corner x,y. */


#endif /* PSCMGFX_H */
