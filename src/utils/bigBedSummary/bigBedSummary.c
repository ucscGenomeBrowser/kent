/* bigBedSummary - Extract summary information from a bigBed file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "bigBed.h"
#include "asParse.h"
#include "udc.h"
#include "obscure.h"

static char const rcsid[] = "$Id: bigBedSummary.c,v 1.7 2009/09/08 19:50:24 kent Exp $";

char *summaryType = "coverage";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigBedSummary - Extract summary information from a bigBed file.\n"
  "usage:\n"
  "   bigBedSummary file.bb chrom start end dataPoints\n"
  "Get summary data from bigBed for indicated region, broken into\n"
  "dataPoints equal parts.  (Use dataPoints=1 for simple summary.)\n"
  "options:\n"
  "   -type=X where X is one of:\n"
  "         coverage - %% of region that is covered (default)\n"
  "         mean - average depth of covered regions\n"
  "         min - minimum depth of covered regions\n"
  "         max - maximum depth of covered regions\n"
  "   -fields - print out information on fields in file.\n"
  "      If fields option is used, the chrom, start, end, dataPoints\n"
  "      parameters may be omitted\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  );
}

static struct optionSpec options[] = {
   {"type", OPTION_STRING},
   {"fields", OPTION_BOOLEAN},
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

void bigBedSummary(char *fileName, char *chrom, int start, int end, int dataPoints)
/* bigBedSummary - Extract summary information from a bigBed file.. */
{
/* Make up values array initialized to not-a-number. */
double nan0 = strtod("NaN", NULL);
double summaryValues[dataPoints];
int i;
for (i=0; i<dataPoints; ++i)
    summaryValues[i] = nan0;

struct bbiFile *bbi = bigBedFileOpen(fileName);
if (bigBedSummaryArray(bbi, chrom, start, end, bbiSummaryTypeFromString(summaryType), 
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
bbiFileClose(&bbi);
}


void bigBedFields(char *fileName)
/* Print out info about fields in bed file. */
{
struct bbiFile *bbi = bigBedFileOpen(fileName);
printf("%d bed definition fields, %d total fields\n", bbi->definedFieldCount, bbi->fieldCount);
struct asObject *as = bigBedAs(bbi);
if (as != NULL)
    {
    struct asColumn *col;
    for (col = as->columnList; col != NULL; col = col->next)
        {
	printf("\t%s\t%s\n", col->name, col->comment);
	}
    }
else
    {
    printf("No additional field information included.\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
if (optionExists("fields"))
    {
    if (argc < 2)
        usage();
    bigBedFields(argv[1]);
    }
else
    {
    summaryType = optionVal("type", summaryType);
    if (argc != 6)
	usage();
    bigBedSummary(argv[1], argv[2], sqlUnsigned(argv[3]), sqlUnsigned(argv[4]), sqlUnsigned(argv[5]));
    }
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
