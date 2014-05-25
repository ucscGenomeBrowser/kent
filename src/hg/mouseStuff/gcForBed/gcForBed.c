/* gcForBed - Calculate g/c percentage and other stats for regions covered by bed. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "dnautil.h"
#include "nib.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "gcForBed - Calculate g/c percentage and other stats for regions covered by bed\n"
  "usage:\n"
  "   gcForBed in.bed nibDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void gcForBed(char *bedName, char *nibDir)
/* gcForBed - Calculate g/c percentage and 
 * other stats for regions covered by bed. */
{
char nibFile[512];
int wordCount;
char *words[32];
struct lineFile *lf = lineFileOpen(bedName, TRUE);
static double baseCounts[5], totalBases = 0;

dnaUtilOpen();
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    struct bed *bed = bedLoadN(words, wordCount);
    struct dnaSeq *seq;
    int i, size;
    DNA *dna;

    sprintf(nibFile, "%s/%s.nib", nibDir, bed->chrom);
    seq = nibLoadPart(nibFile, bed->chromStart, 
    	bed->chromEnd - bed->chromStart);
    if (wordCount >= 6 && bed->strand[0] == '-')
        reverseComplement(seq->dna, seq->size);
    totalBases += seq->size;
    dna = seq->dna;
    size = seq->size;
    for (i=0; i<size; ++i)
	baseCounts[ntVal5[(int)dna[i]]] += 1;
    dnaSeqFree(&seq);
    bedFree(&bed);
    }

printf("A %4.2f%% C %4.2f%% G %4.2f%% T %4.2f%%\n",
	100.0 * baseCounts[A_BASE_VAL] / totalBases,
	100.0 * baseCounts[C_BASE_VAL] / totalBases,
	100.0 * baseCounts[G_BASE_VAL] / totalBases,
	100.0 * baseCounts[T_BASE_VAL] / totalBases);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
gcForBed(argv[1], argv[2]);
return 0;
}
