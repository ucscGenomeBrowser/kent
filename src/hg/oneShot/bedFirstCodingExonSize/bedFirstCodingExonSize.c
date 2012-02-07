/* bedFirstCodingExonSize - Figure out size of first coding exon.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"


int threshold = 100;
void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedFirstCodingExonSize - Figure out size of first coding exon.\n"
  "usage:\n"
  "   bedFirstCodingExonSize in.bed out.size\n"
  "options:\n"
  "   -over=over.bed - Put beds with first coding exon of threshold or greater here.\n"
  "   -under=under.bed - Put beds with first coding exon under threshold here.\n"
  "   -threshold=%d - Threshold\n"
  , threshold
  );
}

static struct optionSpec options[] = {
   {"over", OPTION_STRING},
   {"under", OPTION_STRING},
   {"threshold", OPTION_INT},
   {NULL, 0},
};

int bedFirstCdsSize(struct bed *bed)
/* Return size of coding portion of first coding exon. */
{
int chromStart = bed->chromStart;
if (bed->strand[0] == '-')
    {
    int i;
    for (i=bed->blockCount-1; i >= 0; --i)
        {
	int start = chromStart + bed->chromStarts[i];
	int end = start + bed->blockSizes[i];
	int cdsSize = rangeIntersection(start, end, bed->thickStart, bed->thickEnd);
	if (cdsSize > 0)
	    return cdsSize;
	}
    }
else
    {
    int i;
    for (i=0; i<bed->blockCount; ++i)
        {
	int start = chromStart + bed->chromStarts[i];
	int end = start + bed->blockSizes[i];
	int cdsSize = rangeIntersection(start, end, bed->thickStart, bed->thickEnd);
	if (cdsSize > 0)
	    return cdsSize;
	}
    }
return 0;
}

void bedFirstCodingExonSize(char *inBed, char *overBed, char *underBed, char *outSize)
/* bedFirstCodingExonSize - Figure out size of first coding exon. */
{
FILE *fSize = mustOpen(outSize, "w");
FILE *fOver = NULL, *fUnder = NULL;
if (overBed)
    fOver = mustOpen(overBed, "w");
if (underBed)
    fUnder = mustOpen(underBed, "w");
struct bed *bed, *bedList = bedLoadNAll(inBed, 12);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (bed->thickStart < bed->thickEnd)
        {
	int firstCdsSize = bedFirstCdsSize(bed);
	fprintf(fSize, "%s\t%d\n", bed->name, firstCdsSize);
	if (firstCdsSize >= threshold)
	    {
	    if (fOver != NULL)
	        bedTabOutN(bed, 12, fOver);
	    }
	else
	    {
	    if (fUnder != NULL)
	        bedTabOutN(bed, 12, fUnder);
	    }
	}
    }
carefulClose(&fSize);
carefulClose(&fOver);
carefulClose(&fUnder);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *over = optionVal("over", NULL);
char *under = optionVal("under", NULL);
threshold = optionInt("threshold", threshold);
bedFirstCodingExonSize(argv[1], over, under, argv[2]);
return 0;
}
