/* hvGfx - browser graphics interface.  This is a thin layer on top of vGfx
 * providing genome browser-specific features.  It was added to handle
 * reverse-complement display. */

#include "common.h"
#include "hvGfx.h"

static char const rcsid[] = "$Id: hvGfx.c,v 1.3 2008/03/07 18:47:41 markd Exp $";

static struct hvGfx *hvGfxAlloc(struct vGfx *vg)
/* allocate a hvgGfx object */
{
struct hvGfx *hvg;
AllocVar(hvg);
hvg->vg = vg;
hvg->pixelBased = vg->pixelBased;
hvg->width = vg->width;
hvg->height = vg->height;
hvg->clipMaxX = vg->width;
hvg->clipMaxY = vg->height;
return hvg;
}

struct hvGfx *hvGfxOpenGif(int width, int height, char *fileName)
/* Open up something that we'll write out as a gif someday. */
{
return hvGfxAlloc(vgOpenGif(width, height, fileName));
}

struct hvGfx *hvGfxOpenPostScript(int width, int height, char *fileName)
/* Open up something that will someday be a PostScript file. */
{
return hvGfxAlloc(vgOpenPostScript(width, height, fileName));
}

void hvGfxClose(struct hvGfx **pHvg)
/* Close down virtual graphics object, and finish writing it to file. */
{
struct hvGfx *hvg = *pHvg;
if (hvg != NULL)
    {
    vgClose(&hvg->vg);
    freez(pHvg);
    }
}

void hvGfxMakeColorGradient(struct hvGfx *hvg, 
                            struct rgbColor *start, struct rgbColor *end,
                            int steps, Color *colorIxs)
/* Make a color gradient that goes smoothly from start to end colors in given
 * number of steps.  Put indices in color table in colorIxs */
{
double scale = 0, invScale;
double invStep;
int i;
int r,g,b;

steps -= 1;	/* Easier to do the calculation in an inclusive way. */
invStep = 1.0/steps;
for (i=0; i<=steps; ++i)
    {
    invScale = 1.0 - scale;
    r = invScale * start->r + scale * end->r;
    g = invScale * start->g + scale * end->g;
    b = invScale * start->b + scale * end->b;
    colorIxs[i] = hvGfxFindColorIx(hvg, r, g, b);
    scale += invStep;
    }
}

static long figureTickSpan(long totalLength, int maxNumTicks)
/* Figure out whether ticks on ruler should be 1, 5, 10, 50, 100, 500, 
 * 1000, etc. units apart.  */
{
int roughTickLen = totalLength/maxNumTicks;
int i;
int tickLen = 1;

for (i=0; i<9; ++i)
    {
    if (roughTickLen < tickLen)
    	return tickLen;
    tickLen *= 5;
    if (roughTickLen < tickLen)
	return tickLen;
    tickLen *= 2;
    }
return 1000000000;
}

static void numLabelString(int num, char *label)
/* Returns a numerical labeling string. */
{
char *sign = "";
if (num < 0)
    {
    num = -num;
    sign = "-";
    }
sprintf(label, "%s%d", sign, num);
}

void hvGfxDrawRulerBumpText(struct hvGfx *hvg, int xOff, int yOff, 
	int height, int width,
        Color color, MgFont *font,
        int startNum, int range, int bumpX, int bumpY)
/* Draw a ruler inside the indicated part of mg with numbers that start at
 * startNum and span range.  Bump text positions slightly. */
{
int tickSpan;
int tickPos;
double scale;
int firstTick;
int remainder;
int end = startNum + range;
int x;
char tbuf[14];
int numWid;
int goodNumTicks;
int niceNumTicks = width/35;

numLabelString(startNum+range, tbuf);
numWid = mgFontStringWidth(font, tbuf)+4+bumpX;
goodNumTicks = width/numWid;
if (goodNumTicks < 1) goodNumTicks = 1;
if (goodNumTicks > niceNumTicks) goodNumTicks = niceNumTicks;

tickSpan = figureTickSpan(range, goodNumTicks);

scale = (double)width / range;

firstTick = startNum + tickSpan;
remainder = firstTick % tickSpan;
firstTick -= remainder;
for (tickPos=firstTick; tickPos<end; tickPos += tickSpan)
    {
    numLabelString(tickPos, tbuf);
    numWid = mgFontStringWidth(font, tbuf)+4;
    x = (int)((tickPos-startNum) * scale) + xOff;
    hvGfxBox(hvg, x, yOff, 1, height, color);
    if (x - numWid >= xOff)
        {
        hvGfxTextCentered(hvg, x-numWid + bumpX, yOff + bumpY, numWid, 
                          height, color, font, tbuf);
        }
    }
}

