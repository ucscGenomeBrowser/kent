/* mgCurves.c - Bezier curve and ellipse */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "memgfx.h"

void mgEllipse(struct memGfx *mg, int x0, int y0, int x1, int y1, Color color,
                        int mode, boolean isDashed)
/* Draw an ellipse (or limit to top or bottom) specified by rectangle, using Bresenham algorithm.
 * Optionally, alternate dots.
 * Point 0 is left, point 1 is top of rectangle
 * Adapted trivially from code posted at http://members.chello.at/~easyfilter/bresenham.html
 * Author: Zingl Alois, 8/22/2016
 */
{
   int a = abs(x1-x0), b = abs(y1-y0), b1 = b&1; /* values of diameter */
   long dx = 4*(1-a)*b*b, dy = 4*(b1+1)*a*a; /* error increment */
   long err = dx+dy+b1*a*a, e2; /* error of 1.step */

   if (x0 > x1) { x0 = x1; x1 += a; } /* if called with swapped points */
   if (y0 > y1) y0 = y1; /* .. exchange them */
   y0 += (b+1)/2; y1 = y0-b1;   /* starting pixel */
   a *= 8*a; b1 = 8*b*b;

   int dots = 0;
   do {
       if (!isDashed || (++dots % 3))
           {
           if (mode == ELLIPSE_BOTTOM || mode == ELLIPSE_FULL) 
               {
               mgPutDot(mg, x1, y0, color); /*   I. Quadrant */
               mgPutDot(mg, x0, y0, color); /*  II. Quadrant */
               }
           if (mode == ELLIPSE_TOP || mode == ELLIPSE_FULL) 
               {
               mgPutDot(mg, x0, y1, color); /* III. Quadrant */
               mgPutDot(mg, x1, y1, color); /*  IV. Quadrant */
               }
           }
       e2 = 2*err;
       if (e2 <= dy) { y0++; y1--; err += dy += a; }  /* y step */
       if (e2 >= dx || 2*err > dy) { x0++; x1--; err += dx += b1; } /* x step */
   } while (x0 <= x1);

   while (y0-y1 < b) {  /* too early stop of flat ellipses a=1 */
       if (!isDashed && (++dots % 3))
           {
           mgPutDot(mg, x0-1, y0, color); /* -> finish tip of ellipse */
           mgPutDot(mg, x1+1, y0++, color);
           mgPutDot(mg, x0-1, y1, color);
           mgPutDot(mg, x1+1, y1--, color);
           }
   }
}

