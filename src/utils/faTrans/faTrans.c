/* faTrans - Translate DNA .fa file to peptide. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "options.h"


/* command line options */
static struct optionSpec optionSpecs[] =
{
    {"stop", OPTION_BOOLEAN},
    {"offset", OPTION_INT},
    {"cdsUpper", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean clStop = FALSE;
int clOffset = 0;
boolean clCdsUpper = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faTrans - Translate DNA .fa file to peptide\n"
  "usage:\n"
  "   faTrans in.fa out.fa\n"
  "options:\n"
  "   -stop stop at first stop codon (otherwise puts in Z for stop codons)\n"
  "   -offset=N start at a particular offset.\n"
  "   -cdsUpper - cds is in upper case\n"
);
}

struct dnaSeq *transOne(struct dnaSeq *dna)
/* translate one mRNA sequence to a peptide */
{
int off = clOffset;
if (clCdsUpper)
    {
    /* adjust offset to first upper */
    char *p = dna->dna + off;
    while ((*p != '\0') && islower(*p))
        p++;
    off = p - dna->dna;
    /* zero-terminate at first lower */
    while ((*p != '\0') && isupper(*p))
        p++;
    *p = '\0';
    }
return translateSeq(dna, off, clStop);
}

void faTrans(char *inFile, char *outFile)
/* faTrans - Translate DNA .fa file to peptide. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct dnaSeq dna, *pep;
ZeroVar(&dna);

while (faMixedSpeedReadNext(lf, &dna.dna, &dna.size, &dna.name))
    {
    pep = transOne(&dna);
    faWriteNext(f, pep->name, pep->dna, pep->size);
    freeDnaSeq(&pep);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
clStop = optionExists("stop");
clOffset = optionInt("offset", clOffset);
clCdsUpper = optionExists("cdsUpper");
faTrans(argv[1], argv[2]);
return 0;
}
