/* bedSmash - filter out beds that overlap other beds. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bbiFile.h"
#include "basicBed.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedSmash - filter out beds that overlap other beds, input must be sort on chrom and priority (higher priority first)\n"
  "usage:\n"
  "   bedSmash input.bed chrom.sizes output.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void bedSmash(char *inBed, char *chromSizes, char *outBed)
/* bedSmash - filter out beds that overlap other beds. */
{
struct hash *chromSizesHash = bbiChromSizesFromFile(chromSizes);
FILE *f = mustOpen(outBed, "w");
struct bed *bed = bedLoadAll(inBed);
char *currentChrom = "notAChrom";
Bits *bitmap = NULL;

for(; bed; bed = bed->next)
    {
    if (!sameString(currentChrom, bed->chrom))
	{
	currentChrom = bed->chrom;
	int chromSize = hashIntVal(chromSizesHash, currentChrom);
	bitmap = bitAlloc(chromSize);
	}

    int ii;
    for(ii=0; ii < bed->blockCount; ii++)
	if ( bitCountRange(bitmap, bed->chromStart+bed->chromStarts[ii], bed->blockSizes[ii]) )
	    break;

    if (ii == bed->blockCount)
	{
	for(ii=0; ii < bed->blockCount; ii++)
	    bitSetRange(bitmap, bed->chromStart+bed->chromStarts[ii], bed->blockSizes[ii]);

	bedTabOutN(bed, 12,f);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
bedSmash(argv[1], argv[2], argv[3]);
return 0;
}
