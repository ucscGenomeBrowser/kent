/* textHist2 - Make two dimensional histogram. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

int xBins = 12, yBins = 12;
int xBinSize = 1, yBinSize = 1;
int xMin = 0, yMin = 0;
char *psFile = NULL;
boolean doLog = FALSE;
int margin = 0;
int psSize = 5*72;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "textHist2 - Make two dimensional histogram table out\n"
  "of a list of 2-D points, one per line.\n"
  "usage:\n"
  "   textHist2 input\n"
  "options:\n"
  "   -xBins=N - number of bins in x dimension\n"
  "   -yBins=N - number of bins in y dimension\n"
  "   -xBinSize=N - size of bins in x dimension\n"
  "   -yBinSize=N - size of bins in x dimension\n"
  "   -xMin=N - minimum x number to record\n"
  "   -yMin=N - minimum y number to record\n"
  "   -ps=output.ps - make PostScript output\n"
  "   -psSize=N - Size in points (1/72th of inch)\n"
  "   -margin=N - Margin in points for PostScript output\n"
  "   -log    - Logarithmic output (only works with ps now)\n"
  );
}


struct psOutput 
/* A postScript output file. */
    {
    FILE *f;                    /* File to write to. */
    int pixWidth, pixHeight;	/* Size of image in virtual pixels. */
    double ptWidth, ptHeight;   /* Size of image in points (1/72 of an inch) */
    double xScale, yScale;      /* Conversion from pixels to points. */
    double xOff, yOff;          /* Offset from pixels to points. */
    };

void psFloatOut(FILE *f, double x)
/* Write out a floating point number, but not in too much
 * precision. */
{
int i = round(x);
if (i == x)
   fprintf(f, "%d ", i);
else
   fprintf(f, "%0.3f ", x);
}

void psWriteHeader(FILE *f, double width, double height)
/* Write postScript header.  It's encapsulated PostScript
 * actually, so you need to supply image width and height
 * in points. */
{
char *s =
#include "common.pss"
;

fprintf(f, "%%!PS-Adobe-3.1 EPSF-3.0\n");
fprintf(f, "%%%%BoundingBox: 0 0 ");
psFloatOut(f, width);
psFloatOut(f, height);
fprintf(f, "\n\n");
fprintf(f, "%s", s);
}

struct psOutput *psOpen(char *fileName, 
	int pixWidth, int pixHeight, 	 /* Dimension of image in pixels. */
	double ptWidth, double ptHeight, /* Dimension of image in points. */
	double ptMargin)                 /* Image margin in points. */
/* Open up a new postscript file.  If ptHeight is 0, it will be
 * calculated to keep pixels square. */
{
struct psOutput *ps;

/* Allocate structure and open file. */
AllocVar(ps);
ps->f = mustOpen(fileName, "w");
psWriteHeader(ps->f, ptWidth, ptHeight);

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
   ps->ptHeight = pixHeight * ps->yScale + 2*ptMargin;
   }
ps->xOff = ptMargin;
ps->yOff = ptMargin;

/* Cope with fact y coordinates are bottom to top rather
 * than top to bottom. */
ps->yScale = -ps->yScale;
ps->yOff = ps->ptHeight - ps->yOff;

return ps;
}

void psClose(struct psOutput **pPs)
/* Close out postScript file. */
{
struct psOutput *ps = *pPs;
if (ps != NULL)
    {
    carefulClose(&ps->f);
    freez(pPs);
    }
}

void psXyOut(struct psOutput *ps, int x, int y)
/* Output x,y position transformed into PostScript space. */
{
FILE *f = ps->f;
psFloatOut(f, x * ps->xScale + ps->xOff);
psFloatOut(f, y * ps->yScale + ps->yOff);
}

void psWhOut(struct psOutput *ps, int width, int height)
/* Output width/height transformed into PostScript space. */
{
FILE *f = ps->f;
psFloatOut(f, width * ps->xScale);
psFloatOut(f, height * -ps->yScale);
}

void psDrawBox(struct psOutput *ps, int x, int y, int width, int height)
/* Draw a filled box in current color. */
{
psWhOut(ps, width, height);
psXyOut(ps, x, y+height);
fprintf(ps->f, "fillBox\n");
}

void psMoveTo(struct psOutput *ps, int x, int y)
/* Move PostScript position to given point. */
{
psXyOut(ps, x, y);
fprintf(ps->f, "moveto\n");
}

void psTextAt(struct psOutput *ps, int x, int y, char *text)
/* Output text in current font at given position. */
{
psMoveTo(ps, x, y);
fprintf(ps->f, "(%s) show\n", text);
}

void psTextDown(struct psOutput *ps, int x, int y, char *text)
/* Output text going downwards rather than across at position. */
{
FILE *f = ps->f;
psMoveTo(ps, x, y);
fprintf(ps->f, "gsave\n");
fprintf(ps->f, "-90 rotate\n");
fprintf(ps->f, "(%s) show\n", text);
fprintf(ps->f, "grestore\n");
}

