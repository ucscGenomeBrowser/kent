/* testTwoBitRangeCache - Test a caching capability for sequences from two bit files.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "twoBit.h"
#include "rangeTree.h"
#include "cacheTwoBit.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testTwoBitRangeCache - Test a caching capability for sequences from two bit files.\n"
  "usage:\n"
  "   testTwoBitRangeCache input.tab\n"
  "Where input.tab is a tab-separated-file with columns:\n"
  "   file.2bit	seqName	strand	startBase   endBase\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void testTwoBitRangeCache(char *input)
/* testTwoBitRangeCache - Test a caching capability for sequences from two bit files.. */
{
struct cacheTwoBitRanges *cache = cacheTwoBitRangesNew();
/* Create a new cache for two bits */
struct lineFile *lf = lineFileOpen(input, TRUE);
char *row[5];
int rowCount = 0;
long totalBases = 0;
while (lineFileRow(lf, row))
    {
    char *fileName = row[0];
    char *seqName = row[1];
    char *strand = row[2];
    int startBase = lineFileNeedNum(lf, row, 3);
    int endBase = lineFileNeedNum(lf, row, 4);
    int baseSize = endBase - startBase;
    boolean isRc = (strand[0] == '-');
    int seqOffset=0;
    struct dnaSeq *seq = cacheTwoBitRangesFetch(cache, fileName, 
	seqName, startBase, endBase, isRc, &seqOffset);
    if (seq == NULL)
        warn("Missing seq %s:%s\n", fileName, seqName);
    totalBases += baseSize;
    ++rowCount;
    }
printf("%ld total bases in %d rows, %d 2bit files\n", totalBases, rowCount, cache->urlHash->elCount);
cacheTwoBitRangesPrintStats(cache);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
testTwoBitRangeCache(argv[1]);
return 0;
}
