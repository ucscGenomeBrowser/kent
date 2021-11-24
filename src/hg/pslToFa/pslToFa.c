/* pslToFa - convert psl to fasta file. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslToFa - convert psl (with sequence) to fasta file\n"
  "usage:\n"
  "   pslToFa in.psl out.fa\n"
  "options:\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void printLines(FILE *f, char *s, int lineSize)
/* Print s, lineSize characters (or less) per line. */
{
int len = strlen(s);
int start, left;
int oneSize;

for (start = 0; start < len; start += lineSize)
    {
    oneSize = len - start;
    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, s+start, oneSize);
    fputc('\n', f);
    }
if (start != len)
    fputc('\n', f);
}
void pslToFa(char *pslName, char *faName)
/* pslToFa - convert psl to fasta file. */
{
struct lineFile *in = pslFileOpen(pslName);
FILE *out = mustOpen(faName, "w");
struct psl *psl;
int start, end;
int i, j;

while ((psl = pslNext(in)) != NULL)
    {
    int ii=0;
    int sumQuery = 0;
    char buffer[4096], *str = buffer;
    struct dnaSeq *tSeq;
    fprintf(out,">%s.%s:%d-%d\n",psl->qName,psl->tName, psl->tStart, psl->tEnd);

    start = psl->tStart;
    end = psl->tEnd;
    if (psl->strand[1] == '+')
	end = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1] *3;
    tSeq = hDnaFromSeq(psl->tName, start, end, dnaLower);

    if (psl->strand[1] == '-')
	{
	start = psl->tSize - end;
	reverseComplement(tSeq->dna, tSeq->size);
	}
    for (i=0; i<psl->blockCount; ++i)
	{
	int ts = psl->tStarts[i] - start;
	int sz = psl->blockSizes[i];

	for (j=0; j<sz; ++j)
	    {
	    int codonStart = ts + 3*j;
	    DNA *codon = &tSeq->dna[codonStart];

	    if (codonStart > tSeq->size)
		*str++ = 'X';
	    else
		*str++ = lookupCodon(codon);
	    }
	}

    *str = 0;
    printLines(out, buffer, 50);

    pslFree(&psl);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *liftTarget, *liftQuery;

optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hSetDb(argv[1]);
pslToFa(argv[2], argv[3]);
return 0;
}
