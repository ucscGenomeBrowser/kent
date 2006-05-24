/* Browser graphics - stuff for drawing graphics displays that
 * are reasonably browser (human and intronerator) specific. */

#include "common.h"
#include "memgfx.h"
#include "vGfx.h"
#include "browserGfx.h"

static char const rcsid[] = "$Id: browserGfx.c,v 1.8 2006/05/08 09:43:31 aamp Exp $";


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


void vgDrawRulerBumpText(struct vGfx *vg, int xOff, int yOff, 
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
    vgBox(vg, x, yOff, 1, height, color);
    if (x - numWid >= xOff)
        {
        vgTextCentered(vg, x-numWid + bumpX, yOff + bumpY, numWid, 
	    height, color, font, tbuf);
        }
    }
}

void vgDrawRuler(struct vGfx *vg, int xOff, int yOff, int height, int width,
        Color color, MgFont *font,
        int startNum, int range)
/* Draw a ruler inside the indicated part of mg with numbers that start at
 * startNum and span range.  */
{
vgDrawRulerBumpText(vg, xOff, yOff, height, width, color, font,
    startNum, range, 0, 0);
}


void vgBarbedHorizontalLine(struct vGfx *vg, int x, int y, 
	int width, int barbHeight, int barbSpacing, int barbDir, Color color,
	boolean needDrawMiddle)
/* Draw a horizontal line starting at xOff, yOff of given width.  Will
 * put barbs (successive arrowheads) to indicate direction of line.  
 * BarbDir of 1 points barbs to right, of -1 points them to left. */
{
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
    vgLine(vg, x1, y, x2, yHi, color);
    vgLine(vg, x1, y, x2, yLo, color);
    }
}

void vgNextItemButton(struct vGfx *vg, int x, int y, int w, int h, 
		      Color color, Color bgColor, boolean nextItem)
/* Draw a button that looks like a fast-forward or rewind button on */
/* a remote control. If nextItem is TRUE, it points right, otherwise */
/* left. color is the outline color, and bgColor is the fill color. */
{
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
vgDrawPoly(vg, t1, bgColor, TRUE);
vgDrawPoly(vg, t2, bgColor, TRUE);
/* The two outline triangles. */
vgDrawPoly(vg, t1, color, FALSE);
vgDrawPoly(vg, t2, color, FALSE);
gfxPolyFree(&t1);
gfxPolyFree(&t2);
}
