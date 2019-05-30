/* PostScript graphics - 
 * This provides a bit of a shell around writing graphics to
 * a postScript file.  Perhaps the most important thing it
 * does is convert 0,0 from being at the bottom left to
 * being at the top left. */

#ifndef PSGFX_H
#define PSGFX_H

#include "psPoly.h"

struct psGfx 
/* A postScript output file. */
    {
    FILE *f;                      /* File to write to. */
    double userWidth, userHeight; /* Size of image in virtual pixels. */
    double ptWidth, ptHeight;     /* Size of image in points (1/72 of an inch) */
    double xScale, yScale;        /* Conversion from pixels to points. */
    double xOff, yOff;            /* Offset from pixels to points. */
    double fontHeight;		  /* Height of current font. */
    };

struct psGfx *psOpen(char *fileName, 
	double userWidth, double userHeight, /* Dimension of image in user's units. */
	double ptWidth, double ptHeight,     /* Dimension of image in points. */
	double ptMargin);                    /* Image margin in points. */
/* Open up a new postscript file.  If ptHeight is 0, it will be
 * calculated to keep pixels square. */

void psClose(struct psGfx **pPs);
/* Close out postScript file. */

void psTranslate(struct psGfx *ps, double xTrans, double yTrans);
/* add a constant to translate all coordinates */

void psSetLineWidth(struct psGfx *ps, double factor);
/* Set line width to factor * a single pixel width. */

void psClipRect(struct psGfx *ps, double x, double y, 
	double width, double height);
/* Set clipping rectangle. */

void psDrawBox(struct psGfx *ps, double x, double y, 
	double width, double height);
/* Draw a filled box in current color. */

void psDrawLine(struct psGfx *ps, double x1, double y1, 
	double x2, double y2);
/* Draw a line from x1/y1 to x2/y2 */

void psFillUnder(struct psGfx *ps, double x1, double y1, 
	double x2, double y2, double bottom);
/* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
 * it's top, a horizontal line at bottom at it's bottom,  and
 * vertical lines from the bottom to y1 on the left and bottom to
 * y2 on the right. */

void psXyOut(struct psGfx *ps, double x, double y);
/* Output x,y position transformed into PostScript space. 
 * Useful if you're mixing direct PostScript with psGfx
 * functions. */

void psWhOut(struct psGfx *ps, double width, double height);
/* Output width/height transformed into PostScript space. */

void psMoveTo(struct psGfx *ps, double x, double y);
/* Move PostScript position to given point. */

void psTextAt(struct psGfx *ps, double x, double y, char *text);
/* Output text in current font at given position. */

void psTextBox(struct psGfx *ps, double x, double y, char *text);
/* Fill a box the size of the text at the given position*/

void psTextDown(struct psGfx *ps, double x, double y, char *text);
/* Output text going downwards rather than across at position. */

void psTextRight(struct psGfx *mg, double x, double y, 
	double width, double height, char *text);
/* Draw a line of text right justified in box defined by x/y/width/height */

void psTextCentered(struct psGfx *mg, double x, double y, 
	double width, double height, char *text);
/* Draw a line of text centered in box defined by x/y/width/height */

void psTimesFont(struct psGfx *ps, double size);
/* Set font to times of a certain size. */

void psSetColor(struct psGfx *ps, int r, int g, int b);
/* Set current color. r/g/b values are between 0 and 255. */

void psSetGray(struct psGfx *ps, double grayVal);
/* Set gray value (between 0.0 and 1.0. */

void psPushG(struct psGfx *ps);
/* Save graphics state on stack. */

void psPopG(struct psGfx *ps);
/* Pop off saved graphics state. */

void psDrawPoly(struct psGfx *ps, struct psPoly *poly, boolean filled);
/* Draw a possibly filled polygon */

void psFillEllipse(struct psGfx *ps, double x, double y, double xrad, double yrad);
/* Draw a filled ellipse */

void psDrawEllipse(struct psGfx *ps, double x, double y, double xrad, double yrad,
    double startAngle, double endAngle);
/* Draw an ellipse outline */

void psDrawCurve(struct psGfx *ps, double x1, double y1, double x2, double y2,
                        double x3, double y3, double x4, double y4);
/* Draw Bezier curve specified by 4 points: first (p1) and last (p4)
 *      and 2 control points (p2, p3) */

void psSetDash(struct psGfx *ps, boolean on);
/* Set dashed line mode on or off. If set on, show two points marked, with one point of space */

char * convertEpsToPdf(char *epsFile);
/* Convert EPS to PDF and return filename, or NULL if failure. */

void psLineTo(struct psGfx *ps, double x, double y);
/* Draw line from current point to given point,
 * and make given point new current point. */

void psCircle(struct psGfx *ps, double x, double y, double rad, boolean filled);
#endif /* PSGFX_H */

