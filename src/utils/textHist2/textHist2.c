/* textHist2 - Make two dimensional histogram. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

int xBins = 12, yBins = 12;
int xBinSize = 1, yBinSize = 1;
int xMin = 0, yMin = 0;

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
  );
}

void textHist2(char *inFile)
/* textHist2 - Make two dimensional histogram. */
{
int bothBins = xBins * yBins;
int *hist = needMem(bothBins * sizeof(int));
char *row[2];
struct lineFile *lf = lineFileOpen(inFile, TRUE);
int x, y, both;
int *digits = needMem(xBins);

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
textHist2(argv[1]);
return 0;
}