void psSetColor(struct psOutput *ps, int r, int g, int b)
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

void psSetGray(struct psOutput *ps, double grayVal)
/* Set gray value. */
{
FILE *f = ps->f;
if (grayVal < 0) grayVal = 0;
if (grayVal > 1) grayVal = 1;
psFloatOut(f, grayVal);
fprintf(f, "setgray\n");
}

double mightLog(double val)
/* Put val through a log transform if doLog is set. */
{
if (doLog)
    val = log(1+val);
return val;
}


double findMaxVal(int *hist)
/* Find maximum val in histogram. */
{
int bothBins = xBins * yBins;
int i;
double val, maxVal = 0;
for (i=0; i<bothBins; ++i)
    {
    val = mightLog(hist[i]);
    if (maxVal < val)
        maxVal = val;
    }
return maxVal;
}

void psOutput(int *hist, char *psFile)
/* Output histogram in postscript format. */
{
double labelSize = 72.0/2;
double psInnerSize = psSize - labelSize;
double tickSize = labelSize/4;
double textSize = labelSize - tickSize;
struct psOutput *ps = psOpen(psFile, psSize, psSize, psSize, psSize, margin);
double val, maxVal = findMaxVal(hist);
int x, y, both;
double grayScale;
double xPos, yPos, xMid, yMid;
double xBinPts = psInnerSize/xBins;
double yBinPts = psInnerSize/yBins;

grayScale = 1.0 / maxVal;
for (y=0; y<yBins; ++y)
    {
    int yg = yBins-1-y;
    yPos = yg*yBinPts;
    for (x=0; x<xBins; ++x)
        {
	xPos = x * xBinPts + labelSize;
	both = y*xBins + x;
	val = mightLog(hist[both]);
	psSetGray(ps, 1.0 - val * grayScale);
	psDrawBox(ps, xPos, yPos, xBinPts, yBinPts);
	}
    }

/* Draw ticks and labels. */
psSetGray(ps, 0);
yPos = psInnerSize;
for (x=1; x<xBins; ++x)
    {
    char buf[16];
    sprintf(buf, "%d", x*xBinSize+xMin);
    xPos = x * xBinPts + labelSize;
    xMid = xPos;
    psDrawBox(ps, xPos, yPos, 1, tickSize);
    psTextDown(ps, xPos, yPos+tickSize+2, buf);
    }
for (y=1; y<yBins; ++y)
    {
    char buf[16];
    int yg = yBins-y;
    sprintf(buf, "%d", y*yBinSize+yMin);
    yPos = yg*yBinPts;
    psDrawBox(ps, textSize, yPos, tickSize, 1);
    psTextAt(ps, 0, yPos, buf);
    }
psClose(&ps);
}

void textOutput(int *hist)
/* Output 2D histogram as 2D text table. */
{
int x, y, both;
int *digits = needMem(xBins * sizeof(int));

/* Figure out longest number in each column. */
for (x=0; x<xBins; ++x)
    {
    int dig, maxDig = 0;
    char s[32];
    for (y=0; y<yBins; ++y)
	{
	both = y*xBins + x;
	dig = sprintf(s, "%d", hist[both]);
	if (maxDig < dig) maxDig = dig;
	}
    digits[x] = maxDig;
    }


for (y=0; y<yBins; ++y)
    {
    for (x=0; x<xBins; ++x)
	{
	both = y*xBins + x;
	printf("%*d ", digits[x], hist[both]);
	}
    printf("\n");
    }
freez(&digits);
}

void textHist2(char *inFile)
/* textHist2 - Make two dimensional histogram. */
{
int bothBins = xBins * yBins;
int *hist = needMem(bothBins * sizeof(int));
char *row[2];
struct lineFile *lf = lineFileOpen(inFile, TRUE);
int x, y, both;


/* Read file and make counts. */
while (lineFileRow(lf, row))
    {
    x = lineFileNeedNum(lf, row, 0);
    y = lineFileNeedNum(lf, row, 1);
    if (x >= xMin && y >= yMin)
	{
	x = (x - xMin)/xBinSize;
	y = (y - yMin)/yBinSize;
	if (x >= 0 && x < xBins && y >= 0 && y < yBins)
	    {
	    both = y*xBins + x;
	    ++hist[both];
	    }
	}
    }

/* Output it. */
if (psFile != NULL)
    psOutput(hist, psFile);
else
    textOutput(hist);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
xBins = optionInt("xBins", xBins);
yBins = optionInt("yBins", yBins);
xBinSize = optionInt("xBinSize", xBinSize);
yBinSize = optionInt("yBinSize", yBinSize);
xMin = optionInt("xMin", xMin);
yMin = optionInt("yMin", yMin);
psFile = optionVal("ps", NULL);
psSize = optionInt("psSize", psSize);
margin = optionInt("margin", margin);
doLog = optionExists("log");
textHist2(argv[1]);
return 0;
}
