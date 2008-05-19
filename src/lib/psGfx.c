/* PostScript graphics - 
 * This provides a bit of a shell around writing graphics to
 * a postScript file.  Perhaps the most important thing it
 * does is convert 0,0 from being at the bottom left to
 * being at the top left. */
#include "common.h"
#include "psPoly.h"
#include "psGfx.h"
#include "linefile.h"

static char const rcsid[] = "$Id: psGfx.c,v 1.34 2008/05/19 21:56:23 braney Exp $";

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

void psClipRect(struct psGfx *ps, double x, double y, 
	double width, double height)
/* Set clipping rectangle. */
{
FILE *f = ps->f;
fprintf(f, "cliprestore ");
psXyOut(ps, x, y+height); 
psWhOut(ps, width, height);       
fprintf(f, "rectclip\n");
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
fprintf(f, "%%%%BoundingBox: 0 0 %d %d\n", (int)ceil(width), (int)ceil(height));
fprintf(f, "%%%%LanguageLevel: 3\n\n");
fprintf(f, "%s", s);
}

void psSetLineWidth(struct psGfx *ps, double factor)
/* Set line width to factor * a single pixel width. */
{
fprintf(ps->f, "%f setlinewidth\n", factor * ps->xScale);
}

struct psGfx *psOpen(char *fileName, 
	double userWidth, double userHeight, /* Dimension of image in user's units. */
	double ptWidth, double ptHeight,     /* Dimension of image in points. */
	double ptMargin)                     /* Image margin in points. */
/* Open up a new postscript file.  If ptHeight is 0, it will be
 * calculated to keep pixels square. */
{
struct psGfx *ps;

/* Allocate structure and open file. */
AllocVar(ps);
ps->f = mustOpen(fileName, "w");

/* Save page dimensions and calculate scaling factors. */
ps->userWidth = userWidth;
ps->userHeight = userHeight;
ps->ptWidth = ptWidth;
ps->xScale = (ptWidth - 2*ptMargin)/userWidth;
if (ptHeight != 0.0)
   {
   ps->ptHeight = ptHeight;
   ps->yScale = (ptHeight - 2*ptMargin) / userHeight;
   }
else
   {
   ps->yScale = ps->xScale;
   ptHeight = ps->ptHeight = userHeight * ps->yScale + 2*ptMargin;
   }
/* 0.5, 0.5 is the center of the pixel in upper-left corner which corresponds to (0,0) */
ps->xOff = ptMargin;
ps->yOff = ptMargin;
ps->fontHeight = 10;

/* Cope with fact y coordinates are bottom to top rather
 * than top to bottom. */
ps->yScale = -ps->yScale;
ps->yOff = ps->ptHeight - ps->yOff;

psWriteHeader(ps->f, ptWidth, ptHeight);

/* adding a gsave here fixes an old ghostview bug with cliprestore */
fprintf(ps->f, "gsave\n");

/* Set initial clipping rectangle. */
psClipRect(ps, 0, 0, ps->userWidth, ps->userHeight);

/* Set line width to a single pixel. */
psSetLineWidth(ps,1);

return ps;
}

void psTranslate(struct psGfx *ps, double xTrans, double yTrans)
/* add a constant to translate all coordinates */
{
ps->xOff += xTrans*ps->xScale;   
ps->yOff += yTrans*ps->yScale;
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

void psLineTo(struct psGfx *ps, double x, double y)
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
if (width > 0 && height > 0)
    {
    psWhOut(ps, width, height);
    psXyOut(ps, x, y+height); 
    fprintf(ps->f, "fillBox\n");
    }
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
fprintf(f, "/Helvetica findfont ");

/* Note the 1.2 and the 1.0 below seem to get it to 
 * position about where the stuff developed for pixel
 * based systems expects it.  It is all a kludge though! */
fprintf(f, "%f scalefont setfont\n", -size*ps->yScale*1.2);
ps->fontHeight = size*0.8;
}

