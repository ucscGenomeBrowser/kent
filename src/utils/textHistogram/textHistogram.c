/* textHistogram - Make a histogram in ascii. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


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
    {"autoscale", OPTION_INT},
    {"autoScale", OPTION_INT},
    {"skip", OPTION_INT},
    {"probValues", OPTION_BOOLEAN},
    {"freq", OPTION_BOOLEAN},
    {NULL, 0}
};

int binSize = 1;
double binSizeR = 1.0;
int maxBinCount = 25;
int skip = 0;
char * minValStr = (char *) NULL;
int minVal = 0;
double minValR = 0.0;
boolean doLog = FALSE;
boolean noStar = FALSE;
boolean probValues = FALSE;
int col = 0;
int aveCol = -1;
boolean real = FALSE;
int autoscale = 0;
boolean freq = FALSE;

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
  "   -autoScale=N - autoscale to N # of bins\n"
  "   -probValues - show prob-Values (density and cum.distr.) (sets -noStar too)\n"
  "   -freq - show frequences instead of counts\n"
  "   -skip=N - skip N lines before starting, default 0\n"
  );
}

/*	Read through the file and determine min,max and thus range
 *	set bin size and minimum value
 */
static void autoScale(char *inFile)
{
int wordCount;
char *row[256];
unsigned long dataCount = 0;
double min = HUGE;
double max = - HUGE;
double range = 0.0;
struct lineFile *lf = lineFileOpen(inFile, TRUE);

while ((wordCount = lineFileChop(lf, row)))
    {
    double d;
    if ((wordCount <= col) || (wordCount <= aveCol))
        errAbort("Not enough words line %d of %s", lf->lineIx, lf->fileName);
    d = lineFileNeedDouble(lf, row, col);
    if ( d < min ) min = d;
    if ( d > max ) max = d;
    ++dataCount;
    }
lineFileClose(&lf);

range = max - min;

if (range < 0.0)
        errAbort("range of data invalid: %g = [%g:%g]", range, min, max);

maxBinCount = autoscale;
if (real)
    {
    minValR = min;
    /*	need to make binSizeR slightly larger to get the last data point
     *	in the last bin.  This is a floating point round off situation.
     */
    binSizeR = (range + (range/1000000.0)) / maxBinCount;
    }
else
    {
    minVal = (int) floor(min);
    binSize = (int)ceil(range / maxBinCount);
    if (binSize < 1) binSize = 1;
    verbose(1, "#\tautoscale data range: (%d - %d)/%d = %d\n",
	(int) ceil(max), minVal, maxBinCount, binSize);
    }
verbose(2, "#\tautoscale number of data values: %lu\n", dataCount);
    
verbose(2, "#\tautoscale maxBinCount: %d\n", maxBinCount);
if (real)
    {
    verbose(2, "#\tautoscale data range: %g = [%g:%g]\n",
	range, minValR, max);
    verbose(2, "#\tautoscale minVal: %g\n", minValR);
    verbose(2, "#\tautoscale binSize: %g\n", binSizeR);
    }
else
    {
    verbose(2, "#\tautoscale data range: %g = [%d:%d]\n",
	range, minVal, (int) ceil(max));
    verbose(2, "#\tautoscale minVal: %d\n", minVal);
    verbose(2, "#\tautoscale binSize: %d\n", binSize);
    }
}	/*	autoScale()	*/

