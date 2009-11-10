/* bigBedInfo - Show information about a bigBed file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "udc.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "hmmstats.h"

static char const rcsid[] = "$Id: bigBedInfo.c,v 1.1 2009/11/05 19:57:31 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigBedInfo - Show information about a bigBed file.\n"
  "usage:\n"
  "   bigBedInfo file.bb\n"
  "options:\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  "   -chroms - list all chromosomes and their sizes\n"
  "   -as - get autoSql spec\n"
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {"chroms", OPTION_BOOLEAN},
   {"as", OPTION_BOOLEAN},
   {NULL, 0},
};

void bigBedInfo(char *fileName)
/* bigBedInfo - Show information about a bigBed file.. */
{
struct bbiFile *bbi = bigBedFileOpen(fileName);
printf("version: %d\n", bbi->version);
printf("isSwapped: %d\n", bbi->isSwapped);
printf("itemCount: %lld\n", bigBedItemCount(bbi));
printf("zoomLevels: %d\n", bbi->zoomLevels);
struct bbiZoomLevel *zoom;
for (zoom = bbi->levelList; zoom != NULL; zoom = zoom->next)
    printf("\t%d\n", zoom->reductionLevel);
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
printf("chromCount: %d\n", slCount(chromList));
if (optionExists("chroms"))
    for (chrom=chromList; chrom != NULL; chrom = chrom->next)
	printf("\t%s %d %d\n", chrom->name, chrom->id, chrom->size);
if (optionExists("as"))
    {
    char *asText = bigBedAutoSqlText(bbi);
    if (asText == NULL)
        printf("as:  n/a\n");
    else
	{
	printf("as:\n");
	printf("%s", asText);
	}
    }
struct bbiSummaryElement sum = bbiTotalSummary(bbi);
printf("basesCovered: %lld\n", sum.validCount);
printf("meanDepth (of bases covered): %f\n", sum.sumData/sum.validCount);
printf("minDepth: %f\n", sum.minVal);
printf("maxDepth: %f\n", sum.maxVal);
printf("std of depth: %f\n", calcStdFromSums(sum.sumData, sum.sumSquares, sum.validCount));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
bigBedInfo(argv[1]);
return 0;
}
