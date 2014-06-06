/* wigToBed - Convert wig to bed format using a threshold.  A primitive peak caller.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "metaWig.h"
#include "sqlNum.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "wigToBed - Convert wig (or bigWig) to bed format using a threshold.  A primitive peak caller.\n"
  "usage:\n"
  "   wigToBed input.wig threshold output.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void outputIntervalsOverThreshold(char *chrom, 
	struct bbiInterval *list, double threshold, FILE *f)
/* Go through list and write intervals over threshold to file in bed format.
 * Merge adjacent intervals if they are both over threshold. */
{
struct bbiInterval *overStart = NULL, *overEnd = NULL;
struct bbiInterval *el = list;
double basesCovered = 0;
double sum = 0;

for (;;)
    {
    if (el == NULL || el->val < threshold)
        {
	if (overStart != NULL)
	    {
	    fprintf(f, "%s\t%d\t%d\t%g\n", chrom, overStart->start, overEnd->end, sum/basesCovered);
	    overStart = overEnd = NULL;
	    basesCovered = 0;
	    sum = 0;
	    }
	if (el == NULL)
	    break;
	}
    else
        {
	if (overStart == NULL)
	    overStart = el;
	overEnd = el;
	double elSize = el->end - el->start;
	basesCovered += elSize;
	sum += elSize * el->val;
	}
    el = el->next;
    }
}

void wigToBed(char *inWig, char *thresholdString, char *outBed)
/* wigToBed - Convert wig to bed format using a threshold.  A primitive peak caller.. */
{
double threshold = sqlDouble(thresholdString);
uglyf("converting %s to %s at threshold %g\n", inWig, outBed, threshold);
struct metaWig *mw = metaWigOpen(inWig);
FILE *f = mustOpen(outBed, "w");

struct slName *chrom, *chromList = metaWigChromList(mw);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    verbose(2, "Processing %s\n", chrom->name);
    struct lm *lm = lmInit(0);
    struct bbiInterval *intervalList = metaIntervalsForChrom(mw, chrom->name, lm);
    outputIntervalsOverThreshold(chrom->name, intervalList, threshold, f);
    lmCleanup(&lm);
    }

metaWigClose(&mw);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
wigToBed(argv[1], argv[2], argv[3]);
return 0;
}
