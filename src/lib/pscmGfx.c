/* pscmGfx - routines for making postScript output seem a
 * lot like 256 color bitmap output. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include <math.h>
#ifdef MACHTYPE_sparc
#include <ieeefp.h>
int isinf(double x) { return !finite(x) && x==x; }
#endif
#include "common.h"
#include "hash.h"
#include "memgfx.h"
#include "gfxPoly.h"
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

void pscmSetHint(struct pscmGfx *pscm, char *hint, char *value)
/* set a hint */
{
if (!value) return;
if (sameString(value,""))
    {
    hashRemove(pscm->hints, hint);
    }
struct hashEl *el = hashLookup(pscm->hints, hint);
if (el) 
    {
    freeMem(el->val);
    el->val = cloneString(value);
    }
else
    {
    hashAdd(pscm->hints, hint, cloneString(value));
    }
}

char *pscmGetHint(struct pscmGfx *pscm, char *hint)
/* get a hint */
{
return hashOptionalVal(pscm->hints, hint, "");
}

int pscmGetFontPixelHeight(struct pscmGfx *pscm, MgFont *font)
/* How high in pixels is font? */
{
return font_cel_height(font);
}

int pscmGetFontStringWidth(struct pscmGfx *pscm, MgFont *font, char *string)
/* How wide is a string? */
{
return fnstring_width(font, (unsigned char *)string, strlen(string));
}

void pscmSetClip(struct pscmGfx *pscm, int x, int y, int width, int height)
/* Set clipping rectangle. */
{
double x2 = x + width;
double y2 = y + height;
pscm->clipMinX = x;
pscm->clipMinY = y;
pscm->clipMaxX = x2;     /* one beyond actual last pixel */
pscm->clipMaxY = y2;
/* adjust to pixel-centered coordinates */
x2 -= 1;
y2 -= 1;
double x1 = x;
double y1 = y;
/* pad a half-pixel all the way around the box */
x1 -= 0.5;
y1 -= 0.5;
x2 += 0.5;
y2 += 0.5;
psClipRect(pscm->ps, x1, y1, x2-x1, y2-y1);
}

void pscmUnclip(struct pscmGfx *pscm)
/* Set clipping rect to cover full thing. */
{
pscmSetClip(pscm, 0, 0, pscm->ps->userWidth, pscm->ps->userHeight);
}

#ifndef COLOR32
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
struct rgbColor *c = pscm->colorMap + pscm->colorsUsed;
c->r = r;
c->g = g;
c->b = b;
pscm->colorsUsed += 1;
colHashAdd(pscm->colorHash, r, g, b, colIx);;
return (Color)colIx;
}
#endif

int pscmFindColorIx(struct pscmGfx *pscm, int r, int g, int b)
/* Returns closest color in color map to rgb values.  If it doesn't
 * already exist in color map and there's room, it will create
 * exact color in map. */
{
#ifdef COLOR32
return MAKECOLOR_32(r,g,b);
#else
struct colHashEl *che;
if (r>255||g>255||b>255) 
    errAbort("RGB values out of range (0-255).  r:%d g:%d b:%d", r, g, b);
if ((che = colHashLookup(pscm->colorHash, r, g, b)) != NULL)
    return che->ix;
if (pscm->colorsUsed < 256)
    return pscmAddColor(pscm, r, g, b);
return pscmClosestColor(pscm, r, g, b);
#endif
}


struct rgbColor pscmColorIxToRgb(struct pscmGfx *pscm, int colorIx)
/* Return rgb value at color index. */
{
#ifdef COLOR32
static struct rgbColor rgb;
rgb.r = (colorIx >> 0) & 0xff;
rgb.g = (colorIx >> 8) & 0xff;
rgb.b = (colorIx >> 16) & 0xff;

return rgb;
#else
return pscm->colorMap[colorIx];
#endif
}

#ifndef COLOR32
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
#endif

