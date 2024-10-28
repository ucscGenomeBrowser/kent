/* PostScript graphics - 
 * This provides a bit of a shell around writing graphics to
 * a postScript file.  Perhaps the most important thing it
 * does is convert 0,0 from being at the bottom left to
 * being at the top left. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "psPoly.h"
#include "psGfx.h"
#include "linefile.h"
#include "pipeline.h"
#include "iconv.h"

#include "reEncodeFont.c"  // a windows 1252 encoding

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

static void psWriteHeader(struct psGfx *ps, double width, double height)
/* Write postScript header.  It's encapsulated PostScript
 * actually, so you need to supply image width and height
 * in points. */
{
FILE *f = ps->f;
char *s =
#include "common.pss"
;

fprintf(f, "%%!PS-Adobe-3.1 EPSF-3.0\n");
fprintf(f, "%%%%BoundingBox: 0 0 %d %d\n", (int)ceil(width), (int)ceil(height));
fprintf(f, "%%%%LanguageLevel: 3\n\n");
if (ps->newTransOps)
    fprintf(f, "0 .pushpdf14devicefilter\n");
fprintf(f, "%s", s);
}

void psSetLineWidth(struct psGfx *ps, double factor)
/* Set line width to factor * a single pixel width. */
{
fprintf(ps->f, "%f setlinewidth\n", factor * ps->xScale);
}

