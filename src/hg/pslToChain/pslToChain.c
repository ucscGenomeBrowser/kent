/* pslToChain - Convert psl records to chain records */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "chain.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslToChain - Convert psl records to chain records \n"
  "usage:\n"
  "   pslToChain pslIn chainOut\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void pslToChain(char *pslIn, char *chainOut)
/* pslToChain - Extract multiple psl records. */
{
struct lineFile *lf = pslFileOpen(pslIn);
int ii;
FILE *f = mustOpen(chainOut, "w");
struct psl *psl;
struct chain chain;

while ((psl = pslNext(lf) ) != NULL)
    {
    chain.score = pslScore(psl);
    chain.id = 0;
    chain.tName = psl->tName;
    chain.tSize = psl->tSize;
    chain.tStart = psl->tStart;
    chain.tEnd = psl->tEnd;
    chain.qName = psl->qName;
    chain.qSize = psl->qSize;
    chain.qStart = psl->qStart;
    chain.qEnd = psl->qEnd;
    chain.qStrand = psl->strand[0];
    chainWriteHead(&chain,f);

    for(ii=0; ii < psl->blockCount; ii++)
	{
	fprintf(f, "%d", psl->blockSizes[ii]);
	if (ii < psl->blockCount - 1)
	    fprintf(f, "\t%d\t%d", psl->tStarts[ii+1]-(psl->tStarts[ii] + psl->blockSizes[ii]),
		psl->qStarts[ii+1]-(psl->qStarts[ii] + psl->blockSizes[ii]));
	fprintf(f,"\n");
	}


    pslFree(&psl);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
pslToChain(argv[1], argv[2]);
return 0;
}