void pscmSetWriteMode(struct pscmGfx *pscm, unsigned int writeMode)
/* Set write mode */
{
pscm->writeMode = writeMode;
}

struct pscmGfx *pscmOpen(int width, int height, char *file)
/* Return new pscmGfx. */
{
struct pscmGfx *pscm;

AllocVar(pscm);
pscm->ps = psOpen(file, width, height, 72.0 * 7.5, 0, 0);
psTranslate(pscm->ps,0.5,0.5);  /* translate all coordinates to pixel centers */
#ifndef COLOR32
pscm->colorHash = colHashNew();
pscmSetDefaultColorMap(pscm);
#endif
pscm->clipMinX = pscm->clipMinY = 0;
pscm->clipMaxX = width;     
pscm->clipMaxY = height;
pscm->hints = hashNew(6);
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

void pscmSetColor(struct pscmGfx *pscm, Color color)
/* Set current color to Color. */
{
struct rgbColor *col;

#ifdef COLOR32
struct rgbColor myCol;
col = &myCol;

col->r = (color >> 0) & 0xff;
col->g = (color >> 8) & 0xff;
col->b = (color >> 16) & 0xff;
#else
col = pscm->colorMap + color;
#endif

if (color != pscm->curColor)
    {
    psSetColor(pscm->ps, col->r, col->g, col->b);
    pscm->curColor = color;
    }
}

void pscmBoxToPs(struct pscmGfx *pscm, int x, int y, 
	int width, int height)
/* adjust coordinates for PS */
{
/* Do some clipping here to make the postScript
 * easier to edit in illustrator. */
double x2 = x + width;
double y2 = y + height;

if (x < pscm->clipMinX) x = pscm->clipMinX;
if (y < pscm->clipMinY) y = pscm->clipMinY;
if (x2 > pscm->clipMaxX) x2 = pscm->clipMaxX;
if (y2 > pscm->clipMaxY) y2 = pscm->clipMaxY;

/* adjust to pixel-centered coordinates */
x2 -= 1;
y2 -= 1;
double x1 = x;
double y1 = y;
/* pad a half-pixel all the way around the box */
x1 -= 0.5;
y1 -= 0.5;
x2 += 0.5;
y2 += 0.5;
psDrawBox(pscm->ps, x1, y1, x2-x1, y2-y1);
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
    pscmBoxToPs(pscm, x, y, width, height);
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
pscmBox(pscm, x, y, 1, 1, color);
}



static void pscmVerticalSmear(struct pscmGfx *pscm,
	int xOff, int yOff, int width, int height, 
	Color *dots, boolean zeroClear)
/* Put a series of one 'pixel' width vertical lines. */
{
int x, i;
struct psGfx *ps = pscm->ps;
Color c;
for (i=0; i<width; ++i)
    {
    x = xOff + i;
    c = dots[i];
    if (c != MG_WHITE || !zeroClear)
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
    psTimesFont(pscm->ps, font->psHeight);
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

void psPolyFindExtremes(struct gfxPoly *poly, 
  int *pMinX, int *pMaxX,  
  int *pMinY, int *pMaxY)
/* find min and max of x and y */
{
struct gfxPoint *p = poly->ptList;
int minX, maxX, minY, maxY;
minX = maxX =  p->x;
minY = maxY =  p->y;
for (;;)
    {
    p = p->next;
    if (p == poly->ptList)
	break;
    if (minX > p->x) minX = p->x;
    if (minY > p->y) minY = p->y;
    if (maxX < p->x) maxX = p->x;
    if (maxY < p->y) maxY = p->y;
    }
*pMinX = minX;
*pMaxX = maxX;
*pMinY = minY;
*pMaxY = maxY;
}

double gfxSlope(struct gfxPoint *q, struct gfxPoint *p)
/* determine slope from two gfxPoints */
{
double dx = p->x - q->x;
double dy = p->y - q->y;
return dy/dx;
}

boolean colinearPoly(struct gfxPoly *poly)
/* determine if points are co-linear */
{
if (poly->ptCount < 2) return TRUE;
struct gfxPoint *q = poly->ptList, *p=q->next;
double m0 = gfxSlope(q,p);
for (;;)
    {
    p = p->next;
    if (p == poly->ptList)
	break;
    double m1 = gfxSlope(q,p);
    if (!(isinf(m0) && isinf(m1)) && (m1 != m0))
	return FALSE;
    }
return TRUE;
}

double fatPixel(double c, double center, double fat)
/* fatten coordinate by scaling */
{
return center+((c-center)*fat);
}


void pscmPolyFatten(struct psPoly *psPoly, 
  int minX, int maxX, int minY, int maxY)
/* Fatten polygon by finding the center and then
 * scaling by 0.5 pixel from the center in all directions.
 * Caller assures dx and dy will NOT be zero. */
{
int dx = maxX - minX;
int dy = maxY - minY;
double centerX = (maxX + minX) / 2.0;
double centerY = (maxY + minY) / 2.0;
double fatX = (dx + 1.0)/dx;
double fatY = (dy + 1.0)/dy;
struct psPoint *p = psPoly->ptList;
for (;;)
    {
    p->x = fatPixel(p->x, centerX, fatX);
    p->y = fatPixel(p->y, centerY, fatY);
    p = p->next;
    if (p == psPoly->ptList)
	break;
    }
}

void findLineParams(double x1, double y1, double x2, double y2, double *m, double *b)
/* Return parameters slope m and y-intercept b of equation for line from x1,y1 to x2,y2, 
 * vertical lines return infinite slope with b = x value instead */
{
double dx = x2 - x1;
double dy = y2 - y1;
*m = dy/dx;
if (dx == 0)
    *b = x1;
else
    *b = y1 - (*m)*x1;
}


double bDeltaForParallelLine(double d, double m)
/* find delta-b value for parallel line at distance d for line slope m */
{
return d * sqrt(1 + m*m); /* do not call with m == infinity */
}

void adjustBForParallelLine(boolean cw, double m, 
double x1, double y1, 
double x2, double y2, 
double *b)
/* adjust the b value for parallel line at distance d for line slope m 
 *  cw if clockwise */
{
if (isinf(m))  /* handle vertical lines */
    {
    if ((cw && (y2 < y1)) || (!cw && (y2 > y1)))
	*b += 1;   /* b holds x-value rather than y-intercept */
    else
	*b -= 1;
    }
else
    {
    /* Y axis is increasing downwards */
    double bDelta = bDeltaForParallelLine(1.0, m);
    if ((cw && (x2 > x1)) || (!cw && (x2 < x1)))
       *b += bDelta; 
    else
       *b -= bDelta;
    }
}

void findLinesIntersection(boolean cw, double m0, double b0, double m1, double b1, 
double px, double py, boolean posDeltaX, boolean posDeltaY,
double *ix, double *iy)
/* find intersection between two lines */
{

/* colinear vert */
if ((isinf(m0) && isinf(m1)) && (b0 == b1)) 
    {
    if (cw ^ posDeltaY)
    	*ix = px+1;
    else
    	*ix = px-1;
    *iy = py; 
    }
/* colinear horiz */
else if (((m0==0)&&(m1==0)) && (b0 == b1)) 
    {
    if (cw ^ posDeltaX)
    	*iy = py+1;
    else
    	*iy = py-1;
    *ix = px; 
    }
/* colinear */
else if ((m0 == m1) && (b0 == b1))  
    {
    /* inner point shifted 1 pixel away from the point,
     *   moving perpendicular to m0 towards the center */

    double dx, dy;
    double m = -1/m0; 
    dx = sqrt(1/(1+m*m));
    dy = m * dx;

    if (!(cw ^ posDeltaX))
	{
	*ix = px + dx;
	*iy = py + dy;
	}
    else
	{
	*ix = px - dx;
	*iy = py - dy;
	}

    }
/* non-colinear */
else
    {
    if (isinf(m0) && isinf(m1))
	{  /* should be handled earlier by colinear vert lines above */
	errAbort("pscmGfx: m0 and m1 both inf, shouldn't get here");
	}
    else if (!isinf(m0) && isinf(m1))
	{
    	*ix = b1;
	*iy = m0*(*ix)+b0;
	}
    else if (isinf(m0) && !isinf(m1))
	{
    	*ix = b0;
	*iy = m1*(*ix)+b1;
	}
    else if (!isinf(m0) && !isinf(m1))
	{
    	*ix = -(b1-b0)/(m1-m0);
	*iy = m0*(*ix)+b0;
	}
    }
}

boolean pscmIsClockwise(struct psPoly *psPoly)
/* determine if polygon points are in clockwise order or not
 * using cross-product sum*/
{
struct psPoint *p = psPoly->ptList,*q;
double x0, y0; 
double x1, y1;
double x2, y2;
double crossProd = 0;
x1 = p->x;
y1 = p->y;
p = p->next;
x2 = p->x;
y2 = p->y;
p = p->next;
q = p;
for (;;)
    {
    x0 = x1;
    y0 = y1;
    x1 = x2;
    y1 = y2;
    x2 = p->x;
    y2 = p->y;
    crossProd += ((x1-x0)*(y2-y1) - (y1-y0)*(x2-x1));
    p = p->next;
    if (p == q) 
	break;
    }
return (crossProd > 0);
}

void pscmPolyTrapStrokeOutline(struct pscmGfx *pscm, struct psPoly *psPoly)
/* Stroke a fattened polygon using trapezoids. 
 * Make each stroke-segment of the poly be made of a 4-sided trapezoidal figure
 * whose inner edge is parallel 1 pixel away and the points are found
 * by finding the intersections of these parallel lines. */
{

struct psPoint *p = psPoly->ptList,*q;
double px0, py0; /* outer points */
double px1, py1;
double m0,b0;
double m1,b1;
double ix0,iy0;  /* inner points */
double ix1,iy1;

boolean cw = pscmIsClockwise(psPoly);

px1 = p->x;
py1 = p->y;
p = p->next;
findLineParams(px1, py1, p->x, p->y, &m1, &b1);
adjustBForParallelLine(cw, m1, px1, py1, p->x, p->y, &b1);
px0 = px1;
py0 = py1;
px1 = p->x;
py1 = p->y;
p = p->next;
m0 = m1;
b0 = b1;
findLineParams(px1, py1, p->x, p->y, &m1, &b1);
adjustBForParallelLine(cw, m1, px1, py1, p->x, p->y, &b1);

findLinesIntersection(cw,m0,b0,m1,b1,px1,py1,
    (px1<px0) ^ (m0 < 0),
    (py1>py0),
    &ix1,&iy1);

px0 = px1;
py0 = py1;
px1 = p->x;
py1 = p->y;
p = p->next;
q = p;
for (;;)
    {

    m0 = m1;
    b0 = b1;
    ix0 = ix1;
    iy0 = iy1;
    findLineParams(px1, py1, p->x, p->y, &m1, &b1);
    adjustBForParallelLine(cw, m1, px1, py1, p->x, p->y, &b1);

    findLinesIntersection(cw,m0,b0,m1,b1,px1,py1,
	(px1<px0) ^ (m0 < 0),
	(py1>py0),
	&ix1,&iy1);

    struct psPoly *inner = psPolyNew();
    psPolyAddPoint(inner,px0, py0);
    psPolyAddPoint(inner,px1, py1);
    psPolyAddPoint(inner,ix1, iy1);
    psPolyAddPoint(inner,ix0, iy0);
    psDrawPoly(pscm->ps, inner, TRUE);
    psPolyFree(&inner);

    px0 = px1;
    py0 = py1;
    px1 = p->x;
    py1 = p->y;
    p = p->next;
    if (p == q) 
	break;
    }

}

void pscmDrawPoly(struct pscmGfx *pscm, struct gfxPoly *poly, Color color, 
	boolean filled)
/* Draw a polygon, possibly filled, in color. */
{
struct gfxPoint *p = poly->ptList;
struct psPoly *psPoly = psPolyNew();
int minX, maxX, minY, maxY;
if (poly->ptCount < 1)  /* nothing to do */
    {
    return;
    }
psPolyFindExtremes(poly, &minX, &maxX, &minY, &maxY);
/*  check for co-linear polygon, render as line instead */
if (colinearPoly(poly)) 
    {
    pscmLine(pscm, minX, minY, maxX, maxY, color);
    return;
    }
pscmSetColor(pscm, color);
/* convert pixel coords to real values */
for (;;)
    {
    psPolyAddPoint(psPoly,p->x, p->y);
    p = p->next;
    if (p == poly->ptList)
	break;
    }
boolean fat = sameString(pscmGetHint(pscm,"fat"),"on");
if (fat)
    pscmPolyFatten(psPoly, minX, maxX, minY, maxY);
if (fat && !filled)
    pscmPolyTrapStrokeOutline(pscm,psPoly);
else
    psDrawPoly(pscm->ps, psPoly, filled);
psPolyFree(&psPoly);
}



void pscmFatLine(struct pscmGfx *pscm, double x1, double y1, double x2, double y2)
/* Draw a line from x1/y1 to x2/y2 by making a filled polygon.
 *  This also avoids some problems with stroke-width variation
 *  from different postscript implementations. */
{
struct psPoly *psPoly = psPolyNew();

double cX = (x1+x2)/2.0;
double cY = (y1+y2)/2.0;
double fX;
double fY;

/* fatten by lengthing line by 0.5 at each end */
fX = fY = 1.0+0.5*sqrt(0.5);  

/* expand the length of the line by a half-pixel on each end */
x1 = fatPixel(x1,cX,fX);
x2 = fatPixel(x2,cX,fX);
y1 = fatPixel(y1,cY,fY);
y2 = fatPixel(y2,cY,fY);


/* calculate 4 corners {h,i,j,k} of the rectangle covered */

double m = (y2 - y1)/(x2 - x1);
m = -1/m;  /* rotate slope 90 degrees */

double ddX = sqrt( (0.5*0.5) / (m*m+1) );
double ddY = m * ddX;

double hX = x1-ddX, hY = y1-ddY;
double iX = x1+ddX, iY = y1+ddY;
double jX = x2+ddX, jY = y2+ddY;
double kX = x2-ddX, kY = y2-ddY;

psPolyAddPoint(psPoly,hX,hY);
psPolyAddPoint(psPoly,iX,iY);
psPolyAddPoint(psPoly,jX,jY);
psPolyAddPoint(psPoly,kX,kY);
psDrawPoly(pscm->ps, psPoly, TRUE);
psPolyFree(&psPoly);

}


void pscmLine(struct pscmGfx *pscm, 
	int x1, int y1, int x2, int y2, int color)
/* Draw a line from one point to another. */
{
pscmSetColor(pscm, color);
boolean fat = sameString(pscmGetHint(pscm,"fat"),"on");
if (fat)
    pscmFatLine(pscm, x1, y1, x2, y2);
else
    psDrawLine(pscm->ps, x1, y1, x2, y2);
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
vg->drawPoly = (vg_drawPoly)pscmDrawPoly;
vg->setHint = (vg_setHint)pscmSetHint;
vg->getHint = (vg_getHint)pscmGetHint;
vg->getFontPixelHeight = (vg_getFontPixelHeight)pscmGetFontPixelHeight;
vg->getFontStringWidth = (vg_getFontStringWidth)pscmGetFontStringWidth;
vg->setWriteMode = (vg_setWriteMode)pscmSetWriteMode;
return vg;
}

