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
    };

struct pscmGfx *pscmNew(int width, int height, char *file);
/* Return new pscmGfx. */

void pscmFree(struct pscmGfx **ppscm);
/* Free up structure. */

void pscmClearPixels(struct pscmGfx *pscm);
/* Set all pixels to background. */

void pscmSetClip(struct pscmGfx *pscm, int x, int y, int width, int height);
/* Set clipping rectangle. */

void pscmUnclip(struct pscmGfx *pscm);
/* Set clipping rect cover full thing. */

int pscmAddColor(struct pscmGfx *pscm, int r, int g, int b);
/* Add color to map and return index. */

Color pscmClosestColor(struct pscmGfx *pscm, 
	unsigned char r, unsigned char g, unsigned char b);
/* Returns closest color in color map to r,g,b */

static void pscmVerticalSmear(struct pscmGfx *pscm,
	int xOff, int yOff, int width, int height, 
	Color *dots, boolean zeroClear);
/* Put a series of one 'pixel' width vertical lines. */

void pscmDrawBox(struct pscmGfx *pscm, int x, int y, 
	int width, int height, Color color);
/* Draw a box. */

void pscmDrawHorizontalLine(struct pscmGfx *pscm, int y1, Color color);
/* special case of pscmDrawLine, for horizontal line across entire window 
 * at y-value y1.*/

void pscmDrawLine(struct pscmGfx *pscm, 
	int x1, int y1, int x2, int y2, Color color);
/* Draw a line from one point to another. */

void pscmText(struct pscmGfx *pscm, int x, int y, Color color, 
	MgFont *font, char *text);
/* Draw a line of text with upper left corner x,y. */

void pscmTextCentered(struct pscmGfx *pscm, int x, int y, int width, int height, 
	Color color, MgFont *font, char *text);
/* Draw a line of text centered in box defined by x/y/width/height */

void pscmTextRight(struct pscmGfx *pscm, int x, int y, int width, int height, 
	Color color, MgFont *font, char *text);
/* Draw a line of text right justified in box defined by x/y/width/height */

int pscmFontPixelHeight(MgFont *font);
/* How high in pixels is font? */

int pscmFontLineHeight(MgFont *font);
/* How many pixels to next line ideally? */

int pscmFontWidth(MgFont *font, char *chars, int charCount);
/* How wide are a couple of letters? */

int pscmFontStringWidth(MgFont *font, char *string);
/* How wide is a string? */

int pscmFontCharWidth(MgFont *font, char c);
/* How wide is a character? */

#endif /* PSCMGFX_H */