void psTextBox(struct psGfx *ps, double x, double y, char *text)
/* Output text in current font at given position. */
{
char c;
psMoveTo(ps, x, y + ps->fontHeight);
fprintf(ps->f, "(");
while ((c = *text++) != 0)
    {
    if (c == ')' || c == '(')
        fprintf(ps->f, "\\");
    fprintf(ps->f, "%c", c);
    }
fprintf(ps->f, ") fillTextBox\n");
}

void psTextAt(struct psGfx *ps, double x, double y, char *text)
/* Output text in current font at given position. */
{
char c;
psMoveTo(ps, x, y + ps->fontHeight);
fprintf(ps->f, "(");
while ((c = *text++) != 0)
    {
    if (c == ')' || c == '(')
        fprintf(ps->f, "\\");
    fprintf(ps->f, "%c", c);
    }
fprintf(ps->f, ") show\n");
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
char c;
y += (height - ps->fontHeight)/2;
psMoveTo(ps, x+width/2, y + ps->fontHeight);
fprintf(ps->f, "(");
while ((c = *text++) != 0)
    {
    if (c == ')' || c == '(')
        fprintf(ps->f, "\\");
    fprintf(ps->f, "%c", c);
    }
fprintf(ps->f, ") showMiddle\n");
}

void psTextDown(struct psGfx *ps, double x, double y, char *text)
/* Output text going downwards rather than across at position. */
{
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
fprintf(ps->f, "grestore\n");
}

void psDrawPoly(struct psGfx *ps, struct psPoly *poly, boolean filled)
/* Draw a possibly filled polygon */
{
FILE *f = ps->f;
struct psPoint *p = poly->ptList;
fprintf(f, "newpath\n");
psMoveTo(ps, p->x, p->y);
for (;;)
    {
    p = p->next;
    psLineTo(ps, p->x, p->y);
    if (p == poly->ptList)
	break;
    }
if (filled)
    {
    fprintf(f, "fill\n");
    }
else
    {
    fprintf(f, "closepath\n");
    fprintf(f, "stroke\n");
    }
}


void psFillEllipse(struct psGfx *ps, double x, double y, double xrad, double yrad)
{
FILE *f = ps->f;
fprintf(f, "newpath\n");
psXyOut(ps, x, y);
psWhOut(ps, xrad, yrad);
psFloatOut(f, 0.0);
psFloatOut(f, 360.0);
fprintf(f, "ellipse\n");
fprintf(f, "closepath\n");
fprintf(f, "fill\n");
}

void psDrawEllipse(struct psGfx *ps, double x, double y, double xrad, double yrad,
    double startAngle, double endAngle)
{
FILE *f = ps->f;
fprintf(f, "newpath\n");
psXyOut(ps, x, y);
psWhOut(ps, xrad, yrad);
psFloatOut(f, startAngle);
psFloatOut(f, endAngle);
fprintf(f, "ellipse\n");
fprintf(f, "closepath\n");
fprintf(f, "stroke\n");
}

char * convertEpsToPdf(char *epsFile) 
/* Convert EPS to PDF and return filename, or NULL if failure. */
{
char *pdfTmpName = NULL, *pdfName=NULL;
char cmdBuffer[2048];
int sysVal = 0;
struct lineFile *lf = NULL;
char *line;
int lineSize=0;
float width=0, height=0;
pdfTmpName = cloneString(epsFile);

/* Get the dimensions of bounding box. */
lf = lineFileOpen(epsFile, TRUE);
while(lineFileNext(lf, &line, &lineSize)) 
    {
    if(strstr( line, "BoundingBox:")) 
	{
	char *words[5];
	chopLine(line, words);
	width = atof(words[3]);
	height = atof(words[4]);
	break;
	}
    }
lineFileClose(&lf);
	
/* Do conversion. */
chopSuffix(pdfTmpName);
pdfName = addSuffix(pdfTmpName, ".pdf");
safef(cmdBuffer, sizeof(cmdBuffer), "ps2pdf -dDEVICEWIDTHPOINTS=%d -dDEVICEHEIGHTPOINTS=%d %s %s", 
      round(width), round(height), epsFile, pdfName);
sysVal = system(cmdBuffer);
if(sysVal != 0)
    freez(&pdfName);
freez(&pdfTmpName);
return pdfName;
}

