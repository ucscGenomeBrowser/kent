/* chainSplit - Split chains up by target or query sequence. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "chainBlock.h"

boolean splitOnQ = FALSE;
void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainSplit - Split chains up by target or query sequence\n"
  "usage:\n"
  "   chainSplit outDir inChain(s)\n"
  "options:\n"
  "   -q  - Split on query (default is on target)\n"
  );
}


void chainSplit(char *outDir, int inCount, char *inFiles[])
/* chainSplit - Split chains up by target or query sequence. */
{
struct hash *hash = newHash(0);
int inIx;
makeDir(outDir);
for (inIx = 0; inIx < inCount; ++inIx)
    {
    struct lineFile *lf = lineFileOpen(inFiles[inIx], TRUE);
    struct chain *chain;
    FILE *f;
    while ((chain = chainRead(lf)) != NULL)
        {
	char *chromName = (splitOnQ ? chain->qName : chain->tName);
	if ((f = hashFindVal(hash, chromName)) == NULL)
	    {
	    char path[512];
	    sprintf(path, "%s/%s.chain", outDir, chromName);
	    f = mustOpen(path, "w");
	    hashAdd(hash, chromName, f);
	    }
	chainWrite(chain, f);
	chainFree(&chain);
	}
    lineFileClose(&lf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
splitOnQ = optionExists("q");
if (argc < 3)
    usage();
chainSplit(argv[1], argc-2, argv+2);
return 0;
}
