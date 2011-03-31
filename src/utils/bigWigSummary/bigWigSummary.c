/* bigWigSummary - Extract summary information from a bigWig file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "udc.h"
#include "bigWig.h"
#include "obscure.h"

static char const rcsid[] = "$Id: bigWigSummary.c,v 1.13 2009/11/20 17:12:17 kent Exp $";

char *summaryType = "mean";


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigSummary - Extract summary information from a bigWig file.\n"
  "usage:\n"
  "   bigWigSummary file.bigWig chrom start end dataPoints\n"
  "Get summary data from bigWig for indicated region, broken into\n"
  "dataPoints equal parts.  (Use dataPoints=1 for simple summary.)\n"
  "\nNOTE:  start and end coordinates are in BED format (0-based)\n\n"
  "options:\n"
  "   -type=X where X is one of:\n"
  "         mean - average value in region (default)\n"
  "         min - minimum value in region\n"
  "         max - maximum value in region\n"
  "         std - standard deviation in region\n"
  "         coverage - %% of region that is covered\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  );
}

static struct optionSpec options[] = {
   {"type", OPTION_STRING},
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

void bigWigSummary(char *bigWigFile, char *chrom, int start, int end, int dataPoints)
/* bigWigSummary - Extract summary information from a bigWig file.. */
{
struct bbiFile *bwf = bigWigFileOpen(bigWigFile);

/* Make up values array initialized to not-a-number. */
double nan0 = strtod("NaN", NULL);
double summaryValues[dataPoints];
int i;
for (i=0; i<dataPoints; ++i)
    summaryValues[i] = nan0;

if (bigWigSummaryArray(bwf, chrom, start, end, bbiSummaryTypeFromString(summaryType), 
      dataPoints, summaryValues))
    {
    for (i=0; i<dataPoints; ++i)
	{
	double val = summaryValues[i];
	if (i != 0)
	    printf("\t");
	if (isnan(val))
	    printf("n/a");
	else
	    printf("%g", val);
	}
    printf("\n");
    }
else
    {
    errAbort("no data in region %s:%d-%d in %s\n", chrom, start, end, bigWigFile);
    }
bigWigFileClose(&bwf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
summaryType = optionVal("type", summaryType);
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
bigWigSummary(argv[1], argv[2], sqlUnsigned(argv[3]), sqlUnsigned(argv[4]), sqlUnsigned(argv[5]));
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
