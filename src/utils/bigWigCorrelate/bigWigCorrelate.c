/* bigWigCorrelate - Correlate bigWig files, optionally only on target regions.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include <float.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "correlate.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "bigWig.h"
#include "genomeRangeTree.h"

char *restrictFile = NULL;
double threshold = FLT_MAX;
boolean rootNames = FALSE;
boolean ignoreMissing = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigCorrelate - Correlate bigWig files, optionally only on target regions.\n"
  "usage:\n"
  "   bigWigCorrelate a.bigWig b.bigWig\n"
  "or\n"
  "   bigWigCorrelate listOfFiles\n"
  "options:\n"
  "   -restrict=restrict.bigBed - restrict correlation to parts covered by this file\n"
  "   -threshold=N.N - clip values to this threshold\n"
  "   -rootNames - if set just report the root (minus directory and suffix) of file\n"
  "                names when using listOfFiles\n"
  "   -ignoreMissing - if set do not correlate where either side is missing data\n"
  "                Normally missing data is treated as zeros\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"restrict", OPTION_STRING},
   {"threshold", OPTION_DOUBLE},
   {"rootNames", OPTION_BOOLEAN},
   {"ignoreMissing", OPTION_BOOLEAN},
   {NULL, 0},
};

static void addBwCorrelations(struct bbiChromInfo *chrom, struct genomeRangeTree *targetGrt,
    struct bigWigValsOnChrom *aVals, struct bigWigValsOnChrom *bVals,
    struct bbiFile *aBbi, struct bbiFile *bBbi, 
    double aThreshold, double bThreshold, struct correlate *c)
/* Find bits of a and b that overlap and also overlap with targetRanges.  Do correlations there */
{
struct rbTree *targetRanges = NULL;
boolean useMissing = !ignoreMissing;
if (targetGrt != NULL)
    targetRanges = genomeRangeTreeFindRangeTree(targetGrt, chrom->name);
if (bigWigValsOnChromFetchData(aVals, chrom->name, aBbi) &&
    bigWigValsOnChromFetchData(bVals, chrom->name, bBbi) )
    {
    double *a = aVals->valBuf, *b = bVals->valBuf;
    Bits *aCov = aVals->covBuf, *bCov = bVals->covBuf;
    if (targetRanges == NULL)
	{
	int i, end = chrom->size;
	for (i=0; i<end; ++i)
	    {
	    if (useMissing || (bitReadOne(aCov,i) && bitReadOne(bCov,i)))
		{
		double aVal = a[i], bVal = b[i];
		if (aVal > aThreshold) aVal = aThreshold;
		if (bVal > bThreshold) bVal = bThreshold;
		correlateNext(c, aVal, bVal);
		}
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
		if (useMissing || (bitReadOne(aCov,i) && bitReadOne(bCov,i)))
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

struct correlate *bigWigCorrelate(char *aFileName, char *bFileName)
/* bigWigCorrelate - Correlate bigWig files, optionally only on target regions.. */
{
struct genomeRangeTree *targetGrt = NULL;
if (restrictFile)
    targetGrt = grtFromBigBed(restrictFile);
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
bigWigValsOnChromFree(&aVals);
bigWigValsOnChromFree(&bVals);
bbiFileClose(&aBbi);
bbiFileClose(&bBbi);
genomeRangeTreeFree(&targetGrt);
return c;
}

void bigWigCorrelatePair(char *aFileName, char *bFileName)
/* Correlate a pair of bigWigs and print result */
{
struct correlate *c = bigWigCorrelate(aFileName, bFileName);
printf("%g\n", correlateResult(c));
}

void bigWigCorrelateList(char *listFile)
/* Correlate all files in list to each other */
{
char **fileNames = NULL;
int fileCount = 0;
char *buf = NULL;
readAllWords(listFile, &fileNames, &fileCount, &buf);
int i;
for (i=0; i<fileCount; ++i)
    {
    char *aPath = fileNames[i];
    char aName[FILENAME_LEN];
    if (rootNames)
	splitPath(aPath, NULL, aName, NULL);
    else
        safef(aName, sizeof(aName), "%s", aPath);
    int j;
    for (j=i+1; j<fileCount; ++j)
        {
	char *bPath = fileNames[j];
	char bName[FILENAME_LEN];
	if (rootNames)
	    splitPath(bPath, NULL, bName, NULL);
	else
	    safef(bName, sizeof(bName), "%s", bPath);
	struct correlate *c = bigWigCorrelate(aPath, bPath);
	printf("%s\t%s\t%g\n", aName, bName, correlateResult(c));
	correlateFree(&c);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2 && argc != 3)
    usage();
restrictFile = optionVal("restrict", restrictFile);
threshold = optionDouble("threshold", threshold);
rootNames = optionExists("rootNames");
ignoreMissing = optionExists("ignoreMissing");
if (argc == 3)
    bigWigCorrelatePair(argv[1], argv[2]);
else
    bigWigCorrelateList(argv[1]);
return 0;
}
