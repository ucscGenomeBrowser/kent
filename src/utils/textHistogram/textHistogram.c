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
  );
}

void textHistogram(char *inFile)
/* textHistogram - Make a histogram in ascii. */
{
int *hist;
char *row[1];
struct lineFile *lf = lineFileOpen(inFile, TRUE);
int i;
int minData = maxBinCount, maxData = 0;
int maxCount = 0;
double maxCt;
AllocArray(hist, maxBinCount);
while (lineFileRow(lf, row))
    {
    int x = lineFileNeedNum(lf, row, 0);
    if (x >= minVal)
	{
	x -= minVal;
	x /= binSize;
	if (x >= 0 && x < maxBinCount)
	    hist[x] += 1;
	}
    }
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
if (doLog)
    maxCt = log(maxCt);
if (noStar)
    {
    for (i=minData; i<=maxData; ++i)
	printf("%d\t%d\n", i*binSize + minVal, hist[i]);
    }
else
    {
    for (i=minData; i<=maxData; ++i)
	{
	int j;
	int count = hist[i];
	double ct = count;
	int astCount;

	if (doLog)
	    ct = log(ct);
	astCount = round(ct * 60.0 / maxCt);
	printf("%3d ", i*binSize + minVal);
	for (j=0; j<astCount; ++j)
	    putchar('*');
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
if (argc != 2)
    usage();
textHistogram(argv[1]);
return 0;
}
