/* pslxToFa - convert pslx to fasta file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslxToFa - convert pslx to fasta file\n"
  "usage:\n"
  "   pslxToFa XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void pslxToFa(char *pslName, char *faName)
/* pslxToFa - convert pslx to fasta file. */
{
struct lineFile *in = pslFileOpen(pslName);
FILE *out = mustOpen(faName, "w");
struct psl *psl;

while ((psl = pslNext(in)) != NULL)
    {
    int ii=0;
    fprintf(out,">%s_%d_%d\t%d\t%d\n%s\n",psl->qName, 0, psl->blockCount,psl->blockSizes[0]*3, psl->tBaseInsert, psl->qSequence[0]);
    for(ii=1; ii < psl->blockCount; ii++)
	fprintf(out,">%s_%d_%d\t%d\t%d\n%s\n",psl->qName, ii, psl->blockCount,psl->blockSizes[ii]*3, psl->tStarts[ii] - (psl->tStarts[ii -1] + 3*psl->blockSizes[ii-1]), psl->qSequence[ii]);
    pslFree(&psl);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
pslxToFa(argv[1], argv[2]);
return 0;
}
