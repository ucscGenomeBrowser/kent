/* pscmGfx - routines for making postScript output seem a
 * lot like 256 color bitmap output. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "memgfx.h"
#include "psGfx.h"
#include "pscmGfx.h"
#include "gemfont.h"
#include "vGfx.h"
#include "vGfxPrivate.h"


void pscmSetClip(struct pscmGfx *pscm, int x, int y, int width, int height)
/* Set clipping rectangle. */
{
psPushClipRect(pscm->ps, x, y, width, height);
}

void pscmUnclip(struct pscmGfx *pscm)
/* Set clipping rect to cover full thing. */
{
psPopG(pscm->ps);
}

int pscmFindColorIx(struct pscmGfx *pscm, int r, int g, int b)
/* Find color index for rgb. */
{
/* This guy always returns a new color index.  We probably
 * should borrow the color hashing scheme from memGfx,
 * but it's so absurd really, since PostScript is really
 * not color mapped in the first place... */
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
pscm->colAlloc = 8;
AllocArray(pscm->colMap, pscm->colAlloc);
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
    freez(&pscm->colMap);
    psClose(&pscm->ps);
    freez(pPscm);
    }
}

void pscmSetColor(struct pscmGfx *pscm, int colorIx)
/* Set color to index. */
{
struct rgbColor *col = pscm->colMap + colorIx;
psSetColor(pscm->ps, col->r, col->g, col->b);
}

void pscmBox(struct pscmGfx *pscm, int x, int y, 
	int width, int height, int color)
/* Draw a box. */
{
pscmSetColor(pscm, color);
psDrawBox(pscm->ps, x, y, width, height);
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
psDrawLine(pscm->ps, x1, y1, x2, y2);
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
    if (c != 0)
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
    FILE *f = pscm->ps->f;
    fprintf(f, "/Times-Roman findfont ");
    fprintf(f, "%d scalefont setfont\n", font->frm_hgt);
    }
}

void pscmText(struct pscmGfx *pscm, int x, int y, int color, 
	MgFont *font, char *text)
/* Draw a line of text with upper left corner x,y. */
{
pscmSetColor(pscm, color);
pscmSetFont(pscm, font);
psTextAt(pscm->ps, x, y, text);
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
vg->findColorIx = (vg_findColorIx)pscmFindColorIx;
vg->setClip = (vg_setClip)pscmSetClip;
vg->unclip = (vg_unclip)pscmUnclip;
vg->verticalSmear = (vg_verticalSmear)(pscmVerticalSmear);
return vg;
}

