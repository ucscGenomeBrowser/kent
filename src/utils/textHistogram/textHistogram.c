/* textHistogram - Make a histogram in ascii. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

int binSize = 1;
int maxBinCount = 25;
int minVal = 0;

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
for (i=minData; i<=maxData; ++i)
    {
    int j;
    int count = hist[i];
    int astCount = round(count * 60.0 / maxCount);
    printf("%3d ", i*binSize + minVal);
    for (j=0; j<astCount; ++j)
        putchar('*');
    printf(" %d\n", count);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
binSize = optionInt("binSize", binSize);
maxBinCount = optionInt("maxBinCount", maxBinCount);
minVal = optionInt("minVal", minVal);
if (argc != 2)
    usage();
textHistogram(argv[1]);
return 0;
}
