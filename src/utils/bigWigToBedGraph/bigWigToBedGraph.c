/* bigWigToBedGraph - Convert from bigWig to bedGraph format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: bigWigToBedGraph.c,v 1.1 2009/01/28 14:42:08 kent Exp $";

char *clChrom = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigToBedGraph - Convert from bigWig to bedGraph format.\n"
  "usage:\n"
  "   bigWigToBedGraph in.bigWig out.bedGraph\n"
  "options:\n"
  "   -chrom=chr1 - if set restrict output to given chromosome\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {NULL, 0},
};

void bigWigToBedGraph(char *inFile, char *outFile)
/* bigWigToBedGraph - Convert from bigWig to bedGraph format.. */
{
struct bigWigFile *bwf = bigWigFileOpen(inFile);
FILE *f = mustOpen(outFile, "w");
struct bigWigChromInfo *chrom, *chromList = bigWigChromList(bwf);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (clChrom != NULL && !sameString(clChrom, chrom->name))
        continue;
    char *chromName = chrom->name;
    struct lm *lm = lmInit(0);
    struct bigWigInterval *interval, *intervalList = bigWigIntervalQuery(bwf, chromName, 
    	0, chrom->size, lm);
    for (interval = intervalList; interval != NULL; interval = interval->next)
        fprintf(f, "%s\t%u\t%u\t%g\n", chromName, interval->start, interval->end, interval->val);
    lmCleanup(&lm);
    }
bigWigChromInfoFreeList(&chromList);
carefulClose(&f);
// bigWigFileClose(&bwf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clChrom = optionVal("chrom", NULL);
if (argc != 3)
    usage();
bigWigToBedGraph(argv[1], argv[2]);
return 0;
}
