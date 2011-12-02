/* ave - Compute average and basic stats. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "hmmstats.h"
#include <float.h>


static int col = 1;
static bool tableOut = FALSE;
static bool noQuartiles = FALSE;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ave - Compute average and basic stats\n"
  "usage:\n"
  "   ave file\n"
  "options:\n"
  "   -col=N Which column to use.  Default 1\n"
  "   -tableOut - output by columns (default output in rows)\n"
  "   -noQuartiles - only calculate min,max,mean,standard deviation\n"
  "                - for large data sets that will not fit in memory."
  );
}

int cmpDouble(const void *va, const void *vb)
/* Compare two slNames. */
{
const double *a = va;
const double *b = vb;
double diff = *a - *b;
if (diff < 0)
   return -1;
else if (diff > 0)
   return 1;
else
   return 0;
}

void showStats(double *array, int count)
/* Compute stats on sorted array */
{
double val, minVal = DBL_MAX, maxVal = -DBL_MAX;
double total = 0, average;
int i;
int q1Index, q3Index;		/*	quartile positions	*/
double q1, q3;			/*	quartile values	*/
double oneVar, totalVar = 0;

for (i=0; i<count; ++i)
    {
    val = array[i];
    if (minVal > val) minVal = val;
    if (maxVal < val) maxVal = val;
    total += val;
    }
average = total/count;

q1Index = (count+1)/4;		/*	one fourth, rounded down	*/
q3Index = (3*(count+1))/4;	/*	three fourths, rounded down	*/
if (q1Index < (count-1))
    {
    double range = array[q1Index+1] - array[q1Index];
    q1 = array[q1Index] +
	((((double)count+1.0)/4.0)-(double)q1Index)*range;
    }
else
    q1 = array[q1Index];
if (q3Index < (count-1))
    {
    double range = array[q3Index+1] - array[q3Index];
    q3 = array[q3Index] +
	((3.0*((double)count+1.0)/4.0)-(double)q3Index)*range;
    }
else
    q3 = array[q3Index];

for (i=0; i<count; ++i)
    {
    val = array[i];
    oneVar = (average-val);
    totalVar += oneVar*oneVar;
    }

    double var = totalVar;
    if (count > 1)
	var /= count-1;
    double stdDev = sqrt(var);
if (tableOut)
    {
    printf("# min Q1 median Q3 max mean N sum stddev\n");
    printf("%g %g %g %g %g %g %d %g %g\n", minVal, q1, array[count/2],
	q3, maxVal, average, count, total, stdDev);
    }
else
    {
    printf("Q1 %f\n", q1);
    printf("median %f\n", array[count/2]);
    printf("Q3 %f\n", q3);
    printf("average %f\n", average);
    printf("min %f\n", minVal);
    printf("max %f\n", maxVal);
    printf("count %d\n", count);
    printf("total %f\n", total);
    printf("standard deviation %f\n", stdDev);
    }
}

void aveNoQuartiles(char *fileName)
/* aveNoQuartiles - Compute only min,max,mean,stdDev no quartiles */
{
bits64 count = 0;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[128], *word;
int wordCount;
int wordIx = col-1;
double sumData = 0.0, sumSquares = 0.0;
double minVal = DBL_MAX, maxVal = -DBL_MAX;

while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    word = words[wordIx];
    if (word[0] == '-' || isdigit(word[0]))
        {
	double val = sqlDouble(word);
	if (minVal > val) minVal = val;
	if (maxVal < val) maxVal = val;
	sumData += val;
	sumSquares += val * val;
	++count;
	}
    }
if (count == 0)
    errAbort("No numerical data column %d of %s", col, fileName);
double average = sumData/count;
double stdDev = calcStdFromSums(sumData, sumSquares, count);
if (tableOut)
    {
    printf("# min max mean N sum stddev\n");
    printf("%g %g %g %llu %g %g\n",
	minVal, maxVal, average, count, sumData, stdDev);
    }
else
    {
    printf("average %f\n", average);
    printf("min %f\n", minVal);
    printf("max %f\n", maxVal);
    printf("count %llu\n", count);
    printf("total %f\n", sumData);
    printf("standard deviation %f\n", stdDev);
    }
}

void ave(char *fileName)
/* ave - Compute average and basic stats. */
{
int count = 0;
size_t alloc = 1024;
double *array;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[128], *word;
int wordCount;
int wordIx = col-1;

AllocArray(array, alloc);
while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    if (count >= alloc)
        {
	alloc <<= 1;
	ExpandArray(array, count, alloc);
	}
    word = words[wordIx];
    if (word[0] == '-' || isdigit(word[0]))
        {
	array[count++] = atof(word);
	}
    }
if (count == 0)
    errAbort("No numerical data column %d of %s", col, fileName);
qsort(array, count, sizeof(array[0]), cmpDouble);
showStats(array, count);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
col = optionInt("col", col);
tableOut = optionExists("tableOut");
noQuartiles = optionExists("noQuartiles");
if (noQuartiles)
    aveNoQuartiles(argv[1]);
else
    ave(argv[1]);

return 0;
}
