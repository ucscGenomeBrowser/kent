/* ave - Compute average and basic stats. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

int col = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ave - Compute average and basic stats\n"
  "usage:\n"
  "   ave file\n"
  "options:\n"
  "   -col=N Which column to use.  Default 1\n"
  );
}

int cmpInt(const void *va, const void *vb)
/* Compare two slNames. */
{
const int *a = va;
const int *b = vb;
return *a - *b;
}

void showStats(int *array, int count)
/* Compute stats on sorted array */
{
int val, minVal = BIGNUM, maxVal = -BIGNUM;
double total = 0, average;
int i;
double oneVar, totalVar = 0;

for (i=0; i<count; ++i)
    {
    val = array[i];
    if (minVal > val) minVal = val;
    if (maxVal < val) maxVal = val;
    total += val;
    }
average = total/count;

printf("median %d\n", array[count/2]);
printf("average %f\n", average);
printf("min %d\n", minVal);
printf("max %d\n", maxVal);
printf("count %d\n", count);
printf("total %f\n", total);

for (i=0; i<count; ++i)
    {
    val = array[i];
    oneVar = (average-val);
    totalVar += oneVar*oneVar;
    }
printf("standard deviation %f\n", sqrt(totalVar/count));
}

void ave(char *fileName)
/* ave - Compute average and basic stats. */
{
int count = 0, alloc = 1024;
int *array;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[128];
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
    array[count++] = lineFileNeedNum(lf, words, wordIx);
    }
qsort(array, count, sizeof(array[0]), cmpInt);
showStats(array, count);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
col = optionInt("col", col);
ave(argv[1]);
return 0;
}
