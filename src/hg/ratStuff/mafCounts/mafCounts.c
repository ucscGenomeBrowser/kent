/* mafCounts - count A,C,T,G in maf files and make four wiggles. */

/* Copyright (C) 2022 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "errAbort.h"
#include "linefile.h"
#include "hash.h"
#include "chain.h"
#include "options.h"
#include "maf.h"
#include "bed.h"
#include "twoBit.h"
#include "binRange.h"
#include "bigWig.h"

char *masterSpecies;
char *masterChrom;
struct hash *speciesHash;
struct subSpecies *speciesList;
struct strandHead *strandHeads;
struct bbiFile *scaleBbi ;
struct bigWigValsOnChrom *scaleVals;
double dataMin, dataRange;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafCounts - count the number of A,T,C,G in a maf and output four wiggles with values that sum to one\n"
  "usage:\n"
  "   mafCounts mafIn wigPrefix\n"
  "WARNING:  requires a maf with only a single target sequence\n"
  "options:\n"
  "   -scale=file.bw     scale each wiggle by the bigWig value at that base.  This might be something like a phyloP score.\n"
  );
}

static struct optionSpec options[] = {
   {"scale", OPTION_STRING},
   {NULL, 0},
};

unsigned char letterBox[256];  // to make ASCII values to indices
unsigned ourBufSize =  256 *1024;   // this is the max size of a maf block

#define NUMBER_OF_NUCS   4

void mafCounts(char *mafIn, char *wigPrefix)
/* mafCounts - count A,C,T,G */
{
int jj;
char buffer[4096];  // holds a relatively short filename
FILE *f[NUMBER_OF_NUCS]; // one file for each different nucleotide

safef(buffer, sizeof buffer, "%s.A.wig", wigPrefix);
f[0] = mustOpen(buffer, "w");
safef(buffer, sizeof buffer, "%s.C.wig", wigPrefix);
f[1] = mustOpen(buffer, "w");
safef(buffer, sizeof buffer, "%s.T.wig", wigPrefix);
f[2] = mustOpen(buffer, "w");
safef(buffer, sizeof buffer, "%s.G.wig", wigPrefix);
f[3] = mustOpen(buffer, "w");
struct mafAli *maf;
struct mafFile *mf = mafOpen(mafIn);

unsigned *counts = needLargeMem(NUMBER_OF_NUCS * ourBufSize * sizeof(unsigned));   // this array keeps the counts of each of the 4 possible nucleotides
int last = -1;
double *values = NULL;
char *lastChrom = NULL;
double scaleValue = 0.0;

for(; (maf = mafNext(mf)) != NULL;)
    {
    struct mafComp *comp = maf->components;
    int seqLen = strlen(comp->text);
    if (seqLen > ourBufSize)
        errAbort("ourBufSize not big enough for %d\n", seqLen);
    unsigned blockStart = comp->start;

    // if this block is not contiguos with the last one, we need to start a new wig span
    if (blockStart != last)
        {
        char *chrom = strchr(comp->src, '.');
        *chrom++ = 0;
        for(jj = 0; jj < NUMBER_OF_NUCS; jj++)
	    fprintf(f[jj], "fixedStep chrom=%s start=%d step=1\n", chrom, blockStart + 1);

        // if we're scaling and we're on a new chrom, grab the scale values from the bigWig
        if (scaleBbi && !sameOk(lastChrom, chrom))
            {
            bigWigValsOnChromFetchData(scaleVals, chrom, scaleBbi);
            values = scaleVals->valBuf;
            lastChrom = cloneString(chrom);
            }
        }
    // keep track of where this block ends
    last = blockStart + seqLen;
    int ii;

    // for each maf block we reset the counts of each type of nucleotide to zero
    memset(counts, 0, NUMBER_OF_NUCS * ourBufSize * sizeof(unsigned));

    // now go through each component of the maf block and count the nucs
    for(; comp; comp = comp->next)
        {
        if (comp->text == NULL)
            continue;

        char *str = comp->text;
        char *end = &str[seqLen];

        for(ii=0; str < end; str++, ii++)
            {
            unsigned char nucToIndex = letterBox[tolower(*str)];
            if (nucToIndex > 0)
                counts[(nucToIndex - 1) * ourBufSize + ii]++;
            }
        }

    // now we normalize and scale if asked.
    char *str = maf->components->text;
    unsigned scaleOffset = 0;
    for(ii = 0; ii < seqLen; ii++, str++)
        {
        if (*str == '-')
            continue;

        if (scaleBbi && (values != NULL))
            scaleValue = (values[blockStart + scaleOffset]);

        scaleOffset++;
	double total = 0;
        for(jj = 0; jj < NUMBER_OF_NUCS; jj++)
            {
            unsigned offset = jj * ourBufSize + ii;
            total += counts[offset];
            }

        for(jj = 0; jj < NUMBER_OF_NUCS; jj++)
            {
            unsigned offset = jj * ourBufSize + ii;
            double value = counts[offset]/total;

            if (scaleBbi)
                value *= scaleValue;

            fprintf(f[jj],"%g\n",value);
            }
        }

    mafAliFree(&maf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

char *scaleBw = optionVal("scale", NULL);
if (scaleBw)
    {
    scaleBbi = bigWigFileOpen(scaleBw);
    struct bbiSummaryElement sum = bbiTotalSummary(scaleBbi);
    dataMin = sum.minVal;
    dataRange = sum.maxVal - dataMin;
    scaleVals = bigWigValsOnChromNew();

    }

letterBox['a'] = 1;
letterBox['c'] = 2;
letterBox['t'] = 3;
letterBox['g'] = 4;
mafCounts(argv[1],  argv[2]);
return 0;
}
