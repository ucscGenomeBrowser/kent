/* bedThickToCds - Create CDS predictions in RNA coordinates from thickStart/thickEnd 
 * of BED file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedThickToCds - Create CDS predictions in RNA coordinates from \n"
  "thickStart/thickEnd of BED file.\n"
  "usage:\n"
  "   bedThickToCds in.bed out.cds\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

boolean bedToRnaCds(struct bed *bed, int *retCdsStart, int *retCdsEnd)
/* If bed has thickStart/thickEnd set then return coding region 
 * in RNA coordinates, and TRUE.  Else return FALSE. */
{
/* Get transcription and CDS coordinates and convert them to be relative
 * to transcription start. */
int chromStart = bed->chromStart, chromEnd = bed->chromEnd;
int thickStart = bed->thickStart - chromStart;
int thickEnd = bed->thickEnd - chromStart;
chromStart -= chromStart;
chromEnd -= chromStart;
if (thickStart >= thickEnd)
    return FALSE;

/* Total up bases seen in coding regions and UTRs before and after. */
int startUtrSize = 0, cdsSize = 0, endUtrSize = 0;
int i;
for (i=0; i<bed->blockCount; ++i)
    {
    int start = bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    startUtrSize += positiveRangeIntersection(start, end, chromStart, thickStart);
    cdsSize += positiveRangeIntersection(start, end, thickStart, thickEnd);
    endUtrSize += positiveRangeIntersection(start, end, thickEnd, chromEnd);
    }

if (bed->strand[0] == '+')
    {
    *retCdsStart = startUtrSize;
    *retCdsEnd = startUtrSize + cdsSize;
    }
else
    {
    *retCdsStart = endUtrSize;
    *retCdsEnd = endUtrSize + cdsSize;
    }
return TRUE;
}

void bedThickToCds(char *inBed, char *outCds)
/* bedThickToCds - Create CDS predictions in RNA coordinates from thickStart/thickEnd 
 * of BED file. */
{
struct bed *bed, *bedList = bedLoadNAll(inBed, 12);
FILE *f = mustOpen(outCds, "w");
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    int cdsStart, cdsEnd;
    if (bedToRnaCds(bed, &cdsStart, &cdsEnd))
        fprintf(f, "%s\t%d\t%d\n", bed->name, cdsStart, cdsEnd);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bedThickToCds(argv[1], argv[2]);
return 0;
}
