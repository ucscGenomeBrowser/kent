/* pslToChain - Convert psl records to chain records */

/* Copyright (C) 2008 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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
  "Options:\n"
  "   -ignore   ignore psl records with negative target strand rather than exiting\n"
  );
}

boolean ignoreError;

static struct optionSpec options[] = {
   {"ignore", OPTION_BOOLEAN},
   {NULL, 0},
};

void pslToChain(char *pslIn, char *chainOut)
/* pslToChain - Extract multiple psl records. */
{
struct lineFile *lf = pslFileOpen(pslIn);
int chainId = 1;
int ii;
FILE *f = mustOpen(chainOut, "w");
struct psl *psl;
struct chain chain;

while ((psl = pslNext(lf) ) != NULL)
    {
    if (psl->strand[1] == '-') 
        {
        if (ignoreError)
            continue;
        errAbort("PSL record on line %d has '-' for target strand which is not allowed.", lf->lineIx);
        }

    chain.score = pslScore(psl);
    chain.id = chainId++;
    chain.tName = psl->tName;
    chain.tSize = psl->tSize;
    chain.tStart = psl->tStart;
    chain.tEnd = psl->tEnd;
    chain.qName = psl->qName;
    chain.qSize = psl->qSize;
    chain.qStrand = psl->strand[0];

    if (psl->strand[0] == '-')
        {
        chain.qEnd = psl->qSize - psl->qStart;
        chain.qStart = psl->qSize - psl->qEnd;
        }
    else
        {
        chain.qStart = psl->qStart;
        chain.qEnd = psl->qEnd;
        }
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
ignoreError = optionExists("ignore");
pslToChain(argv[1], argv[2]);
return 0;
}
