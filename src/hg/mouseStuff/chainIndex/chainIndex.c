/* chainIndex - Create simple two column file index for chain. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainIndex - Create simple two column file index for chain\n"
  "usage:\n"
  "   chainIndex in.chain out.index\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void chainIndex(char *inChain, char *outIndex)
/* chainIndex - Create simple two column file index for chain. */
{
struct lineFile *lf = lineFileOpen(inChain, TRUE);
FILE *f = mustOpen(outIndex, "w");
struct chain *chain, *lastChain = NULL;
long pos = 0;
struct hash *uniqHash = hashNew(16);

while ((chain = chainRead(lf)) != NULL)
    {
    if (lastChain == NULL || !sameString(chain->tName, lastChain->tName))
	{
	if (hashLookup(uniqHash, chain->tName))
	    {
	    errAbort("%s is not sorted, %s repeated with intervening %s", 
	    	inChain, chain->tName, lastChain->tName);
	    }
	hashAddInt(uniqHash, chain->tName, pos);
        fprintf(f, "%lx\t%s\n", pos, chain->tName);
	}
    chainFree(&lastChain);
    lastChain = chain;
    pos = lineFileTell(lf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
chainIndex(argv[1], argv[2]);
return 0;
}
