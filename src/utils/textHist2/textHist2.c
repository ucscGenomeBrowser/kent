/* textHist2 - Make two dimensional histogram. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psGfx.h"


int xBins = 12, yBins = 12;
int xBinSize = 1, yBinSize = 1;
int xMin = 0, yMin = 0;
char *psFile = NULL;
boolean doLog = FALSE;
int margin = 0;
int psSize = 5*72;
int labelStep = 1;
double postScale = 1.0;

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
  "   -labelStep=N - How many bins to skip between labels\n"
  "   -margin=N - Margin in points for PostScript output\n"
  "   -log    - Logarithmic output (only works with ps now)\n"
  "   -postScale=N (default %f) - What to scale by after normalization\n"
  , postScale
  );
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

void boxOut(struct psGfx *ps, int x, int y, int width, int height)
/* Draw a filled box in current color. */
{
psDrawBox(ps, x, y, width+1, height+1);
}


void psOutput(int *hist, char *psFile)
/* Output histogram in postscript format. */
{
double labelSize = 72.0/2;
double psInnerSize = psSize - labelSize;
double tickSize = labelSize/4;
double textSize = labelSize - tickSize;
struct psGfx *ps = psOpen(psFile, psSize, psSize, psSize, psSize, margin);
double val, maxVal = findMaxVal(hist);
int x, y, both;
double grayScale;
double xPos, yPos;
double xBinPts = psInnerSize/xBins;
double yBinPts = psInnerSize/yBins;

grayScale = 1.0 / maxVal * postScale;
for (y=0; y<yBins; ++y)
    {
    int yg = yBins-1-y;
    yPos = yg*yBinPts;
    for (x=0; x<xBins; ++x)
        {
	xPos = x * xBinPts + labelSize;
	both = y*xBins + x;
	val = mightLog(hist[both]) * grayScale;
	if (val > 1.0) val = 1.0;
	if (val < 0.0) val = 0.0;
	psSetGray(ps, 1.0 - val);
	boxOut(ps, xPos, yPos, xBinPts, yBinPts);
	}
    }

/* Draw ticks and labels. */
psSetGray(ps, 0);
yPos = psInnerSize;
for (x=labelStep; x<xBins; x += labelStep)
    {
    char buf[16];
    sprintf(buf, "%d", x*xBinSize+xMin);
    xPos = x * xBinPts + labelSize;
    psDrawBox(ps, xPos, yPos, 1, tickSize);
    psTextDown(ps, xPos, yPos+tickSize+2, buf);
    }
for (y=labelStep; y<yBins; y += labelStep)
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
labelStep = optionInt("labelStep", labelStep);
postScale = optionFloat("postScale", postScale);
textHist2(argv[1]);
return 0;
}
