/* bigWigInfo - Print out information about bigWig file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "udc.h"
#include "bigWig.h"
#include "hmmstats.h"


static char const rcsid[] = "$Id: bigWigInfo.c,v 1.4 2009/11/07 19:30:32 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigInfo - Print out information about bigWig file.\n"
  "usage:\n"
  "   bigWigInfo file.bw\n"
  "options:\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  "   -chroms - list all chromosomes and their sizes\n"
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {"chroms", OPTION_BOOLEAN},
   {NULL, 0},
};

void bigWigInfo(char *fileName)
/* bigWigInfo - Print out information about bigWig file.. */
{
struct bbiFile *bwf = bigWigFileOpen(fileName);
printf("version: %d\n", bwf->version);
printf("isSwapped: %d\n", bwf->isSwapped);
printf("zoomLevels: %d\n", bwf->zoomLevels);
printf("primaryDataSize: %lld\n", (long long)(bwf->unzoomedIndexOffset -  bwf->unzoomedDataOffset));
if (bwf->levelList != NULL)
    {
    long long indexEnd = bwf->levelList->dataOffset;
    printf("primaryIndexSize: %lld\n", indexEnd - bwf->unzoomedIndexOffset);
    }
struct bbiZoomLevel *zoom;
for (zoom = bwf->levelList; zoom != NULL; zoom = zoom->next)
    printf("\t%d\n", zoom->reductionLevel);
struct bbiChromInfo *chrom, *chromList = bbiChromList(bwf);
printf("chromCount: %d\n", slCount(chromList));
if (optionExists("chroms"))
    for (chrom=chromList; chrom != NULL; chrom = chrom->next)
	printf("\t%s %d %d\n", chrom->name, chrom->id, chrom->size);
struct bbiSummaryElement sum = bbiTotalSummary(bwf);
printf("basesCovered: %lld\n", sum.validCount);
printf("mean: %f\n", sum.sumData/sum.validCount);
printf("min: %f\n", sum.minVal);
printf("max: %f\n", sum.maxVal);
printf("std: %f\n", calcStdFromSums(sum.sumData, sum.sumSquares, sum.validCount));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
if (argc != 2)
    usage();
bigWigInfo(argv[1]);
return 0;
}
