/* edwComparePeaks - Compare  two peak files and print some stats. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genomeRangeTree.h"
#include "bigBed.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwComparePeaks - Compare  two peak files and print some stats\n"
  "usage:\n"
  "   edwComparePeaks aPeaks bPeaks out.ra\n"
  "where aPeaks and bPeaks are in a bigBed peaky format\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct genomeRangeTree *grtFromPeaks(char *peakFileName)
/* Convert big bed peaks to genome range tree in memory */
{
struct genomeRangeTree *grt = genomeRangeTreeNew();
struct bbiFile *bbi = bigBedFileOpen(peakFileName);
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct rbTree *rangeTree = genomeRangeTreeFindOrAddRangeTree(grt, chrom->name);
    struct lm *lm = lmInit(0);
    struct bigBedInterval *el, *list = bigBedIntervalQuery(bbi,chrom->name,0,chrom->size,0,lm);
    for (el = list; el != NULL; el = el->next)
	{
	rangeTreeAdd(rangeTree, el->start, el->end);
	}
    lmCleanup(&lm);
    }
bbiChromInfoFreeList(&chromList);
bigBedFileClose(&bbi);
return grt;
}

struct rtSumIntersectContext
/* Some context for the rtSumIntersect function */
    {
    struct rbTree *other;
    long long sumIntersect;
    };

static void rtSumIntersectCallback(void *item, void *context)
/* This is a callback for rbTreeTraverse with context.  It just adds up
 * end-start */
{
struct range *range = item;
struct rtSumIntersectContext *c = context;
c->sumIntersect += rangeTreeOverlapSize(c->other,  range->start, range->end);
}

long long genomeRangeTreeIntersectSize(struct genomeRangeTree *aGrt, struct genomeRangeTree *bGrt)
/* Return size of intersection between  and b */
{
struct rtSumIntersectContext context = {.sumIntersect=0};
struct hashEl *chrom, *chromList = hashElListHash(bGrt->hash);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct rbTree *bTree = chrom->val;
    struct rbTree *aTree = hashFindVal(aGrt->hash, chrom->name);
    if (aTree != NULL)
        {
	context.other = aTree;
	rbTreeTraverseWithContext(bTree, rtSumIntersectCallback, &context);
	}
    }
return context.sumIntersect;
}

void edwComparePeaks(char *aFileName, char *bFileName, char *outFileName)
/* edwComparePeaks - Compare  two peak files and print some stats. */
{
FILE *f = mustOpen(outFileName, "w");
struct genomeRangeTree *aGrt = grtFromPeaks(aFileName);
fprintf(f, "aFileName %s\n", aFileName);
long long aBaseCount = genomeRangeTreeSumRanges(aGrt);
fprintf(f, "aBaseCount %lld\n", aBaseCount);
struct genomeRangeTree *bGrt = grtFromPeaks(bFileName);
fprintf(f, "bFileName %s\n", bFileName);
long long bBaseCount = genomeRangeTreeSumRanges(bGrt);
fprintf(f, "bBaseCount %lld\n", bBaseCount);
long long intersectionSize = genomeRangeTreeIntersectSize(aGrt, bGrt);
fprintf(f, "intersectionSize %lld\n", intersectionSize);
long long unionSize = aBaseCount + bBaseCount - intersectionSize;
fprintf(f, "unionSize = %lld\n", unionSize);
double iuRatio = 0;
if (unionSize > 0)
     iuRatio = (double)intersectionSize/(double)unionSize;
fprintf(f, "iuRatio %g\n", iuRatio);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
edwComparePeaks(argv[1], argv[2], argv[3]);
return 0;
}
