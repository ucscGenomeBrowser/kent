/* testBb - Test out some bigBed stuff.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "sig.h"
#include "sqlNum.h"
#include "bPlusTree.h"
#include "cirTree.h"
#include "rbTree.h"
#include "rangeTree.h"
#include "obscure.h"
#include "dystring.h"
#include "bwgInternal.h"
#include "bigWig.h"
#include "bigBed.h"



int blockSize = 1024;
int itemsPerSlot = 64;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testBb - Test out some bigBed stuff.\n"
  "usage:\n"
  "   testBb in.bed chrom.sizes out.bb chrom start end\n"
  "options:\n"
  "   -blockSize=N - Number of items to bundle in r-tree.  Default %d\n"
  "   -itemsPerSlot=N - Number of data points bundled at lowest level. Default %d\n"
  , blockSize, itemsPerSlot
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void testQuery(char *bbName, char *chrom, int start, int end)
/* Print matching queries. */
{
struct bigWigFile *bb = bigBedFileOpen(bbName);
struct lm *lm = lmInit(0);
struct bigBedInterval *bi, *biList = bigBedIntervalQuery(bb, chrom, start, end, lm);
for (bi = biList; bi != NULL; bi = bi->next)
    {
    uglyf("%s:%d-%d %s\n", chrom, bi->start, bi->end, bi->rest);
    }
lmCleanup(&lm);
bigBedFileClose(&bb);

double sum[10];
int i;
for (i=0; i<ArraySize(sum); ++i)
    sum[i] = nan("");
if (bigBedSummaryArray(bbName, chrom, start, end, bigWigSumMean, 10, sum))
    {
    for (i=0; i<ArraySize(sum); ++i)
	uglyf("%g ", sum[i]);
    uglyf("\n");
    }

}

void testBb(char *inBed, char *chromSizes, char *outBb, char *queryChrom, int queryStart, int queryEnd)
/* testBb - Test out some bigBed stuff.. */
{
bigBedFileCreate(inBed, chromSizes, blockSize, itemsPerSlot, outBb);
testQuery(outBb, queryChrom, queryStart, queryEnd);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
if (argc != 7)
    usage();
testBb(argv[1], argv[2], argv[3], argv[4], sqlUnsigned(argv[5]), sqlUnsigned(argv[6]));
return 0;
}
