/* encode2BedDoctor - Selectively fix up encode2 bed files.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "basicBed.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2BedDoctor - Selectively fix up encode2 bed files.\n"
  "usage:\n"
  "   encode2BedDoctor XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

boolean bedFixBlocks(struct bed *bed)
/* Make sure no blocks in bed list overlap. */
{
int i;
boolean anyFix = FALSE;
for (i=0; i<bed->blockCount-1; ++i)
    {
    int start = bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    int nextStart = bed->chromStarts[i+1];
    int overlap = end - nextStart;
    if (overlap > 0)
        {
	verbose(2, "Fixing block overlap of %d\n", overlap);
	anyFix = TRUE;
	int mid = (end + nextStart)/2;
	bed->blockSizes[i] = mid - start;
	int nextEnd = nextStart + bed->blockSizes[i+1];
	bed->chromStarts[i+1] = mid;
	bed->blockSizes[i+1] = nextEnd - mid;
	}
    }
return anyFix;
}

void encode2BedDoctor(char *inBed, char *outBed)
/* encode2BedDoctor - Selectively fix up encode2 bed files.. */
{
struct bed *bed, *bedList = NULL;
int fieldCount;
boolean itemRgb;
bedLoadAllReturnFieldCountAndRgb(inBed, &bedList, &fieldCount, &itemRgb);
int fixCount = 0;
FILE *f = mustOpen(outBed, "w");
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (fieldCount >= 12)
	if (bedFixBlocks(bed))
	    ++fixCount;
    }
carefulClose(&f);
verbose(1, "Fixed %d items in %s\n", fixCount, inBed);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
encode2BedDoctor(argv[1], argv[2]);
return 0;
}
