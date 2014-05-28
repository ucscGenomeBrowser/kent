/* faTrimPolyA - trim Poly-A tail*/

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "fa.h"
#include "dnautil.h"


/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"polyA", OPTION_BOOLEAN},
    {"polyT", OPTION_BOOLEAN},
    {NULL, 0}
};
boolean trimPolyA = FALSE;
boolean trimPolyT = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faTrimPolyA - trim poly-A tails\n"
  "usage:\n"
  "   faTrimPolyA in.fa out.fa \n"
  "Options:\n"
  "  -polyA - trim poly-As at the end of the sequence.  This is the\n"
  "   default if no trim option is specified.\n" 
  "  -polyT - trim poly-Ts at the beginning of the sequence.  To trim both,\n"
  "   specify both options.\n"
  );
}

void polyTrimSeq(struct dnaSeq *seq, FILE *fh)
/*  trim a sequence */
{
if (trimPolyA)
    {
    int sz = maskTailPolyA(seq->dna, seq->size);
    seq->size -= sz;
    seq->dna[seq->size] = '\0';
    }
if (trimPolyT)
    {
    int sz = maskHeadPolyT(seq->dna, seq->size);
    seq->size -= sz;
    seq->dna += sz;
    }

faWriteNext(fh, seq->name, seq->dna, seq->size);
}

void faTrimPolyA(char *inFile, char *outFile)
/* faTrimPolyA - trim Poly-A tail*/
{
struct dnaSeq seq;
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *fh = mustOpen(outFile, "w");
ZeroVar(&seq);

while (faSomeSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name, FALSE))
    polyTrimSeq(&seq, fh);
carefulClose(&fh);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionInit(&argc, argv, optionSpecs);
if (argc != 3 )
    usage();
trimPolyA = optionExists("polyA");
trimPolyT = optionExists("polyT");
if (!(trimPolyA || trimPolyT))
    trimPolyA = TRUE;

faTrimPolyA(argv[1], argv[2]);
return 0;
}