boolean supportNewTrans(void)
/* Check the version of GS and return whether it supports the newer transparency operators */
{
boolean support = FALSE;
char *versionCmd[] = { "gs", "--version", NULL };
struct pipeline *pl = pipelineOpen1(versionCmd, pipelineRead, "/dev/null", NULL, 0);
if (pl)
    {
    struct lineFile *lf = pipelineLineFile(pl);
    char *versionString;
    lineFileNext(lf, &versionString, NULL);
    char *minorVersion = NULL;
    if (strchr(versionString, '.'))
        minorVersion = strchr(versionString, '.') + 1;
    if ((atoi(versionString) > 9) || (atoi(versionString) == 9 && minorVersion && atoi(minorVersion) > 51))
        support = TRUE;
    pipelineClose(&pl);
    }
return support;
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

ps->newTransOps = supportNewTrans();
psWriteHeader(ps, ptWidth, ptHeight);

/* adding a gsave here fixes an old ghostview bug with cliprestore */
fprintf(ps->f, "gsave\n");

/* put out the Windows 1252 encoding */
fputs(encodeFont, ps->f);

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
    if (ps->newTransOps)
        fprintf(ps->f, ".poppdf14devicefilter\n");
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

void psSetFont(struct psGfx *ps, char *fontName, double size)
{
FILE *f = ps->f;
fprintf(f, "/%s-Win /%s  reencodefont ", fontName, fontName);

/* Note the 1.2 and the 1.0 below seem to get it to 
 * position about where the stuff developed for pixel
 * based systems expects it.  It is all a kludge though! */
fprintf(f, "%f scalefont setfont\n", -size*ps->yScale*1.2);
ps->fontHeight = size*0.8;
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

static iconv_t ourIconv; // convert context UTF-8 > Windows 1252

static size_t utf8ToWindows1252(char *text, unsigned char *buffer, size_t nLength)
/* Convert UTF-8 string to Unicode string. */
{
if (ourIconv == NULL)
    {
    ourIconv = iconv_open("WINDOWS-1252//TRANSLIT", "UTF-8");
        
    if (ourIconv == NULL)
        errAbort("iconv problem errno %d\n",errno);
    }

size_t length = strlen(text);
char *newText = (char *)buffer;

int ret = iconv(ourIconv, &text, &length, &newText,  &nLength);

if (ret < 0)
    errAbort("iconv problem errno %d\n",errno);

return (newText - (char *)buffer);
}

void psTextOutEscaped(struct psGfx *ps, char *text)
/* Output post-script-escaped text. 
 * Notice that ps uses escaping similar to C itself.*/
{
size_t length = strlen(text);
unsigned char buff[length * 2];  // should be way too much

length = utf8ToWindows1252(text, buff, length * 4);
buff[length] = '\0';

text = (char *)buff;
char c;
while ((c = *text++) != 0)
    {
    if (c == '(')
        fprintf(ps->f, "\\(");  // left-paren
    else if (c == ')')
        fprintf(ps->f, "\\)");  // right-paren
    else if (c == '\\')
        fprintf(ps->f, "\\\\"); // back-slash
    else if (c == '\n')
        fprintf(ps->f, "\\n");  // new-line
    else if (c == '\r')
        fprintf(ps->f, "\\r");  // carriage-return
    else if (c == '\t')
        fprintf(ps->f, "\\t");  // tab
    else if (c == '\b')
        fprintf(ps->f, "\\b");  // back-space
    else if (c == '\f')
        fprintf(ps->f, "\\f");  // form-feed
    else
	fprintf(ps->f, "%c", c);
    }
}

void psTextBox(struct psGfx *ps, double x, double y, char *text)
/* Output text in current font at given position. */
{
psMoveTo(ps, x, y + ps->fontHeight);
fprintf(ps->f, "(");
psTextOutEscaped(ps, text);
fprintf(ps->f, ") fillTextBox\n");
}

void psTextAt(struct psGfx *ps, double x, double y, char *text)
/* Output text in current font at given position. */
{
psMoveTo(ps, x, y + ps->fontHeight);
fprintf(ps->f, "(");
psTextOutEscaped(ps, text);
fprintf(ps->f, ") show\n");
}

void psTextRight(struct psGfx *ps, double x, double y, 
	double width, double height, 
	char *text)
/* Draw a line of text right justified in box defined by x/y/width/height */
{
y += (height - ps->fontHeight)/2;
psMoveTo(ps, x+width, y + ps->fontHeight);
fprintf(ps->f, "(");
psTextOutEscaped(ps, text);
fprintf(ps->f, ") showBefore\n");
}

void psTextInBox(struct psGfx *ps, double x, double y, 
	double width, double height, 
	char *text)
/* Draw a line of text that fills a box. */
{
if (height == 0)
    return;
if (height > 0)
    {
    psMoveTo(ps, x, y + height);
    fprintf(ps->f, "gsave\n");
    psSetFont(ps, "Courier", 1.0);
    fprintf(ps->f, "%g %g scale\n", width,height);
    fprintf(ps->f, "(");
    psTextOutEscaped(ps, text);
    fprintf(ps->f, ") show\n");
    fprintf(ps->f, "grestore\n");
    }
else
    {
    //height = -height;
    psMoveTo(ps, x, y);
    fprintf(ps->f, "gsave\n");
    psSetFont(ps, "Courier", 1.0);
    fprintf(ps->f, "%g %g scale\n", width,height);
    fprintf(ps->f, "(");
    psTextOutEscaped(ps, text);
    fprintf(ps->f, ") show\n");
    fprintf(ps->f, "grestore\n");
    }
}

void psTextCentered(struct psGfx *ps, double x, double y, 
	double width, double height, 
	char *text)
/* Draw a line of text centered in box defined by x/y/width/height */
{
y += (height - ps->fontHeight)/2;
psMoveTo(ps, x+width/2, y + ps->fontHeight);
fprintf(ps->f, "(");
psTextOutEscaped(ps, text);
fprintf(ps->f, ") showMiddle\n");
}

void psTextDown(struct psGfx *ps, double x, double y, char *text)
/* Output text going downwards rather than across at position. */
{
psMoveTo(ps, x, y);
fprintf(ps->f, "gsave\n");
fprintf(ps->f, "-90 rotate\n");
fprintf(ps->f, "(");
psTextOutEscaped(ps, text);
fprintf(ps->f, ") show\n");
fprintf(ps->f, "grestore\n");
}

void psSetColorAlpha(struct psGfx *ps, int a)
/* Sets alpha in the output if transparency is enabled, otherwise does nothing */
{
FILE *f = ps->f;
double scale = 1.0/255;
if (ps->newTransOps)
    {
    fprintf(f, "%f .setfillconstantalpha\n", scale * a);
    fprintf(f, "%f .setstrokeconstantalpha\n", scale * a);
    }
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

void psCircle(struct psGfx *ps, double x, double y, double rad, boolean filled)
{
FILE *f = ps->f;
fprintf(f, "newpath\n");
psXyOut(ps, x, y);
psFloatOut(f, rad * ps->xScale);
psFloatOut(f, 0.0);
psFloatOut(f, 360.0);
fprintf(f, "arc\n");
fprintf(f, "closepath\n");
if (filled)
    fprintf(f, "fill\n");
else
    fprintf(f, "stroke\n");
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
/* Draw ellipse.  Args are center point x and y, horizontal radius, vertical radius,
        start and end angles (e.g. 0 and 180 to draw top half, 180 and 360 for bottom)
 */
{
FILE *f = ps->f;
fprintf(f, "newpath\n");
psXyOut(ps, x, y);
psWhOut(ps, xrad, yrad);
psFloatOut(f, startAngle);
psFloatOut(f, endAngle);
fprintf(f, "ellipse\n");
fprintf(f, "stroke\n");
}

void psDrawCurve(struct psGfx *ps, double x1, double y1, double x2, double y2,
                                        double x3, double y3, double x4, double y4)
/* Draw Bezier curve specified by 4 points: first (p1) and last (p4)
 *      and 2 control points (p2, p3) */
{
FILE *f = ps->f;
psXyOut(ps, x1, y1);
fprintf(f, "moveto\n");
psXyOut(ps, x2, y2);
psXyOut(ps, x3, y3);
psXyOut(ps, x4, y4);
fprintf(f, "curveto\n");
fprintf(f, "stroke\n");
}

void psSetDash(struct psGfx *ps, boolean on)
/* Set dashed line mode on or off. If set on, show two points marked, with one point of space */
{
FILE *f = ps->f;
if (on)
    fprintf(f, "[2 1] 0 setdash\n");
else
    fprintf(f, "[] 0 setdash\n");
}

char * convertEpsToPdf(char *epsFile) 
/* Convert EPS to PDF and return filename, or NULL if failure. */
{
char *pdfTmpName = NULL, *pdfName=NULL;
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
char widthBuff[1024], heightBuff[1024];
safef(widthBuff, sizeof widthBuff, "-dDEVICEWIDTHPOINTS=%d", round(width));
safef(heightBuff, sizeof heightBuff, "-dDEVICEHEIGHTPOINTS=%d", round(height));
struct pipeline *pl = NULL;
boolean newTrans = supportNewTrans();
if (newTrans)
    {
    char *pipeCmd[] = { "ps2pdf", "-dALLOWPSTRANSPARENCY", widthBuff, heightBuff, epsFile, pdfName, NULL } ;
    pl = pipelineOpen1(pipeCmd, pipelineWrite, "/dev/null", NULL, 0);
    }
else
    {
    char *pipeCmd[] = { "ps2pdf", widthBuff, heightBuff, epsFile, pdfName, NULL } ;
    pl = pipelineOpen1(pipeCmd, pipelineWrite, "/dev/null", NULL, 0);
    }
sysVal = pipelineWait(pl);
if(sysVal != 0)
    freez(&pdfName);
freez(&pdfTmpName);
return pdfName;
}

