/* bedMergeAdjacent - merge adjacent blocks in a BED 12. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "basicBed.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedMergeAdjacent - merge adjacent blocks in a BED 12\n"
  "usage:\n"
  "   bedMergeAdjacent inBed outBed\n"
  "options:\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static void fixBed(struct bed *bed)
/* merge adjacent blocks in a bed */
{
int iInBlk, iOutBlk = 0;
for (iInBlk = 1; iInBlk < bed->blockCount; iInBlk++)
    {
    unsigned chromNext = bed->chromStarts[iOutBlk] + bed->blockSizes[iOutBlk];
    if (bed->chromStarts[iInBlk] < chromNext)
        errAbort("overlapping BED blocks: %s:%u-%u (%s)", bed->chrom, bed->chromStart, bed->chromEnd, bed->name);
    if (bed->chromStarts[iInBlk] == chromNext)
        {
        // adjacent, merge
        bed->blockSizes[iOutBlk] += bed->blockSizes[iInBlk];
        }
    else
        {
        // not adjacent
        iOutBlk++;
        bed->chromStarts[iOutBlk] = bed->chromStarts[iInBlk];
        bed->blockSizes[iOutBlk] = bed->blockSizes[iInBlk];
        }
    }
bed->blockCount = iOutBlk + 1;
}

static void bedMergeAdjacent(char *inBed, char *outBed)
/* bedMergeAdjacent - merge adjacent blocks in a BED 12. */
{
FILE *outBedFh = mustOpen(outBed, "w");
struct lineFile *lf = lineFileOpen(inBed, TRUE);
char *line, *row[12];

while (lineFileNextReal(lf, &line))
    {
    int numFields = chopByWhite(line, row, ArraySize(row));
    if (numFields < 12)
	errAbort("file %s doesn't appear to be in blocked-bed format. At least 12 fields required, got %d", inBed, numFields);
    struct bed* bed = bedLoadN(row, numFields);
    fixBed(bed);
    bedTabOutN(bed, numFields, outBedFh);
    bedFree(&bed);
    }

carefulClose(&outBedFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bedMergeAdjacent(argv[1], argv[2]);
return 0;
}
