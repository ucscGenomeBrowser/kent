/* mafCounts - Filter out maf files. */

/* Copyright (C) 2011 The Regents of the University of California 
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


char *masterSpecies;
char *masterChrom;
struct hash *speciesHash;
struct subSpecies *speciesList;
struct strandHead *strandHeads;

boolean addN = FALSE;
boolean addDash = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafCounts - count the number of A,T,C,G,N in a maf and output four wiggles\n"
  "usage:\n"
  "   mafCounts mafIn wigPrefix\n"
  "WARNING:  requires a maf with only a single target sequence\n"
  "options:\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

unsigned letterBox[256];
unsigned ourBufSize =  16 * 1024;

void mafCounts(char *mafIn, char *wigPrefix)
/* mafCounts - Filter out maf files. */
{
int jj;
char buffer[4096];
FILE *f[6];
safef(buffer, sizeof buffer, "%sA.wig", wigPrefix);
f[0] = mustOpen(buffer, "w");
safef(buffer, sizeof buffer, "%sC.wig", wigPrefix);
f[1] = mustOpen(buffer, "w");
safef(buffer, sizeof buffer, "%sT.wig", wigPrefix);
f[2] = mustOpen(buffer, "w");
safef(buffer, sizeof buffer, "%sG.wig", wigPrefix);
f[3] = mustOpen(buffer, "w");
safef(buffer, sizeof buffer, "%sD.wig", wigPrefix);
f[4] = mustOpen(buffer, "w");
safef(buffer, sizeof buffer, "%sN.wig", wigPrefix);
f[5] = mustOpen(buffer, "w");
struct mafAli *maf;
struct mafFile *mf = mafOpen(mafIn);

unsigned *counts = needMem(6 * ourBufSize * sizeof(unsigned));
int last = -1;

for(; (maf = mafNext(mf)) != NULL;)
    {
    struct mafComp *comp = maf->components;
    int seqLen = strlen(comp->text);
    if (seqLen > ourBufSize)
        errAbort("ourBufSize not big enough for %d\n", seqLen);

    if (comp->start != last)
        {
        char *dot = strchr(comp->src, '.');
        *dot++ = 0;
        for(jj = 0; jj < 6; jj++)
	    fprintf(f[jj], "fixedStep chrom=%s start=%d step=1\n", dot, comp->start + 1);
        }
    last = comp->start + seqLen;
    memset(counts, 0, 6 * ourBufSize * sizeof(unsigned));
    int ii;
    for(; comp; comp = comp->next)
        {
        char *str = comp->text;
        char *end = &str[seqLen];

        for(ii=0; str < end; str++, ii++)
            counts[letterBox[tolower(*str)] * ourBufSize + ii ]++;
        }
    char *str = maf->components->text;
    for(ii = 0; ii < seqLen; ii++, str++)
        {
        if (*str == '-')
            continue;


	double total = 0;
        for(jj = 0; jj < 4; jj++)
		{
		unsigned offset = jj * ourBufSize + ii;
		total += counts[offset];
		}

        for(jj = 0; jj < 4; jj++)
        //for(jj = 0; jj < 6; jj++)
            {
            unsigned offset = jj * ourBufSize + ii;
            fprintf(f[jj],"%g\n",counts[offset]/total);
            counts[offset] = 0;
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

letterBox['a'] = 0;
letterBox['c'] = 1;
letterBox['t'] = 2;
letterBox['g'] = 3;
letterBox['-'] = 4;
letterBox['n'] = 5;
mafCounts(argv[1],  argv[2]);
return 0;
}
