/* pscmGfx - routines for making postScript output seem a
 * lot like 256 color bitmap output. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "memgfx.h"
#include "psGfx.h"
#include "pscmGfx.h"


void pscmSetClip(struct pscmGfx *pscm, int x, int y, int width, int height)
/* Set clipping rectangle. */
{
psPushClipRect(pscm->ps, x, y, width, height);
}

void pscmUnclip(struct pscmGfx *pscm)
/* Set clipping rect cover full thing. */
{
psPopG(pscm->ps);
}

int pscmAddColor(struct pscmGfx *pscm, int r, int g, int b)
/* Add color to map and return index. */
{
int colIx;
struct rgbColor *col;
if (pscm->colAlloc >= pscm->colUsed);
    {
    int oldAlloc = pscm->colAlloc;
    pscm->colAlloc *= 2;
    pscm->colMap = ExpandArray(pscm->colMap, oldAlloc, pscm->colAlloc);
    }
colIx = pscm->colUsed++;
col = pscm->colMap + colIx;
col->r = r;
col->g = g;
col->b = b;
return colIx;
}

static void pscmSetDefaultColorMap(struct pscmGfx *pscm)
/* Set up default color map for a memGfx. */
{
/* Note dependency in order here and in MG_WHITE, MG_BLACK, etc. */
int i;
for (i=0; i<ArraySize(mgFixedColors); ++i)
    {
    struct rgbColor *c = &mgFixedColors[i];
    pscmAddColor(pscm, c->r, c->g, c->b);
    }
}

struct pscmGfx *pscmNew(int width, int height, char *file)
/* Return new pscmGfx. */
{
struct pscmGfx *pscm;
int i;

AllocVar(pscm);
pscm->ps = psOpen(file, width, height, 72.0 * 7.5, 0, 0);
pscm->colAlloc = 8;
AllocArray(pscm->colMap, pscm->colAlloc);
pscmSetDefaultColorMap(pscm);
return pscm;
}

void pscmFree(struct pscmGfx **ppscm)
/* Free up structure. */
{
struct pscmGfx *pscm = *ppscm;
if (pscm != NULL)
    {
    freez(&pscm->colMap);
    psClose(&pscm->ps);
    freez(ppscm);
    }
*ppscm = NULL;
}

void pscmClearPixels(struct pscmGfx *pscm)
/* Set all pixels to background. */
{
}

Color pscmFindColor(struct pscmGfx *pscm, 
	unsigned char r, unsigned char g, unsigned char b)
/* Returns closest color in color map to rgb values.  If it doesn't
 * already exist in color map and there's room, it will create
 * exact color in map.  For a pscm it turns out there's always
 * room. */
{
return pscmAddColor(pscm, r, g, b);
}

Color pscmClosestColor(struct pscmGfx *pscm, 
	unsigned char r, unsigned char g, unsigned char b)
/* Returns closest color in color map to r,g,b */
{
return pscmAddColor(pscm, r, g, b);
}

static void pscmSetColor(struct pscmGfx *pscm, Color color)
/* Set color */
{
struct rgbColor *col = &pscm->colMap[color];
psSetColor(pscm->ps, col->r, col->g, col->b);
}

static void pscmVerticalSmear(struct pscmGfx *pscm,
	int xOff, int yOff, int width, int height, 
	Color *dots, boolean zeroClear)
/* Put a series of one 'pixel' width vertical lines. */
{
int x, i;
struct psGfx *ps = pscm->ps;
struct rgbColor *col;
Color c;
for (i=0; i<width; ++i)
    {
    x = xOff + i;
    c = dots[i];
    if (c != 0)
	{
	pscmSetColor(pscm, c);
	psDrawBox(ps, x, yOff, 1, height);
	}
    }
}

void pscmDrawBox(struct pscmGfx *pscm, int x, int y, 
	int width, int height, Color color)
/* Draw a box. */
{
pscmSetColor(pscm, color);
psDrawBox(pscm->ps, x, y, width, height);
}

void pscmDrawHorizontalLine(struct pscmGfx *pscm, int y1, Color color)
/* special case of pscmDrawLine, for horizontal line across entire window 
 * at y-value y1.*/
{
pscmDrawBox(pscm, 0, y1, pscm->ps->pixWidth, 1, color);
}

void pscmDrawLine(struct pscmGfx *pscm, 
	int x1, int y1, int x2, int y2, Color color)
/* Draw a line from one point to another. */
{
pscmSetColor(pscm, color);
psDrawLine(pscm->ps, x1, y1, x2, y2);
}

static void pscmSetFont(struct pscmGfx *pscm, MgFont *font)
/* Set font. */
{
// ~~~ fill this in later.
}

void pscmText(struct pscmGfx *pscm, int x, int y, Color color, 
	MgFont *font, char *text)
/* Draw a line of text with upper left corner x,y. */
{
pscmSetColor(pscm, color);
pscmSetFont(pscm, font);
psTextAt(pscm->ps, x, y, text);
}

void pscmTextCentered(struct pscmGfx *pscm, int x, int y, int width, int height, 
	Color color, MgFont *font, char *text)
/* Draw a line of text centered in box defined by x/y/width/height */
{
pscmText(pscm, x, y, color, font, text);	// ~~~ fill this in later.
}

void pscmTextRight(struct pscmGfx *pscm, int x, int y, int width, int height, 
	Color color, MgFont *font, char *text)
/* Draw a line of text right justified in box defined by x/y/width/height */
{
pscmText(pscm, x, y, color, font, text);	// ~~~ fill this in later.
}

int pscmFontPixelHeight(MgFont *font)
/* How high in pixels is font? */
{
return 7;	// ~~~ fill this in later 
}

int pscmFontLineHeight(MgFont *font)
/* How many pixels to next line ideally? */
{
return 8;	// ~~~ fill this in later 
}

int pscmFontWidth(MgFont *font, char *chars, int charCount)
/* How wide are a couple of letters? */
{
return charCount * 5;  // ~~~ fill this in later
}

int pscmFontStringWidth(MgFont *font, char *string)
/* How wide is a string? */
{
return pscmFontWidth(font, string, strlen(string));
}

int pscmFontCharWidth(MgFont *font, char c)
/* How wide is a character? */
{
return pscmFontWidth(font, &c, 1);
}

