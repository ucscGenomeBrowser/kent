/* PostScript graphics - 
 * This provides a bit of a shell around writing graphics to
 * a postScript file.  Perhaps the most important thing it
 * does is convert 0,0 from being at the bottom left to
 * being at the top left. */

#ifndef PSGFX_H
#define PSGFX_H

struct psGfx 
/* A postScript output file. */
    {
    FILE *f;                    /* File to write to. */
    int pixWidth, pixHeight;	/* Size of image in virtual pixels. */
    double ptWidth, ptHeight;   /* Size of image in points (1/72 of an inch) */
    double xScale, yScale;      /* Conversion from pixels to points. */
    double xOff, yOff;          /* Offset from pixels to points. */
    };

struct psGfx *psOpen(char *fileName, 
	int pixWidth, int pixHeight, 	 /* Dimension of image in pixels. */
	double ptWidth, double ptHeight, /* Dimension of image in points. */
	double ptMargin);                /* Image margin in points. */
/* Open up a new postscript file.  If ptHeight is 0, it will be
 * calculated to keep pixels square. */

void psClose(struct psGfx **pPs);
/* Close out postScript file. */

void psDrawBox(struct psGfx *ps, int x, int y, int width, int height);
/* Draw a filled box in current color. */

void psDrawLine(struct psGfx *ps, int x1, int y1, int x2, int y2);
/* Draw a line from x1/y1 to x2/y2 */

void psXyOut(struct psGfx *ps, int x, int y);
/* Output x,y position transformed into PostScript space. 
 * Useful if you're mixing direct PostScript with psGfx
 * functions. */

void psMoveTo(struct psGfx *ps, int x, int y);
/* Move PostScript position to given point. */

void psTextAt(struct psGfx *ps, int x, int y, char *text);
/* Output text in current font at given position. */

void psWhOut(struct psGfx *ps, int width, int height);
/* Output width/height transformed into PostScript space. */

void psTextDown(struct psGfx *ps, int x, int y, char *text);
/* Output text going downwards rather than across at position. */

void psSetColor(struct psGfx *ps, int r, int g, int b);
/* Set current color. r/g/b values are between 0 and 255. */

void psSetGray(struct psGfx *ps, double grayVal);
/* Set gray value (between 0.0 and 1.0. */

void psPushG(struct psGfx *ps);
/* Save graphics state on stack. */

void psPopG(struct psGfx *ps);
/* Pop off saved graphics state. */

void psPushClipRect(struct psGfx *ps, int x, int y, int width, int height);
/* Push clipping rectangle onto graphics stack. */


#endif /* PSGFX_H */

