/* chainToBigChain - converts chain to bigChain. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "chain.h"
#include "bigChain.h"
#include "bigLink.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainToBigChain - converts chain to bigChain input (bed format with extra fields)\n"
  "usage:\n"
  "  chainToBigChain chainIn bigChainOut bigLinkOut\n"
  "\n"
  "Output will be sorted\n"
  "\n"
  "To build bigBed files:\n"
  "  bedToBigBed -type=bed6+6 -as=bigChain.as -tab data.bigChain hg38.chrom.sizes data.bb\n"
  "  bedToBigBed -type=bed4+1 -as=bigLink.as -tab data.bigLink hg38.chrom.sizes data.link.bb\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static void convertChains(char *chainIn, struct bigChain **bigChains, struct bigLink **bigLinks)
/* convert all chains to bigChains and links  */
{
struct chain *chain;
struct lineFile *lf = lineFileOpen(chainIn, TRUE);
while ((chain = chainRead(lf)) != NULL)
    {
    chainToBigChainOne(chain, bigChains, bigLinks);
    chainFree(&chain);
    }
lineFileClose(&lf);
slSort(bigChains, bigChainCmpTarget);
slSort(bigLinks, bigLinkCmpTarget);
}

static void bigChainWrite(char *bigChainOut, struct bigChain *bigChains)
/* write bigChain records to a tab file */
{
FILE *fh = mustOpen(bigChainOut, "w");
for (struct bigChain *bc = bigChains; bc != NULL; bc = bc->next)
    bigChainTabOut(bc, fh);
carefulClose(&fh);
}

static void bigLinkWrite(char *bigLinkOut, struct bigLink *bigLinks)
/* write bigLink records to a tab file */
{
FILE *fh = mustOpen(bigLinkOut, "w");
for (struct bigLink *cl = bigLinks; cl != NULL; cl = cl->next)
    bigLinkTabOut(cl, fh);
carefulClose(&fh);
}

static void chainToBigChain(char *chainIn, char *bigChainOut, char *bigLinkOut)
/* chainToBigChain - converts chains to bigChain and bigLink files. */
{
struct bigChain *bigChains = NULL;
struct bigLink *bigLinks = NULL;
convertChains(chainIn, &bigChains, &bigLinks);
bigChainWrite(bigChainOut, bigChains);
bigLinkWrite(bigLinkOut, bigLinks);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
chainToBigChain(argv[1], argv[2], argv[3]);
return 0;
}
