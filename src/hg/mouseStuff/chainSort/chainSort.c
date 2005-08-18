/* chainSort - Sort chains. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"

static char const rcsid[] = "$Id: chainSort.c,v 1.5 2005/08/18 07:42:12 baertsch Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainSort - Sort chains.  By default sorts by score.\n"
  "Note this loads all chains into memory, so it is not\n"
  "suitable for large sets.  Use chainMergeSort for that\n"
  "usage:\n"
  "   chainSort inFile outFile\n"
  "Note that inFile and outFile can be the same\n"
  "options:\n"
  "   -target sort on target start rather than score\n"
  "   -query sort on query start rather than score\n"
  );
}

void chainSort(char *inFile, char *outFile)
/* chainSort - Sort chains. */
{
struct chain *chainList = NULL, *chain;
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");

lineFileSetMetaDataOutput(lf, f);

/* Read in all chains. */
while ((chain = chainRead(lf)) != NULL)
    {
    slAddHead(&chainList, chain);
    }
lineFileClose(&lf);

/* Sort. */
if (optionExists("target"))
    slSort(&chainList, chainCmpTarget);
else if (optionExists("query"))
    slSort(&chainList, chainCmpQuery);
else
    slSort(&chainList, chainCmpScore);

/* Output. */
for (chain = chainList; chain != NULL; chain = chain->next)
    chainWrite(chain, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
chainSort(argv[1], argv[2]);
return 0;
}
