/* bigWigToWig - Convert bigWig to wig.  This will keep more of the same structure of the 
 * original wig than bigWigToBedGraph does, but still will break up large stepped sections into 
 * smaller ones. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "udc.h"
#include "bigWig.h"
#include "obscure.h"

static char const rcsid[] = "$Id: bigWigToWig.c,v 1.1 2009/11/13 00:26:28 kent Exp $";

char *clChrom = NULL;
int clStart = -1;
int clEnd = -1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigToWig - Convert bigWig to wig.  This will keep more of the same structure of the\n"
  "original wig than bigWigToBedGraph does, but still will break up large stepped sections\n"
  "into smaller ones.\n"
  "usage:\n"
  "   bigWigToWig in.bigWig out.wig\n"
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

void bigWigToWig(char *inFile, char *outFile)
/* bigWigToWig - Convert bigWig to wig.  This will keep more of the same structure of the 
 * original wig than bigWigToBedGraph does, but still will break up large stepped sections into 
 * smaller ones. */
{
struct bbiFile *bwf = bigWigFileOpen(inFile);
FILE *f = mustOpen(outFile, "w");
struct bbiChromInfo *chrom, *chromList = bbiChromList(bwf);
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
    bigWigIntervalDump(bwf, chromName, start, end, 0, f);
    }
bbiChromInfoFreeList(&chromList);
carefulClose(&f);
bbiFileClose(&bwf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clChrom = optionVal("chrom", clChrom);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
bigWigToWig(argv[1], argv[2]);
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