static void textHistogram(char *inFile)
/* textHistogram - Make a histogram in ascii. */
{
double *hist = NULL;
double *total = NULL;
char *row[256];
int wordCount;
struct lineFile *lf = lineFileOpen(inFile, TRUE);
int i,j;
int minData = maxBinCount, maxData = 0;
int totalTooBig = 0;
double maxCount = 0;
double maxCt;
double maxVal = 0;
int truncation = 0;
int begin, end;
unsigned long long totalCounts = 0;
double cpd;

/* Allocate histogram and optionally space for
 * second column totals. */
AllocArray(hist, maxBinCount);
if (aveCol >= 0)
    AllocArray(total, maxBinCount);

while (skip-- > 0)
wordCount = lineFileChop(lf, row);

/* Go through each line of input file accumulating
 * data. */
while ((wordCount = lineFileChop(lf, row)))
    {
    int x;	/*	will become the index into hist[]	*/
    if (wordCount <= col || wordCount <= aveCol)
        errAbort("Not enough words line %d of %s", lf->lineIx, lf->fileName);
    x = -1;
    if (real)	/*	for real data, work in real space to find index */
	{
	double d;
	d = lineFileNeedDouble(lf, row, col);
	if (d > maxVal)
	    maxVal = d;
	if (d >= minValR)
	    {
	    d -= minValR;
	    x = (int) floor(d / binSizeR);
	    }
	}
    else
	{
	x = lineFileNeedNum(lf, row, col);
	if (x > maxVal)
	    maxVal = x;
	if (x >= minVal)
	    {
	    x -= minVal;
	    x /= binSize;
	    }
	}
    /*	index x is calculated, accumulate it when in range	*/
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
        {
        verbose(2, "truncating index %d\n", x);
        truncation = (x > truncation) ? x : truncation;
        totalTooBig += 1;
        }
    }

lineFileClose(&lf);

if (truncation > 0)
    {
    if (real)
	fprintf(stderr,"large values truncated: need %d bins or larger binSize than %g\n",truncation, binSizeR);
    else
	fprintf(stderr,"large values truncated: need %d bins or larger binSize than %d\n",truncation, binSize);
    printf("Maximum value %f\n", maxVal);
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

begin = minData;
end = maxData + 1;
if (verboseLevel()>1)
    {
    begin = 0;
    end = maxBinCount;
    }

if (probValues || freq)
    {
    totalCounts = 0;
    for (i=begin; i<end; ++i)
	totalCounts += hist[i];
    verbose(2,"#\ttotal data values: %llu\n", totalCounts);
    if (totalCounts < 1)
	errAbort("ERROR: No bins with any data ?\n");
    }

if (freq)
    maxCt = maxCt/(double)totalCounts;
if (doLog)
    maxCt = log(maxCt);

if (verboseLevel()>1)
    {
    if (noStar) {
	if (probValues)
	    printf("# bin\tValue\t\tprob-Value\t\tlog2(prob-Value)\tCPD\t1-CPD\n");
	else
	    printf("# bin  Value	ascii graph\n");
    } else
	printf("# bin  Value	ascii graph\n");
    }

cpd = 0.0;	/*	cumulative probability distribution	*/
/* Output results. */
for (i=begin; i<=end; ++i)
    {
    double ct;
    double binStartR = 0.0;
    int binStart = 0;
    long count;

    if (i != end)
	count = hist[i];
    else
	{
	if (totalTooBig == 0)
	    break;
        count = totalTooBig;
	}
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
    else if (freq)
        {
        ct = count/(double)totalCounts;
        }
    else
	{
	ct = count;
	}
    if (doLog)
	ct = log(ct);
    if (noStar)
	{
	if (i == end)
	    printf("<minVal or >=");
	if (verboseLevel()>1)
	    printf("%02d\t", i);
	if (real)
	    {
	    if (probValues)
		{
		if (verboseLevel()>1)
		    printf("%g:%g", binStartR, binStartR+binSizeR);
		else
		    printf("%3d %g:%g", i, binStartR, binStartR+binSizeR);
		}
	    else
		printf("%3d %g:%g\t%f", i, binStartR, binStartR+binSizeR, ct);
	    }
	else
	    {
	    printf("%d\t%f", binStart, ct);
	    }
	if (probValues)
	    {
	    if (ct > 0)
		{
		cpd += (double)ct/(double)totalCounts;
		printf("\t%f\t%f\t%f\t%f\n", (double)ct/(double)totalCounts,
		    log((double)ct/(double)totalCounts)/log(2.0), cpd, 1.0-cpd);
		}
	    else
		printf("\t0.0      \tN/A     \t%f\t%f\n", cpd, 1.0-cpd);
	    }
	else
	    printf("\n");
	}
    else
	{
	int astCount = round(ct * 60.0 / maxCt);
	if (i == end)
	    printf("<minVal or >=");
	if (verboseLevel()>1)
	    printf("%2d ", i);
	if (real)
	    printf("%f ", binStartR);
	else
	    printf("%3d ", binStart);
	for (j=0; j<astCount; ++j)
	    putchar('*');
	if ((aveCol >= 0) || freq)
	    printf(" %f\n", ct);
	else
	    printf(" %ld\n", count);
	}
    }
}	/*	textHistogram()	*/

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
probValues = optionExists("probValues");
col = optionInt("col", 1) - 1;
aveCol = optionInt("aveCol", 0) - 1;
real = optionExists("real");
autoscale = optionInt("autoscale", 0);
if (autoscale == 0)
    autoscale = optionInt("autoScale", 0);
freq = optionExists("freq") && !probValues;  /* freq and probValues don't currently work right together */
skip = optionInt("skip", 0);

/*	probValues turns on noStar too	*/
if (probValues) noStar = TRUE;

if (real)
    {
    char *valEnd;
    char *val = binSizeStr;
    binSizeR = strtod(val, &valEnd);
    if ((*val == '\0') || (*valEnd != '\0'))
	errAbort("Not a valid float for -binSize=%s\n", binSizeStr);
    if (binSizeR <= 0.0)
	errAbort("invalid binSize, must be greater than zero: %g\n", binSizeR);
    val = minValStr;
    minValR = strtod(val, &valEnd);
    if ((*val == '\0') || (*valEnd != '\0'))
	errAbort("Not a valid float for -minVal=%s\n", binSizeStr);
    }
else
    {
    binSize = atoi(binSizeStr);
    if (binSize < 1)
	errAbort("invalid binSize, must be >= one: %d\n", binSize);
    minVal = atoi(minValStr);
    }

verbose(2,"#\tbinSize: ");
if (real) verbose(2, "%f\n", binSizeR);
    else verbose(2, "%d\n", binSize);
verbose(2,"#\tmaxBinCount: %d\n", maxBinCount);
verbose(2,"#\tminVal: ");
if (real) verbose(2, "%f\n", minValR);
    else verbose(2, "%d\n", minVal);
verbose(2,"#\tlog function: %s\n", doLog ? "ON" : "OFF" );
verbose(2,"#\tdraw asterisks: %s\n", noStar ? "NO" : "YES" );
verbose(2,"#\thistogram on data input column: %d\n", col+1);
if (aveCol >= 0)
    verbose(2, "#\taveCol: %d\n", aveCol);
else
    verbose(2, "#\taveCol: not selected\n");
verbose(2,"#\treal valued data: %s\n", real ? "YES" : "NO" );
if (autoscale > 0)
    verbose(2, "#\tautoscaling to %d bins\n", autoscale);
else
    verbose(2, "#\tautoscale: not selected\n");
verbose(2, "#\tshow prob-Values: %s\n", probValues ? "YES" : "NO" );

/*	to autoscale stdin we would need to keep all the data read in
 *	during the min,max scan and reuse that data for the histogram
 *	calculation.  Not implemented yet.
 */
if (autoscale > 0)
    {
    if (startsWith("stdin", argv[1]))
	{
	errAbort("Sorry, can not autoscale stdin at this time.  Outstanding feature request.");
	}
    autoScale(argv[1]);
    }

textHistogram(argv[1]);
return 0;
}
