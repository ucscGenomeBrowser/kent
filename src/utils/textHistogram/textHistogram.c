/* textHistogram - Make a histogram in ascii. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: textHistogram.c,v 1.10 2003/12/10 00:27:31 hiram Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"binSize", OPTION_STRING},
    {"maxBinCount", OPTION_INT},
    {"minVal", OPTION_STRING},
    {"log", OPTION_BOOLEAN},
    {"noStar", OPTION_BOOLEAN},
    {"col", OPTION_INT},
    {"aveCol", OPTION_INT},
    {"real", OPTION_BOOLEAN},
    {"verbose", OPTION_BOOLEAN},
    {NULL, 0}
};

int binSize = 1;
double binSizeR = 1.0;
int maxBinCount = 25;
char * minValStr = (char *) NULL;
int minVal = 0;
double minValR = 0.0;
boolean doLog = FALSE;
boolean noStar = FALSE;
int col = 0;
int aveCol = -1;
boolean real = FALSE;
boolean verbose = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "textHistogram - Make a histogram in ascii\n"
  "usage:\n"
  "   textHistogram [options] inFile\n"
  "Where inFile contains one number per line.\n"
  "  options:\n"
  "   -binSize=N - Size of bins, default 1\n"
  "   -maxBinCount=N - Maximum # of bins, default 25\n"
  "   -minVal=N - Minimum value to put in histogram, default 0\n"
  "   -log - Do log transformation before plotting\n"
  "   -noStar - Don't draw asterisks\n"
  "   -col=N - Which column to use. Default 1\n"
  "   -aveCol=N - A second column to average over. The averages\n"
  "             will be output in place of counts of primary column.\n"
  "   -real - Data input are real values (default is integer)\n"
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
int truncation = 0;

/* Allocate histogram and optionally space for
 * second column totals. */
AllocArray(hist, maxBinCount);
if (aveCol >= 0)
    AllocArray(total, maxBinCount);

/* Go through each line of input file accumulating
 * data. */
while (wordCount = lineFileChop(lf, row))
    {
    int x;	/*	will become the index into hist[]	*/
    if (wordCount <= col || wordCount <= aveCol)
        errAbort("Not enough words line %d of %s", lf->lineIx, lf->fileName);
    x = -1;
    if (real)	/*	for real data, work in real space to find index */
	{
	double d;
	d = lineFileNeedDouble(lf, row, col);
	if (d >= minValR)
	    {
	    d -= minValR;
	    x = (int) (d / binSizeR);
	    }
	}
    else
	{
	x = lineFileNeedNum(lf, row, col);
	if (x >= minVal)
	    {
	    x -= minVal;
	    x /= binSize;
	    }
	}
    /*	index x is calculated, accumulate it, if in range	*/
    if (x >= 0 && x < maxBinCount)
	{
	hist[x] += 1;
	if (aveCol >= 0)
	    {
	    double a;
	    a = lineFileNeedDouble(lf, row, aveCol);
	    total[x] += a;
	    }
	}
	else
	    truncation = (x > truncation) ? x : truncation;
    }

if (truncation > 0)
    {
    if (real)
	fprintf(stderr,"large values truncated: need %d bins or larger binSize than %g\n",truncation, binSizeR);
    else
	fprintf(stderr,"large values truncated: need %d bins or larger binSize than %d\n",truncation, binSize);
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
    double binStartR = 0.0;
    int binStart = 0;

    if (real)
	binStartR = i*binSizeR + minValR;
    else
	binStart = i*binSize + minVal;

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
	{
	if (real)
	    printf("%3d %g:%g\t%f\n", i, binStartR, binStartR+binSizeR, ct);
	else
	    printf("%d\t%f\n", binStart, ct);
	}
    else
	{
	int astCount = round(ct * 60.0 / maxCt);
	if (real)
	    printf("%g ", binStartR);
	else
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
char * binSizeStr = (char *) NULL;

optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();

binSizeStr = optionVal("binSize", "1");
maxBinCount = optionInt("maxBinCount", 25);
minValStr = optionVal("minVal", "0");
doLog = optionExists("log");
noStar = optionExists("noStar");
col = optionInt("col", 1) - 1;
aveCol = optionInt("aveCol", 0) - 1;
real = optionExists("real");
verbose = optionExists("verbose");

if (real)
    {
    char *valEnd;
    char *val = binSizeStr;
    binSizeR = strtod(val, &valEnd);
    if ((*val == '\0') || (*valEnd != '\0'))
	errAbort("Not a valid float for -binSize=%s\n", binSizeStr);
    val = minValStr;
    minValR = strtod(val, &valEnd);
    if ((*val == '\0') || (*valEnd != '\0'))
	errAbort("Not a valid float for -minVal=%s\n", binSizeStr);
    binSize = binSizeR;
    minVal = minValR;
    }
else
    {
	binSize = atoi(binSizeStr);
	minVal = atoi(minValStr);
    }

if (verbose)
    {
    fprintf(stderr, "#\tverbose on, options:\n");
    fprintf(stderr, "#\tbinSize: ");
    if (real) fprintf(stderr, "%f\n", binSizeR);
	else fprintf(stderr, "%d\n", binSize);
    fprintf(stderr, "#\tmaxBinCount: %d\n", maxBinCount);
    fprintf(stderr, "#\tminVal: ");
    if (real) fprintf(stderr, "%f\n", minValR);
	else fprintf(stderr, "%d\n", minVal);
    fprintf(stderr, "#\tlog function: %s\n", doLog ? "ON" : "OFF" );
    fprintf(stderr, "#\tdraw asterisks: %s\n", noStar ? "NO" : "YES" );
    fprintf(stderr, "#\thistogram on data input column: %d\n", col+1);
    if (aveCol >= 0)
	fprintf(stderr, "#\taveCol: %d\n", aveCol);
    else
	fprintf(stderr, "#\taveCol: not selected\n");
    fprintf(stderr, "#\treal valued data: %s\n", real ? "YES" : "NO" );
    }

textHistogram(argv[1]);
return 0;
}