void hvGfxDrawRuler(struct hvGfx *hvg, int xOff, int yOff, int height, int width,
        Color color, MgFont *font,
        int startNum, int range)
/* Draw a ruler inside the indicated part of mg with numbers that start at
 * startNum and span range.  */
{
hvGfxDrawRulerBumpText(hvg, xOff, yOff, height, width, color, font,
                       startNum, range, 0, 0);
}


void hvGfxBarbedHorizontalLine(struct hvGfx *hvg, int x, int y, 
	int width, int barbHeight, int barbSpacing, int barbDir, Color color,
	boolean needDrawMiddle)
/* Draw a horizontal line starting at xOff, yOff of given width.  Will
 * put barbs (successive arrowheads) to indicate direction of line.  
 * BarbDir of 1 points barbs to right, of -1 points them to left. */
{
barbDir = (hvg->rc) ? -barbDir : barbDir;
if (barbDir == 0)
    return;  // fully clipped, or no barbs
x = hvGfxAdjXW(hvg, x, width);
int x1, x2;
int yHi, yLo;
int offset, startOffset, endOffset, barbAdd;
int scrOff = (barbSpacing - 1) - (x % (barbSpacing));

yHi = y + barbHeight;
yLo = y - barbHeight;
if (barbDir < 0)
    {
    startOffset = scrOff - barbHeight;
    startOffset = (startOffset >= 0) ?startOffset : 0;
    endOffset = width - barbHeight;
    barbAdd = barbHeight;
    }
else
    {
    startOffset = scrOff + barbHeight;
    endOffset = width;
    barbAdd = -barbHeight;
    }

for (offset = startOffset; offset < endOffset; offset += barbSpacing)
    {
    x1 = x + offset;
    x2 = x1 + barbAdd;
    vgLine(hvg->vg, x1, y, x2, yHi, color);
    vgLine(hvg->vg, x1, y, x2, yLo, color);
    }
}

void hvGfxNextItemButton(struct hvGfx *hvg, int x, int y, int w, int h, 
		      Color color, Color hvgColor, boolean nextItem)
/* Draw a button that looks like a fast-forward or rewind button on */
/* a remote control. If nextItem is TRUE, it points right, otherwise */
/* left. color is the outline color, and hvgColor is the fill color. */
{
x = hvGfxAdjXW(hvg, x, w);
if (hvg->rc)
    nextItem = !nextItem;

struct gfxPoly *t1, *t2;
/* Make the triangles */
t1 = gfxPolyNew();
t2 = gfxPolyNew();
if (nextItem)
    /* point right. */
    {
    gfxPolyAddPoint(t1, x, y);
    gfxPolyAddPoint(t1, x+w/2, y+h/2);
    gfxPolyAddPoint(t1, x, y+h);
    gfxPolyAddPoint(t2, x+w/2, y);
    gfxPolyAddPoint(t2, x+w, y+h/2);
    gfxPolyAddPoint(t2, x+w/2, y+h);    
    }
else
    /* point left. */
    {
    gfxPolyAddPoint(t1, x, y+h/2);
    gfxPolyAddPoint(t1, x+w/2, y);
    gfxPolyAddPoint(t1, x+w/2, y+h);
    gfxPolyAddPoint(t2, x+w/2, y+h/2);
    gfxPolyAddPoint(t2, x+w, y);
    gfxPolyAddPoint(t2, x+w, y+h);
    }
/* The two filled triangles. */
vgDrawPoly(hvg->vg, t1, hvgColor, TRUE);
vgDrawPoly(hvg->vg, t2, hvgColor, TRUE);
/* The two outline triangles. */
vgDrawPoly(hvg->vg, t1, color, FALSE);
vgDrawPoly(hvg->vg, t2, color, FALSE);
gfxPolyFree(&t1);
gfxPolyFree(&t2);
}


