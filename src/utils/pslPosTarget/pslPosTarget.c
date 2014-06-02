/* pslPosTarget - flip psl strands so target is positive. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslPosTarget - flip psl strands so target is positive\n"
  "usage:\n"
  "   pslPosTarget inPsl outPsl\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

char flipStrand(char strand)
/* Turn + to - and vice versa. */
{
return (strand == '-' ? '+' : '-');
}

void pslPosTarget(char *pslFileName, char *outFileName)
/* pslPosTarget - flip psl strands so target is positive. */
{
struct lineFile *pslLf = pslFileOpen(pslFileName);
FILE *out = mustOpen(outFileName, "w");
struct psl *psl;
int ii;

while ((psl = pslNext(pslLf)) != NULL)
    {
    if (psl->strand[1] == '-')
	{
	    psl->strand[0] = flipStrand(psl->strand[0]);
	    psl->strand[1] = '+';
	    reverseInts(psl->blockSizes, psl->blockCount);
	    reverseInts(psl->qStarts, psl->blockCount);
	    reverseInts(psl->tStarts, psl->blockCount);
	    for(ii=0; ii < psl->blockCount; ii++)
		psl->qStarts[ii] = psl->qSize - 
		    (psl->qStarts[ii] + psl->blockSizes[ii]);
	    for(ii=0; ii < psl->blockCount; ii++)
		psl->tStarts[ii] = psl->tSize - 
		    (psl->tStarts[ii] + psl->blockSizes[ii]);
	}
    pslTabOut(psl, out);
    }
lineFileClose(&pslLf);
fclose(out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
pslPosTarget(argv[1], argv[2]);
return 0;
}
