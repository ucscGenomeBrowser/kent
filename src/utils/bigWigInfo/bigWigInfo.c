/* bigWigInfo - Print out information about bigWig file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "udc.h"
#include "bigWig.h"
#include "obscure.h"
#include "hmmstats.h"


static char const rcsid[] = "$Id: bigWigInfo.c,v 1.8 2010/04/28 22:56:25 braney Exp $";

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
  "   -zooms - list all zoom levels and their sizes\n"
  "   -minMax - list the min and max on a single line\n"
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {"chroms", OPTION_BOOLEAN},
   {"zooms", OPTION_BOOLEAN},
   {"minMax", OPTION_BOOLEAN},
   {NULL, 0},
};

void printLabelAndLongNumber(char *label, long long l)
/* Print label: 1,234,567 format number */
{
printf("%s: ", label);
printLongWithCommas(stdout, l);
printf("\n");
}

void bigWigInfo(char *fileName)
/* bigWigInfo - Print out information about bigWig file.. */
{
struct bbiFile *bwf = bigWigFileOpen(fileName);

if (optionExists("minMax"))
    {
    struct bbiSummaryElement sum = bbiTotalSummary(bwf);
    printf("%f %f\n", sum.minVal, sum.maxVal);
    return;
    }

printf("version: %d\n", bwf->version);
printf("isCompressed: %s\n", (bwf->uncompressBufSize > 0 ? "yes" : "no"));
printf("isSwapped: %d\n", bwf->isSwapped);
printLabelAndLongNumber("primaryDataSize", bwf->unzoomedIndexOffset - bwf->unzoomedDataOffset);
if (bwf->levelList != NULL)
    {
    long long indexEnd = bwf->levelList->dataOffset;
    printLabelAndLongNumber("primaryIndexSize", indexEnd - bwf->unzoomedIndexOffset);
    }
printf("zoomLevels: %d\n", bwf->zoomLevels);
if (optionExists("zooms"))
    {
    struct bbiZoomLevel *zoom;
    for (zoom = bwf->levelList; zoom != NULL; zoom = zoom->next)
	printf("\t%d\t%d\n", zoom->reductionLevel, (int)(zoom->indexOffset - zoom->dataOffset));
    }
struct bbiChromInfo *chrom, *chromList = bbiChromList(bwf);
printf("chromCount: %d\n", slCount(chromList));
if (optionExists("chroms"))
    for (chrom=chromList; chrom != NULL; chrom = chrom->next)
	printf("\t%s %d %d\n", chrom->name, chrom->id, chrom->size);
struct bbiSummaryElement sum = bbiTotalSummary(bwf);
printLabelAndLongNumber("basesCovered", sum.validCount);
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
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
