/* textHistogram - Make a histogram in ascii. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

int binSize = 1;
int maxBinCount = 25;
int minVal = 0;
boolean doLog = FALSE;
boolean noStar = FALSE;
int col = 0;
int aveCol = -1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "textHistogram - Make a histogram in ascii\n"
  "usage:\n"
  "   textHistogram inFile\n"
  "Where inFile contains one number per line."
  "options:\n"
  "   -binSize=N  Size of bins, default 1\n"
  "   -maxBinCount=N  Maximum # of bins, default 25\n"
  "   -minVal=N  Minimum value to put in histogram, default 0\n"
  "   -log Do log transformation before plotting\n"
  "   -noStar Don't draw asterisks\n"
  "   -col=N Which column to use. Default 1\n"
  "   -aveCol=N A second column to average over. The averages\n"
  "             will be output in place of counts of primary column.\n"
  );
}

void textHistogram(char *inFile)
/* textHistogram - Make a histogram in ascii. */
{
double *hist = NULL;
double *total = NULL;
char *row[256];
int wordCount;
struct lineFile *lf = lineFileOpen(inFile, TRUE);
int i,j;
int minData = maxBinCount, maxData = 0;
double maxCount = 0;
double maxCt;

/* Allocate histogram and optionally space for
 * second column totals. */
AllocArray(hist, maxBinCount);
if (aveCol >= 0)
    AllocArray(total, maxBinCount);

/* Go through each line of input file accumulating
 * data. */
while (wordCount = lineFileChop(lf, row))
    {
    int x;
    if (wordCount <= col || wordCount <= aveCol)
        errAbort("Not enough words line %d of %s", lf->lineIx, lf->fileName);
    x = lineFileNeedNum(lf, row, col);
    if (x >= minVal)
	{
	x -= minVal;
	x /= binSize;
	if (x >= 0 && x < maxBinCount)
	    {
	    hist[x] += 1;
	    if (aveCol >= 0)
	        total[x] += atof(row[aveCol]);
	    }
	}
    }

/* Figure out range that has data, maximum data
 * value and optionally compute averages. */
if (aveCol >= 0)
    {
    double ave, maxAve = -BIGNUM;
    for (i=0; i<maxBinCount; ++i)
	{
	int count = hist[i];
	if (count != 0)
	    {
	    ave = total[i]/count;
	    if (maxAve < ave) maxAve = ave;
	    if (minData > i) minData = i;
	    if (maxData < i) maxData = i;
	    }
	}
    maxCt = maxAve;
    }
else
    {
    for (i=0; i<maxBinCount; ++i)
	{
	int count = hist[i];
	if (count != 0)
	    {
	    if (maxCount < count) maxCount = count;
	    if (minData > i) minData = i;
	    if (maxData < i) maxData = i;
	    }
	}
    maxCt = maxCount;
    }
if (doLog)
    maxCt = log(maxCt);

/* Output results. */
for (i=minData; i<=maxData; ++i)
    {
    int count = hist[i];
    double ct;
    int binStart = i*binSize + minVal;

    if (aveCol >= 0)
	{
	if (count > 0)
	    ct = total[i]/count;
	else
	    ct = 0;
	}
    else
	{
	ct = count;
	}
    if (doLog)
	ct = log(ct);
    if (noStar)
	printf("%d\t%f\n", binStart, ct);
    else
	{
	int astCount = round(ct * 60.0 / maxCt);
	printf("%3d ", binStart);
	for (j=0; j<astCount; ++j)
	    putchar('*');
	if (aveCol >= 0)
	    printf(" %f\n", ct);
	else
	    printf(" %d\n", count);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
binSize = optionInt("binSize", binSize);
maxBinCount = optionInt("maxBinCount", maxBinCount);
minVal = optionInt("minVal", minVal);
doLog = optionExists("log");
noStar = optionExists("noStar");
if (optionExists("col"))
    col = optionInt("col", col) - 1;
if (optionExists("aveCol"))
    aveCol = optionInt("aveCol", aveCol) - 1;
if (argc != 2)
    usage();
textHistogram(argv[1]);
return 0;
}
