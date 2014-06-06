/* chainSwap - Swap target and query in chain. */

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
  "chainSwap - Swap target and query in chain\n"
  "usage:\n"
  "   chainSwap in.chain out.chain\n"
  );
}

void chainSwapFile(char *in, char *out)
/* chainSwap - Swap target and query in chain. */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f = mustOpen(out, "w");
struct chain *chain;
while ((chain = chainRead(lf)) != NULL)
    {
    chainSwap(chain);
    chainWrite(chain, f);
    chainFree(&chain);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
chainSwapFile(argv[1], argv[2]);
return 0;
}
