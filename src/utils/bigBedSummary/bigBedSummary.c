/* bigBedSummary - Extract summary information from a bigBed file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "bigBed.h"

static char const rcsid[] = "$Id: bigBedSummary.c,v 1.2 2009/02/02 06:05:52 kent Exp $";

char *summaryType = "coverage";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigBedSummary - Extract summary information from a bigBed file.\n"
  "usage:\n"
  "   bigWigSummary file.bb chrom start end dataPoints\n"
  "Get summary data from bigBed for indicated region, broken into\n"
  "dataPoints equal parts.  (Use dataPoints=1 for simple summary.)\n"
  "options:\n"
  "   -type=X where X is one of:\n"
  "         coverage - %% of region that is covered (default)\n"
  "         mean - average depth of covered regions\n"
  "         min - minimum depth of covered regions\n"
  "         max - maximum depth of covered regions\n"
  );
}

static struct optionSpec options[] = {
   {"type", OPTION_STRING},
   {NULL, 0},
};

void bigBedSummary(char *fileName, char *chrom, int start, int end, int dataPoints)
/* bigBedSummary - Extract summary information from a bigBed file.. */
{
/* Make up values array initialized to not-a-number. */
double nan0 = nan(NULL);
double summaryValues[dataPoints];
int i;
for (i=0; i<dataPoints; ++i)
    summaryValues[i] = nan0;

if (bigBedSummaryArray(fileName, chrom, start, end, bbiSummaryTypeFromString(summaryType), 
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
    errAbort("no data in region %s:%d-%d in %s\n", chrom, start, end, fileName);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
summaryType = optionVal("type", summaryType);
if (argc != 6)
    usage();
bigBedSummary(argv[1], argv[2], sqlUnsigned(argv[3]), sqlUnsigned(argv[4]), sqlUnsigned(argv[5]));
return 0;
}
