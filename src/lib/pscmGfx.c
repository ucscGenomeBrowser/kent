/* pscmGfx - routines for making postScript output seem a
 * lot like 256 color bitmap output. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "memgfx.h"
#include "colHash.h"
#include "psGfx.h"
#include "pscmGfx.h"
#include "gemfont.h"
#include "vGfx.h"
#include "vGfxPrivate.h"


static struct pscmGfx *boxPscm;	 /* Used to keep from drawing the same box again
                                  * and again with no other calls between.  This
				  * ends up cutting down the file size by 5x
				  * in the whole chromosome case of the browser. */

void pscmSetClip(struct pscmGfx *pscm, int x, int y, int width, int height)
/* Set clipping rectangle. */
{
psPushClipRect(pscm->ps, x, y, width, height);
}

void pscmUnclip(struct pscmGfx *pscm)
/* Set clipping rect to cover full thing. */
{
/* This is one of the most painful parts of the interface.
 * Postscript can only cleanly clip/unclip via
 * gsave/grestores, which requires nesting.
 * Unfortunately the grestore will also likely
 * nuke our current font and color, so uncache it. */
psPopG(pscm->ps);
pscm->curFont = NULL;
pscm->curColor = -1;
}

static Color pscmClosestColor(struct pscmGfx *pscm, 
	unsigned char r, unsigned char g, unsigned char b)
/* Returns closest color in color map to r,g,b */
{
struct rgbColor *c = pscm->colorMap;
int closestDist = 0x7fffffff;
int closestIx = -1;
int dist, dif;
int i;
for (i=0; i<pscm->colorsUsed; ++i)
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
}

static Color pscmAddColor(struct pscmGfx *pscm, 
	unsigned char r, unsigned char g, unsigned char b)
/* Adds color to end of color map if there's room. */
{
int colIx = pscm->colorsUsed;
struct rgbColor *mapPos;
struct rgbColor *c = pscm->colorMap + pscm->colorsUsed;
c->r = r;
c->g = g;
c->b = b;
pscm->colorsUsed += 1;
colHashAdd(pscm->colorHash, r, g, b, colIx);;
return (Color)colIx;
}

int pscmFindColorIx(struct pscmGfx *pscm, int r, int g, int b)
/* Returns closest color in color map to rgb values.  If it doesn't
 * already exist in color map and there's room, it will create
 * exact color in map. */
{
struct colHashEl *che;
if ((che = colHashLookup(pscm->colorHash, r, g, b)) != NULL)
    return che->ix;
if (pscm->colorsUsed < 256)
    return pscmAddColor(pscm, r, g, b);
return pscmClosestColor(pscm, r, g, b);
}


struct rgbColor pscmColorIxToRgb(struct pscmGfx *pscm, int colorIx)
/* Return rgb value at color index. */
{
return pscm->colorMap[colorIx];
}

static void pscmSetDefaultColorMap(struct pscmGfx *pscm)
/* Set up default color map for a memGfx. */
{
/* Note dependency in order here and in MG_WHITE, MG_BLACK, etc. */
int i;
for (i=0; i<ArraySize(mgFixedColors); ++i)
    {
    struct rgbColor *c = &mgFixedColors[i];
    pscmFindColorIx(pscm, c->r, c->g, c->b);
    }
}

struct pscmGfx *pscmOpen(int width, int height, char *file)
/* Return new pscmGfx. */
{
struct pscmGfx *pscm;
int i;

AllocVar(pscm);
pscm->ps = psOpen(file, width, height, 72.0 * 7.5, 0, 0);
pscm->colorHash = colHashNew();
pscmSetDefaultColorMap(pscm);
pscmSetClip(pscm, 0, 0, width, height);
return pscm;
}

void pscmClose(struct pscmGfx **pPscm)
/* Finish writing out and free structure. */
{
struct pscmGfx *pscm = *pPscm;
if (pscm != NULL)
    {
    psClose(&pscm->ps);
    colHashFree(&pscm->colorHash);
    freez(pPscm);
    }
}

void pscmSetColor(struct pscmGfx *pscm, int colorIx)
/* Set color to index. */
{
struct rgbColor *col = pscm->colorMap + colorIx;
if (colorIx != pscm->curColor)
    {
    psSetColor(pscm->ps, col->r, col->g, col->b);
    pscm->curColor = colorIx;
    }
}

void pscmBox(struct pscmGfx *pscm, int x, int y, 
	int width, int height, int color)
