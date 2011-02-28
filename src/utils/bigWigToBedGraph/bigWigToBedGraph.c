/* bigWigToBedGraph - Convert from bigWig to bedGraph format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "udc.h"
#include "bigWig.h"
#include "obscure.h"

static char const rcsid[] = "$Id: bigWigToBedGraph.c,v 1.5 2009/09/08 19:50:25 kent Exp $";

char *clChrom = NULL;
int clStart = -1;
int clEnd = -1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigToBedGraph - Convert from bigWig to bedGraph format.\n"
  "usage:\n"
  "   bigWigToBedGraph in.bigWig out.bedGraph\n"
  "options:\n"
  "   -chrom=chr1 - if set restrict output to given chromosome\n"
  "   -start=N - if set, restrict output to only that over start\n"
  "   -end=N - if set, restict output to only that under end\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

void bigWigToBedGraph(char *inFile, char *outFile)
/* bigWigToBedGraph - Convert from bigWig to bedGraph format.. */
{
struct bbiFile *bwf = bigWigFileOpen(inFile);
FILE *f = mustOpen(outFile, "w");
struct bbiChromInfo *chrom, *chromList = bbiChromList(bwf);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    boolean firstTime = TRUE;
    int saveStart = -1, prevEnd = -1;
    double saveVal = -1.0;

    if (clChrom != NULL && !sameString(clChrom, chrom->name))
        continue;
    char *chromName = chrom->name;
    struct lm *lm = lmInit(0);
    int start = 0, end = chrom->size;
    if (clStart > 0)
        start = clStart;
    if (clEnd > 0)
        end = clEnd;
    struct bbiInterval *interval, *intervalList = bigWigIntervalQuery(bwf, chromName, 
    	start, end, lm);
    for (interval = intervalList; interval != NULL; interval = interval->next)
	{
	if (firstTime)
	    {
	    saveStart = interval->start;
	    saveVal = interval->val;
	    firstTime = FALSE;
	    }
	else
	    {
	    if (!((prevEnd == interval->start) && (saveVal == interval->val)))
		{
		fprintf(f, "%s\t%u\t%u\t%g\n", chromName, saveStart, prevEnd, saveVal);
		saveStart = interval->start;
		saveVal = interval->val;
		}

	    }
	prevEnd = interval->end;
	}
    if (!firstTime)
	fprintf(f, "%s\t%u\t%u\t%g\n", chromName, saveStart, prevEnd, saveVal);

    lmCleanup(&lm);
    }
bbiChromInfoFreeList(&chromList);
carefulClose(&f);
bbiFileClose(&bwf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clChrom = optionVal("chrom", clChrom);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
if (argc != 3)
    usage();
bigWigToBedGraph(argv[1], argv[2]);
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
