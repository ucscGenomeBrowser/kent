/* bigChainToChain - convert bigChain files back into a chain file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "chain.h"
#include "bigBed.h"
#include "bigChain.h"
#include "chainNetDbLoad.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigChainToChain - convert bigChain files back into a chain file\n"
  "usage:\n"
  "   bigChainToChain bigChain.bb bigLinks.bb output.chain\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void bigChainToChain(char *inChain, char *inLinks, char *out)
/* bigChainArrange - output a set of rearrangement breakpoints. */
{
struct bbiFile *chainBbi = bigBedFileOpen(inChain);
struct bbiChromInfo *chrom, *chromList = bbiChromList(chainBbi);
FILE *f = mustOpen(out, "w");

for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct chain  *chains = chainLoadIdRangeHub(NULL, inChain, inLinks, chrom->name, 0, chrom->size, -1);
    chainWriteAll(chains, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
bigChainToChain(argv[1], argv[2], argv[3]);
return 0;
}
