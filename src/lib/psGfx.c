/* PostScript graphics - 
 * This provides a bit of a shell around writing graphics to
 * a postScript file.  Perhaps the most important thing it
 * does is convert 0,0 from being at the bottom left to
 * being at the top left. */
#include "common.h"
#include "psGfx.h"

static void psFloatOut(FILE *f, double x)
/* Write out a floating point number, but not in too much
 * precision. */
{
int i = round(x);
if (i == x)
   fprintf(f, "%d ", i);
else
   fprintf(f, "%0.4f ", x);
}

static void psWriteHeader(FILE *f, double width, double height)
/* Write postScript header.  It's encapsulated PostScript
 * actually, so you need to supply image width and height
 * in points. */
{
char *s =
#include "common.pss"
;

fprintf(f, "%%!PS-Adobe-3.1 EPSF-3.0\n");
fprintf(f, "%%%%BoundingBox: 0 0 %d %d\n\n", (int)ceil(width), (int)ceil(height));
fprintf(f, "%s", s);
}

struct psGfx *psOpen(char *fileName, 
	int pixWidth, int pixHeight, 	 /* Dimension of image in pixels. */
	double ptWidth, double ptHeight, /* Dimension of image in points. */
	double ptMargin)                 /* Image margin in points. */
/* Open up a new postscript file.  If ptHeight is 0, it will be
 * calculated to keep pixels square. */
{
struct psGfx *ps;

/* Allocate structure and open file. */
AllocVar(ps);
ps->f = mustOpen(fileName, "w");

/* Save page dimensions and calculate scaling factors. */
ps->pixWidth = pixWidth;
ps->pixHeight = pixHeight;
ps->ptWidth = ptWidth;
ps->xScale = (ptWidth - 2*ptMargin)/pixWidth;
if (ptHeight != 0.0)
   {
   ps->ptHeight = ptHeight;
   ps->yScale = (ptHeight - 2*ptMargin) / pixHeight;
   }
else
   {
   ps->yScale = ps->xScale;
   ptHeight = ps->ptHeight = pixHeight * ps->yScale + 2*ptMargin;
   }
ps->xOff = ptMargin;
ps->yOff = ptMargin;
ps->fontHeight = 10;

/* Cope with fact y coordinates are bottom to top rather
 * than top to bottom. */
ps->yScale = -ps->yScale;
ps->yOff = ps->ptHeight - ps->yOff;

psWriteHeader(ps->f, ptWidth, ptHeight);

/* Set line width to a single pixel. */
fprintf(ps->f, "%f setlinewidth\n", ps->xScale);

return ps;
}

void psClose(struct psGfx **pPs)
/* Close out postScript file. */
{
struct psGfx *ps = *pPs;
if (ps != NULL)
    {
    carefulClose(&ps->f);
    freez(pPs);
    }
}

void psXyOut(struct psGfx *ps, double x, double y)
/* Output x,y position transformed into PostScript space. */
{
FILE *f = ps->f;
psFloatOut(f, x * ps->xScale + ps->xOff);
psFloatOut(f, y * ps->yScale + ps->yOff);
}

void psWhOut(struct psGfx *ps, double width, double height)
/* Output width/height transformed into PostScript space. */
{
FILE *f = ps->f;
psFloatOut(f, width * ps->xScale);
psFloatOut(f, height * -ps->yScale);
}

void psMoveTo(struct psGfx *ps, double x, double y)
/* Move PostScript position to given point. */
{
psXyOut(ps, x, y);
fprintf(ps->f, "moveto\n");
}

static void psLineTo(struct psGfx *ps, double x, double y)
/* Draw line from current point to given point,
 * and make given point new current point. */
{
psXyOut(ps, x, y);
fprintf(ps->f, "lineto\n");
}

void psDrawBox(struct psGfx *ps, double x, double y, 
	double width, double height)
/* Draw a filled box in current color. */
{
psWhOut(ps, width, height);
psXyOut(ps, x, y+height);
fprintf(ps->f, "fillBox\n");
}

