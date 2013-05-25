/* bigWigCorrelate - Correlate bigWig files, optionally only on target regions.. */
#include <float.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "correlate.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "bigWig.h"
#include "genomeRangeTree.h"

char *restrict = NULL;
double threshold = FLT_MAX;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigCorrelate - Correlate bigWig files, optionally only on target regions.\n"
  "usage:\n"
  "   bigWigCorrelate a.bigWig b.bigWig\n"
  "options:\n"
  "   -restrict=restrict.bigBed - restrict correlation to parts covered by this file\n"
  "   -threshold=N.N - clip values to this threshold\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"restrict", OPTION_STRING},
   {"threshold", OPTION_DOUBLE},
   {NULL, 0},
};

static void addBwCorrelations(struct bbiChromInfo *chrom, struct genomeRangeTree *targetGrt,
    struct bigWigValsOnChrom *aVals, struct bigWigValsOnChrom *bVals,
    struct bbiFile *aBbi, struct bbiFile *bBbi, 
    double aThreshold, double bThreshold, struct correlate *c)
/* Find bits of a and b that overlap and also overlap with targetRanges.  Do correlations there */
{
struct rbTree *targetRanges = NULL;
if (targetGrt != NULL)
    targetRanges = genomeRangeTreeFindRangeTree(targetGrt, chrom->name);
if (bigWigValsOnChromFetchData(aVals, chrom->name, aBbi) &&
    bigWigValsOnChromFetchData(bVals, chrom->name, bBbi) )
    {
    double *a = aVals->valBuf, *b = bVals->valBuf;
    if (targetRanges == NULL)
	{
	int i, end = chrom->size;
	for (i=0; i<end; ++i)
	    {
	    double aVal = a[i], bVal = b[i];
	    if (aVal > aThreshold) aVal = aThreshold;
	    if (bVal > bThreshold) bVal = bThreshold;
	    correlateNext(c, aVal, bVal);
	    }
	}
    else
	{
	struct range *range, *rangeList = rangeTreeList(targetRanges);
	for (range = rangeList; range != NULL; range = range->next)
	    {
	    int start = range->start, end = range->end;
	    int i;
	    for (i=start; i<end; ++i)
		{
		double aVal = a[i], bVal = b[i];
		if (aVal > aThreshold) aVal = aThreshold;
		if (bVal > bThreshold) bVal = bThreshold;
		correlateNext(c, aVal, bVal);
		}
	    }
	}
    }
}


struct genomeRangeTree *grtFromBigBed(char *fileName)
/* Return genome range tree for simple (unblocked) bed */
{
struct bbiFile *bbi = bigBedFileOpen(fileName);
if (bbi->definedFieldCount >= 12)
    warn("Ignoring blocks of %s, just using chrom/chromStart/chromEnd", fileName);
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
struct genomeRangeTree *grt = genomeRangeTreeNew();
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct rbTree *tree = genomeRangeTreeFindOrAddRangeTree(grt, chrom->name);
    struct lm *lm = lmInit(0);
    struct bigBedInterval *iv, *ivList = NULL;
    ivList = bigBedIntervalQuery(bbi, chrom->name, 0, chrom->size, 0, lm);
    for (iv = ivList; iv != NULL; iv = iv->next)
        rangeTreeAdd(tree, iv->start, iv->end);
    lmCleanup(&lm);
    }
bigBedFileClose(&bbi);
bbiChromInfoFreeList(&chromList);
return grt;
}

void bigWigCorrelate(char *aFileName, char *bFileName)
/* bigWigCorrelate - Correlate bigWig files, optionally only on target regions.. */
{
struct genomeRangeTree *targetGrt = NULL;
if (restrict)
    targetGrt = grtFromBigBed(restrict);
struct bbiFile *aBbi = bigWigFileOpen(aFileName);
struct bbiFile *bBbi = bigWigFileOpen(bFileName);
struct correlate *c = correlateNew();
struct bbiChromInfo *chrom, *chromList = bbiChromList(aBbi);
struct bigWigValsOnChrom *aVals = bigWigValsOnChromNew();
struct bigWigValsOnChrom *bVals = bigWigValsOnChromNew();
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    addBwCorrelations(chrom, targetGrt, aVals, bVals, aBbi, bBbi, threshold, threshold, c);
    }
printf("%g\n", correlateResult(c));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
restrict = optionVal("restrict", restrict);
threshold = optionDouble("threshold", threshold);
bigWigCorrelate(argv[1], argv[2]);
return 0;
}
