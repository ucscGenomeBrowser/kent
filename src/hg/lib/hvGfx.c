/* hvGfx - browser graphics interface.  This is a thin layer on top of vGfx
 * providing genome browser-specific features.  It was added to handle
 * reverse-complement display. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hvGfx.h"
#include "obscure.h"


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

struct hvGfx *hvGfxOpenPng(int width, int height, char *fileName, boolean useTransparency)
/* Open up something that we'll write out as a PNG someday. */
{
return hvGfxAlloc(vgOpenPng(width, height, fileName, useTransparency));
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
char tbuf[18];
int numWid;
int goodNumTicks;
int niceNumTicks = width/35;

sprintLongWithCommas(tbuf, startNum+range);
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
    sprintLongWithCommas(tbuf, tickPos);
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

/* from memgfx.c and vPng.c*/

struct memPng
/* Something that handles a PNG. */
    {
    struct memGfx mg;	/* Memory form.  This needs to be first field. */
    char *fileName;	/* PNG file name. */
    boolean useTransparency;   /* Make background color transparent if TRUE. */
    };

#define _mgBpr(mg) ((mg)->width)
#define _mgPixAdr(mg,x,y) ((mg)->pixels+_mgBpr(mg) * (y) + (x))

INLINE void mixDot(struct hvGfx *hvg, int x, int y,  float frac, Color col)
/* Puts a single dot on the image, mixing it with what is already there
 * based on the frac argument. */
{
struct memPng *png = (struct memPng *)hvg->vg->data;
struct memGfx *img = (struct memGfx *)png;
if ((x < img->clipMinX) || (x > img->clipMaxX) || (y < img->clipMinY) || (y > img->clipMaxY))
    return;

Color *pt = _mgPixAdr(img,x,y);
float invFrac = 1 - frac;

int r = COLOR_32_RED(*pt) * invFrac + COLOR_32_RED(col) * frac;
int g = COLOR_32_GREEN(*pt) * invFrac + COLOR_32_GREEN(col) * frac;
int b = COLOR_32_BLUE(*pt) * invFrac + COLOR_32_BLUE(col) * frac;
hvGfxDot(hvg, x, y, MAKECOLOR_32(r,g,b));
hvGfxDot(hvg, x, y, MAKECOLOR_32(r,g,b));
}

#define fraction(X) (((double)(X))-(double)(int)(X))
#define invFraction(X) (1.0-fraction(X))

void hvGfxCurveSeg(struct hvGfx *hvg, int x0, int y0, int x1, int y1, int x2, int y2, Color color)
/* Draw a segment of a curve within 3 points (quadratic Bezier)
 * Adapted trivially from code posted on github and at http://members.chello.at/~easyfilter/bresenham.html */
 /* Thanks to author  * @author Zingl Alois
 * @date 22.08.2016 */
{
  int sx = x2-x1, sy = y2-y1;
  long xx = x0-x1, yy = y0-y1, xy;              /* relative values for checks */
  double dx, dy, err, cur = xx*sy-yy*sx;                         /* curvature */

  assert(xx*sx <= 0 && yy*sy <= 0);       /* sign of gradient must not change */

  if (sx*(long)sx+sy*(long)sy > xx*xx+yy*yy) {      /* begin with longer part */
    x2 = x0; x0 = sx+x1; y2 = y0; y0 = sy+y1; cur = -cur;       /* swap P0 P2 */
  }
  if (cur != 0) {                                         /* no straight line */
    xx += sx; xx *= sx = x0 < x2 ? 1 : -1;                /* x step direction */
    yy += sy; yy *= sy = y0 < y2 ? 1 : -1;                /* y step direction */
    xy = 2*xx*yy; xx *= xx; yy *= yy;               /* differences 2nd degree */
    if (cur*sx*sy < 0) {                                /* negated curvature? */
      xx = -xx; yy = -yy; xy = -xy; cur = -cur;
    }
    dx = 4.0*sy*cur*(x1-x0)+xx-xy;                  /* differences 1st degree */
    dy = 4.0*sx*cur*(y0-y1)+yy-xy;
    xx += xx; yy += yy; err = dx+dy+xy;                     /* error 1st step */
    do {
      hvGfxDot(hvg, x0, y0, color);                                          /* plot curve */
      if (x0 == x2 && y0 == y2) return;       /* last pixel -> curve finished */
      y1 = 2*err < dx;                       /* save value for test of y step */
      if (2*err > dy) { x0 += sx; dx -= xy; err += dy += yy; }      /* x step */
      if (    y1    ) { y0 += sy; dy -= xy; err += dx += xx; }      /* y step */
    } while (dy < 0 && dx > 0);        /* gradient negates -> algorithm fails */
  }
  hvGfxLine(hvg, x0,y0, x2,y2, color);                       /* plot remaining part to end */
}

void hvGfxCurveSegAA(struct hvGfx *hvg, int x0, int y0, int x1, int y1, int x2, int y2, Color color)
/* Draw a segment of an anti-aliased curve within 3 points (quadratic Bezier)
 * Adapted trivially from code posted on github and at http://members.chello.at/~easyfilter/bresenham.html */
 /* Thanks to author  * @author Zingl Alois
 * @date 22.08.2016 */
{
   int sx = x2-x1, sy = y2-y1;
   long xx = x0-x1, yy = y0-y1, xy;             /* relative values for checks */
   double dx, dy, err, ed, cur = xx*sy-yy*sx;                    /* curvature */

   assert(xx*sx <= 0 && yy*sy <= 0);      /* sign of gradient must not change */

   if (sx*(long)sx+sy*(long)sy > xx*xx+yy*yy) {     /* begin with longer part */
      x2 = x0; x0 = sx+x1; y2 = y0; y0 = sy+y1; cur = -cur;     /* swap P0 P2 */
   }
   if (cur != 0)
   {                                                      /* no straight line */
      xx += sx; xx *= sx = x0 < x2 ? 1 : -1;              /* x step direction */
      yy += sy; yy *= sy = y0 < y2 ? 1 : -1;              /* y step direction */
      xy = 2*xx*yy; xx *= xx; yy *= yy;             /* differences 2nd degree */
      if (cur*sx*sy < 0) {                              /* negated curvature? */
         xx = -xx; yy = -yy; xy = -xy; cur = -cur;
      }
      dx = 4.0*sy*(x1-x0)*cur+xx-xy;                /* differences 1st degree */
      dy = 4.0*sx*(y0-y1)*cur+yy-xy;
      xx += xx; yy += yy; err = dx+dy+xy;                   /* error 1st step */
      do {
         cur = fmin(dx+xy,-xy-dy);
         ed = fmax(dx+xy,-xy-dy);               /* approximate error distance */
         ed += 2*ed*cur*cur/(4*ed*ed+cur*cur);
         mixDot(hvg, x0,y0, 1-fabs(err-dx-dy-xy)/ed, color);          /* plot curve */
         if (x0 == x2 || y0 == y2) break;     /* last pixel -> curve finished */
         x1 = x0; cur = dx-err; y1 = 2*err+dy < 0;
         if (2*err+dx > 0) {                                        /* x step */
            if (err-dy < ed) mixDot(hvg, x0,y0+sy, 1-fabs(err-dy)/ed, color);
            x0 += sx; dx -= xy; err += dy += yy;
         }
         if (y1) {                                                  /* y step */
            if (cur < ed) mixDot(hvg, x1+sx,y0, 1-fabs(cur)/ed, color);
            y0 += sy; dy -= xy; err += dx += xx;
         }
      } while (dy < dx);                  /* gradient negates -> close curves */
   }
   hvGfxLine(hvg, x0,y0, x2,y2, color);                  /* plot remaining needle to end */
}

void hvGfxCurveAA(struct hvGfx *hvg, int x0, int y0, int x1, int y1, int x2, int y2, Color color)
/* Draw a segment of an anti-aliased curve within 3 points (quadratic Bezier)
 * Adapted trivially from code posted at http://members.chello.at/~easyfilter/bresenham.html */
{
   int x = x0-x1, y = y0-y1;
   double t = x0-2*x1+x2, r;

   if ((long)x*(x2-x1) > 0) {                        /* horizontal cut at P4? */
      if ((long)y*(y2-y1) > 0)                     /* vertical cut at P6 too? */
         if (fabs((y0-2*y1+y2)/t*x) > abs(y)) {               /* which first? */
            x0 = x2; x2 = x+x1; y0 = y2; y2 = y+y1;            /* swap points */
         }                            /* now horizontal cut at P4 comes first */
      t = (x0-x1)/t;
      r = (1-t)*((1-t)*y0+2.0*t*y1)+t*t*y2;                       /* By(t=P4) */
      t = (x0*x2-x1*x1)*t/(x0-x1);                       /* gradient dP4/dx=0 */
      x = floor(t+0.5); y = floor(r+0.5);
      r = (y1-y0)*(t-x0)/(x1-x0)+y0;                  /* intersect P3 | P0 P1 */
      hvGfxCurveSegAA(hvg,x0,y0, x,floor(r+0.5), x,y, color);
      r = (y1-y2)*(t-x2)/(x1-x2)+y2;                  /* intersect P4 | P1 P2 */
      x0 = x1 = x; y0 = y; y1 = floor(r+0.5);             /* P0 = P4, P1 = P8 */
   }
   if ((long)(y0-y1)*(y2-y1) > 0) {                    /* vertical cut at P6? */
      t = y0-2*y1+y2; t = (y0-y1)/t;
      r = (1-t)*((1-t)*x0+2.0*t*x1)+t*t*x2;                       /* Bx(t=P6) */
      t = (y0*y2-y1*y1)*t/(y0-y1);                       /* gradient dP6/dy=0 */
      x = floor(r+0.5); y = floor(t+0.5);
      r = (x1-x0)*(t-y0)/(y1-y0)+x0;                  /* intersect P6 | P0 P1 */
      hvGfxCurveSegAA(hvg,x0,y0, floor(r+0.5),y, x,y, color);
      r = (x1-x2)*(t-y2)/(y1-y2)+x2;                  /* intersect P7 | P1 P2 */
      x0 = x; x1 = floor(r+0.5); y0 = y1 = y;             /* P0 = P6, P1 = P7 */
   }
   hvGfxCurveSegAA(hvg,x0,y0, x1,y1, x2,y2, color);                  /* remaining part */
}

void hvGfxCurve(struct hvGfx *hvg, int x0, int y0, int x1, int y1, int x2, int y2, Color color)
/* Draw a segment of an anti-aliased curve within 3 points (quadratic Bezier)
 * Adapted trivially from code posted at http://members.chello.at/~easyfilter/bresenham.html */
{
   int x = x0-x1, y = y0-y1;
   double t = x0-2*x1+x2, r;

   if ((long)x*(x2-x1) > 0) {                        /* horizontal cut at P4? */
      if ((long)y*(y2-y1) > 0)                     /* vertical cut at P6 too? */
         if (fabs((y0-2*y1+y2)/t*x) > abs(y)) {               /* which first? */
            x0 = x2; x2 = x+x1; y0 = y2; y2 = y+y1;            /* swap points */
         }                            /* now horizontal cut at P4 comes first */
      t = (x0-x1)/t;
      r = (1-t)*((1-t)*y0+2.0*t*y1)+t*t*y2;                       /* By(t=P4) */
      t = (x0*x2-x1*x1)*t/(x0-x1);                       /* gradient dP4/dx=0 */
      x = floor(t+0.5); y = floor(r+0.5);
      r = (y1-y0)*(t-x0)/(x1-x0)+y0;                  /* intersect P3 | P0 P1 */
      hvGfxCurveSeg(hvg,x0,y0, x,floor(r+0.5), x,y, color);
      r = (y1-y2)*(t-x2)/(x1-x2)+y2;                  /* intersect P4 | P1 P2 */
      x0 = x1 = x; y0 = y; y1 = floor(r+0.5);             /* P0 = P4, P1 = P8 */
   }
   if ((long)(y0-y1)*(y2-y1) > 0) {                    /* vertical cut at P6? */
      t = y0-2*y1+y2; t = (y0-y1)/t;
      r = (1-t)*((1-t)*x0+2.0*t*x1)+t*t*x2;                       /* Bx(t=P6) */
      t = (y0*y2-y1*y1)*t/(y0-y1);                       /* gradient dP6/dy=0 */
      x = floor(r+0.5); y = floor(t+0.5);
      r = (x1-x0)*(t-y0)/(y1-y0)+x0;                  /* intersect P6 | P0 P1 */
      hvGfxCurveSeg(hvg,x0,y0, floor(r+0.5),y, x,y, color);
      r = (x1-x2)*(t-y2)/(y1-y2)+x2;                  /* intersect P7 | P1 P2 */
      x0 = x; x1 = floor(r+0.5); y0 = y1 = y;             /* P0 = P6, P1 = P7 */
   }
   hvGfxCurveSeg(hvg,x0,y0, x1,y1, x2,y2, color);                  /* remaining part */
}

#ifdef INFINITE
void hvGfxEllipseAA(struct hvGfx *hvg, int x0, int y0, int x1, int y1, Color color)
/* Draw an anti-aliased ellipse specified by rectangle, using Bresenham algorithm.
 * Point 0 is left, point 1 is top of rectangle
 * Adapted trivially from code posted at http://members.chello.at/~easyfilter/bresenham.html
 */
{
   long a = abs(x1-x0), b = abs(y1-y0), b1 = b&1;                 /* diameter */
   float dx = 4*(a-1.0)*b*b, dy = 4*(b1+1)*a*a;            /* error increment */
   float ed, i, err = b1*a*a-dx+dy;                        /* error of 1.step */
   bool f;

   if (a == 0 || b == 0) return hvGfxLine(hvg, x0, y0, x1, y1, color);
   if (x0 > x1) { x0 = x1; x1 += a; }        /* if called with swapped points */
   if (y0 > y1) y0 = y1;                                  /* .. exchange them */
   y0 += (b+1)/2; y1 = y0-b1;                               /* starting pixel */
   a = 8*a*a; b1 = 8*b*b;

   int q = 0;
   for (q=0; q<1000; q++) {                             /* approximate ed=sqrt(dx*dx+dy*dy) */
   //for (;;) {                             /* approximate ed=sqrt(dx*dx+dy*dy) */
      i = min(dx,dy); ed = max(dx,dy);
      if (y0 == y1+1 && err > dy && a > b1) ed = 255*4./a;           /* x-tip */
      else ed = 255/(ed+2*ed*i*i/(4*ed*ed+i*i));             /* approximation */
      i = ed*fabs(err+dx-dy);           /* get intensity value by pixel error */
      mixDot(hvg, x0,y0, i, color); mixDot(hvg, x0,y1, i, color);
      mixDot(hvg, x1,y0, i, color); mixDot(hvg, x1,y1, i, color);

      if ((f = 2*err+dy) >= 0) {                  /* x step, remember condition */
         if (x0 >= x1) break;
         i = ed*(err+dx);
         if (i < 255) {
            mixDot(hvg, x0,y0+1, i, color); mixDot(hvg, x0,y1-1, i, color);
            mixDot(hvg, x1,y0+1, i, color); mixDot(hvg, x1,y1-1, i, color);
         }          /* do error increment later since values are still needed */
      }
      if ((2*err) <= dx) {                                            /* y step */
         i = ed*(dy-err);
         if (i < 255) {
            mixDot(hvg, x0+1,y0, i, color); mixDot(hvg, x1-1,y0, i, color);
            mixDot(hvg, x0+1,y1, i, color); mixDot(hvg, x1-1,y1, i, color);
         }
         y0++; y1--; err += dy += a;
      }
      if (f) { x0++; x1--; err -= dx -= b1; }            /* x error increment */
   }
   if (--x0 == x1++)                       /* too early stop of flat ellipses */
      while (y0-y1 < b) {
         i = 255*4*fabs(err+dx)/b1;               /* -> finish tip of ellipse */
         mixDot(hvg, x0,++y0, i, color); mixDot(hvg, x1,y0, i, color);
         mixDot(hvg, x0,--y1, i, color); mixDot(hvg, x1,y1, i, color);
         err += dy += a;
      }
}

#endif

void hvGfxEllipseDraw(struct hvGfx *hvg, int x0, int y0, int x1, int y1, Color color, int mode)
/* Draw an ellipse (or limit to top or bottom) specified by rectangle, using Bresenham algorithm.
 * Point 0 is left, point 1 is top of rectangle
 * Adapted trivially from code posted at http://members.chello.at/~easyfilter/bresenham.html
 */
{
   int a = abs(x1-x0), b = abs(y1-y0), b1 = b&1; /* values of diameter */
   long dx = 4*(1-a)*b*b, dy = 4*(b1+1)*a*a; /* error increment */
   long err = dx+dy+b1*a*a, e2; /* error of 1.step */

   if (x0 > x1) { x0 = x1; x1 += a; } /* if called with swapped points */
   if (y0 > y1) y0 = y1; /* .. exchange them */
   y0 += (b+1)/2; y1 = y0-b1;   /* starting pixel */
   a *= 8*a; b1 = 8*b*b;

   do {
       if (mode == ELLIPSE_BOTTOM || mode == ELLIPSE_FULL) {
           hvGfxDot(hvg, x1, y0, color); /*   I. Quadrant */
           hvGfxDot(hvg, x0, y0, color); /*  II. Quadrant */
       }
       if (mode == ELLIPSE_TOP || mode == ELLIPSE_FULL) {
           hvGfxDot(hvg, x0, y1, color); /* III. Quadrant */
           hvGfxDot(hvg, x1, y1, color); /*  IV. Quadrant */
       }
       e2 = 2*err;
       if (e2 <= dy) { y0++; y1--; err += dy += a; }  /* y step */
       if (e2 >= dx || 2*err > dy) { x0++; x1--; err += dx += b1; } /* x step */
   } while (x0 <= x1);

   while (y0-y1 < b) {  /* too early stop of flat ellipses a=1 */
       hvGfxDot(hvg, x0-1, y0, color); /* -> finish tip of ellipse */
       hvGfxDot(hvg, x1+1, y0++, color);
       hvGfxDot(hvg, x0-1, y1, color);
       hvGfxDot(hvg, x1+1, y1--, color);
   }
}

void hvGfxEllipse(struct hvGfx *hvg, int x0, int y0, int x1, int y1, Color color)
/* Draw a full ellipse specified by rectangle, using Bresenham algorithm.
 * Point 0 is left, point 1 is top of rectangle
 * Adapted trivially from code posted at http://members.chello.at/~easyfilter/bresenham.html
 */
{
hvGfxEllipseDraw(hvg, x0, y0, x1, y1, color, ELLIPSE_FULL);
}