void psDrawLine(struct psGfx *ps, double x1, double y1, double x2, double y2)
/* Draw a line from x1/y1 to x2/y2 */
{
FILE *f = ps->f;
fprintf(f, "newpath\n");
psMoveTo(ps, x1, y1);
psXyOut(ps, x2, y2);
fprintf(ps->f, "lineto\n");
fprintf(f, "stroke\n");
}

void psFillUnder(struct psGfx *ps, double x1, double y1, 
	double x2, double y2, double bottom)
/* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
 * it's top, a horizontal line at bottom at it's bottom,  and
 * vertical lines from the bottom to y1 on the left and bottom to
 * y2 on the right. */
{
FILE *f = ps->f;
fprintf(f, "newpath\n");
psMoveTo(ps, x1, y1);
psLineTo(ps, x2, y2);
psLineTo(ps, x2, bottom);
psLineTo(ps, x1, bottom);
fprintf(f, "closepath\n");
fprintf(f, "fill\n");
}

void psTimesFont(struct psGfx *ps, double size)
/* Set font to times of a certain size. */
{
FILE *f = ps->f;
// fprintf(f, "/Times-Roman findfont ");
fprintf(f, "/Helvetica findfont ");

/* Note the 1.2 and the 1.0 below seem to get it to 
 * position about where the stuff developed for pixel
 * based systems expects it.  It is all a kludge though! */
fprintf(f, "%f scalefont setfont\n", -size*ps->yScale*1.2);
ps->fontHeight = size*0.8;
}


void psTextAt(struct psGfx *ps, double x, double y, char *text)
/* Output text in current font at given position. */
{
psMoveTo(ps, x, y + ps->fontHeight);
fprintf(ps->f, "(%s) show\n", text);
}

void psTextRight(struct psGfx *ps, double x, double y, 
	double width, double height, 
	char *text)
/* Draw a line of text right justified in box defined by x/y/width/height */
{
y += (height - ps->fontHeight)/2;
psMoveTo(ps, x+width, y + ps->fontHeight);
fprintf(ps->f, "(%s) showBefore\n", text);
}

void psTextCentered(struct psGfx *ps, double x, double y, 
	double width, double height, 
	char *text)
/* Draw a line of text centered in box defined by x/y/width/height */
{
y += (height - ps->fontHeight)/2;
psMoveTo(ps, x+width/2, y + ps->fontHeight);
fprintf(ps->f, "(%s) showMiddle\n", text);
}

void psTextDown(struct psGfx *ps, double x, double y, char *text)
/* Output text going downwards rather than across at position. */
{
FILE *f = ps->f;
psMoveTo(ps, x, y);
fprintf(ps->f, "gsave\n");
fprintf(ps->f, "-90 rotate\n");
fprintf(ps->f, "(%s) show\n", text);
fprintf(ps->f, "grestore\n");
}

void psSetColor(struct psGfx *ps, int r, int g, int b)
/* Set current color. */
{
FILE *f = ps->f;
double scale = 1.0/255;
if (r == g && g == b)
    {
    psFloatOut(f, scale * r);
    fprintf(f, "setgray\n");
    }
else
    {
    psFloatOut(f, scale * r);
    psFloatOut(f, scale * g);
    psFloatOut(f, scale * b);
    fprintf(f, "setrgbcolor\n");
    }
}

void psSetGray(struct psGfx *ps, double grayVal)
/* Set gray value. */
{
FILE *f = ps->f;
if (grayVal < 0) grayVal = 0;
if (grayVal > 1) grayVal = 1;
psFloatOut(f, grayVal);
fprintf(f, "setgray\n");
}

void psPushG(struct psGfx *ps)
/* Save graphics state on stack. */
{
fprintf(ps->f, "gsave\n");
}

void psPopG(struct psGfx *ps)
/* Pop off saved graphics state. */
{
fprintf(ps->f, "grestore %%unclip\n");
}

void psPushClipRect(struct psGfx *ps, double x, double y, 
	double width, double height)
/* Push clipping rectangle onto graphics stack. */
{
FILE *f = ps->f;
fprintf(f, "gsave ");
psXyOut(ps, x, y+height);
psWhOut(ps, width, height);
fprintf(f, "rectclip\n");
}