/* Draw a box. */
{
/* When viewing whole chromosomes the browser tends
 * to draw the same little vertical tick over and
 * over again.  This tries to remove the worst of
 * the redundancy anyway. */
static int lx, ly, lw, lh, lc=-1;
if (x != lx || y != ly || width != lw || height != lh || color != lc || 
	pscm != boxPscm)
    {
    pscmSetColor(pscm, color);
    psDrawBox(pscm->ps, x, y, width, height);
    lx = x;
    ly = y;
    lw = width;
    lh = height;
    lc = color;
    boxPscm = pscm;
    }
}

void pscmDot(struct pscmGfx *pscm, int x, int y, int color)
/* Set a dot. */
{
pscmSetColor(pscm, color);
psDrawBox(pscm->ps, x, y, 1, 1);
}

void pscmLine(struct pscmGfx *pscm, 
	int x1, int y1, int x2, int y2, int color)
/* Draw a line from one point to another. */
{
pscmSetColor(pscm, color);
psDrawLine(pscm->ps, x1+0.5, y1+0.5, x2+0.5, y2+0.5);
boxPscm = NULL;
}

static void pscmVerticalSmear(struct pscmGfx *pscm,
	int xOff, int yOff, int width, int height, 
	unsigned char *dots, boolean zeroClear)
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
    if (c != 0 || !zeroClear)
	{
	pscmSetColor(pscm, c);
	psDrawBox(ps, x, yOff, 1, height);
	}
    }
}

static void pscmSetFont(struct pscmGfx *pscm, MgFont *font)
/* Set font. */
{
/* For now we basically still get the sizing info from the old
 * gem fonts.   I'm not sure how to get sizing info out of
 * PostScript.  We'll try and arrange it so that the PostScript
 * fonts match the gem fonts more or less. */
void *v = font;
if (v != pscm->curFont)
    {
    psTimesFont(pscm->ps, font->frm_hgt);
    pscm->curFont = v;
    }
}

void pscmText(struct pscmGfx *pscm, int x, int y, int color, 
	MgFont *font, char *text)
/* Draw a line of text with upper left corner x,y. */
{
pscmSetColor(pscm, color);
pscmSetFont(pscm, font);
psTextAt(pscm->ps, x, y, text);
boxPscm = NULL;
}

void pscmTextRight(struct pscmGfx *pscm, int x, int y, int width, int height,
	int color, MgFont *font, char *text)
/* Draw a line of text right justified in box defined by x/y/width/height */
{
pscmSetColor(pscm, color);
pscmSetFont(pscm, font);
psTextRight(pscm->ps, x, y, width, height, text);
boxPscm = NULL;
}

void pscmTextCentered(struct pscmGfx *pscm, int x, int y, 
	int width, int height, int color, MgFont *font, char *text)
/* Draw a line of text centered in box defined by x/y/width/height */
{
pscmSetColor(pscm, color);
pscmSetFont(pscm, font);
psTextCentered(pscm->ps, x, y, width, height, text);
boxPscm = NULL;
}

void pscmFillUnder(struct pscmGfx *pscm, int x1, int y1, int x2, int y2, 
	int bottom, Color color)
/* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
 * it's top, a horizontal line at bottom at it's bottom,  and
 * vertical lines from the bottom to y1 on the left and bottom to
 * y2 on the right. */
{
pscmSetColor(pscm, color);
psFillUnder(pscm->ps, x1, y1, x2, y2, bottom);
boxPscm = NULL;
}

struct vGfx *vgOpenPostScript(int width, int height, char *fileName)
/* Open up something that will someday be a PostScript file. */
{
struct vGfx *vg = vgHalfInit(width, height);
vg->data = pscmOpen(width, height, fileName);
vg->close = (vg_close)pscmClose;
vg->dot = (vg_dot)pscmDot;
vg->box = (vg_box)pscmBox;
vg->line = (vg_line)pscmLine;
vg->text = (vg_text)pscmText;
vg->textRight = (vg_textRight)pscmTextRight;
vg->textCentered = (vg_textCentered)pscmTextCentered;
vg->findColorIx = (vg_findColorIx)pscmFindColorIx;
vg->colorIxToRgb = (vg_colorIxToRgb)pscmColorIxToRgb;
vg->setClip = (vg_setClip)pscmSetClip;
vg->unclip = (vg_unclip)pscmUnclip;
vg->verticalSmear = (vg_verticalSmear)pscmVerticalSmear;
vg->fillUnder = (vg_fillUnder)pscmFillUnder;
return vg;
}

