/* bigChainToChain - convert bigChain files back into a chain file. */
#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "chain.h"
#include "bigBed.h"
#include "bigChain.h"
#include "chainNetDbLoad.h"
#include "options.h"
#include "basicBed.h"
#include "bigBedCmdSupport.h"

char *clChrom = NULL;
int clStart = -1;
int clEnd = -1;
char *clBed = NULL;
char *clPos = NULL;

char *inChain = NULL;
char *inLinks = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigChainToChain - convert bigChain files back into a chain file\n"
  "usage:\n"
  "   bigChainToChain bigChain.bb bigLinks.bb output.chain\n"
  "options:\n"
  "   -chrom=chr1 - if set restrict output to given chromosome\n"
  "   -start=N - if set, restrict output to only that over start\n"
  "   -end=N - if set, restict output to only that under end\n"
  "   -bed=in.bed - restrict output to all regions in a BED file\n"
  "   -positions=in.pos - restrict output to all regions in a position file with 1-based start\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {"bed", OPTION_STRING},
   {"positions", OPTION_STRING},
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

static void processChromChunk(struct bbiFile *bbi, char *chrom,
                              int start, int end, char *bedName, FILE *f)
/* Output one chunk.  Only blocks where start is in the range will be written
 * to avoid outputting a block multiple tines.  */
{
    char *chromName = chrom;
    struct chain  *chains = chainLoadIdRangeHub(NULL, inChain, inLinks, chromName, start, end, -1);
    chainWriteAll(chains, f);
}


void bigChainToChain(char *out)
/* bigChainArrange - output a set of rearrangement breakpoints. */
{
struct bbiFile *bbi = bigBedFileOpen(inChain);
FILE *f = mustOpen(out, "w");

if (clBed != NULL)
    {
    genericBigToNonBigFromBed(bbi, clBed, f, &processChromChunk);
    return;
    }
if (clPos != NULL)
    {
    genericBigToNonBigFromPos(bbi, clPos, f, &processChromChunk);
    return;
    }

struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);

for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (clChrom != NULL && !sameString(clChrom, chrom->name))
        continue;

    int start = 0, end = chrom->size;
    if (clStart >= 0)
        start = clStart;
    if (clEnd >= 0)
	{
        end = clEnd;
	if (end > chrom->size)
	    end = chrom->size;
	}
    if (start > end)
	errAbort("invalid range, start=%d > end=%d", start, end);

    processChromChunk(bbi, chrom->name, start, end, NULL, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
clChrom = optionVal("chrom", clChrom);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
clBed = optionVal("bed", clBed);
clPos = optionVal("positions", clPos);
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));

if (argc != 3)
    usage();

if ((clBed || clPos) && (clChrom || (clStart >= 0) || (clEnd >= 0)))
    errAbort("-bed or -positions can not be used with -chrom -start or -end options");
if (clBed && clPos)
    errAbort("-bed and -positions can not be used together");

inChain = argv[1];
inLinks = argv[2];
 
bigChainToChain(argv[3]);

if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
