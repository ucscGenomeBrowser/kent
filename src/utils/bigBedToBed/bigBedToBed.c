/* bigBedToBed - Convert from bigBed to ascii bed format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "udc.h"
#include "bigBed.h"
#include "obscure.h"

static char const rcsid[] = "$Id: bigBedToBed.c,v 1.7 2009/09/08 19:50:24 kent Exp $";

char *clChrom = NULL;
int clStart = -1;
int clEnd = -1;
int maxItems = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigBedToBed - Convert from bigBed to ascii bed format.\n"
  "usage:\n"
  "   bigBedToBed input.bb output.bed\n"
  "options:\n"
  "   -chrom=chr1 - if set restrict output to given chromosome\n"
  "   -start=N - if set, restrict output to only that over start\n"
  "   -end=N - if set, restict output to only that under end\n"
  "   -maxItems=N - if set, restrict output to first N items\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {"maxItems", OPTION_INT},
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

void bigBedToBed(char *inFile, char *outFile)
/* bigBedToBed - Convert from bigBed to ascii bed format.. */
{
struct bbiFile *bbi = bigBedFileOpen(inFile);
FILE *f = mustOpen(outFile, "w");
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
int itemCount = 0;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (clChrom != NULL && !sameString(clChrom, chrom->name))
        continue;
    char *chromName = chrom->name;
    int start = 0, end = chrom->size;
    if (clStart > 0)
        start = clStart;
    if (clEnd > 0)
        end = clEnd;
    int itemsLeft = 0;	// Zero actually means no limit.... 
    if (maxItems != 0)
        {
	itemsLeft = maxItems - itemCount;
	if (itemsLeft <= 0)
	    break;
	}
    struct lm *lm = lmInit(0);
    struct bigBedInterval *interval, *intervalList = bigBedIntervalQuery(bbi, chromName, 
    	start, end, itemsLeft, lm);
    for (interval = intervalList; interval != NULL; interval = interval->next)
	{
	fprintf(f, "%s\t%u\t%u", chromName, interval->start, interval->end);
	char *rest = interval->rest;
	if (rest != NULL)
	    fprintf(f, "\t%s\n", rest);
	else
	    fprintf(f, "\n");
	}
    lmCleanup(&lm);
    }
bbiChromInfoFreeList(&chromList);
carefulClose(&f);
bbiFileClose(&bbi);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clChrom = optionVal("chrom", clChrom);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
maxItems = optionInt("maxItems", maxItems);
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
if (argc != 3)
    usage();
bigBedToBed(argv[1], argv[2]);
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
