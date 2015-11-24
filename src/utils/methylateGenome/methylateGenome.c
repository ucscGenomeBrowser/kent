/* methylateGenome - Creates a methylated version of an input genome, in which any occurance of CG becomes TG. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaLoad.h"
#include "dnaseq.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "methylateGenome - Creates a methylated version of a genome, in which any occurrence of CG becomes TG\n"
  "usage:\n"
  "   methylateGenome input.[fa|2bit] outputPrefix\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void old(struct dnaSeq *seq)
{
int end = seq->size - 1;
int i;
for (i = 0; i < end; ++i)
    {
    char b1 = seq->dna[i];
    int v1 = ntVal[(unsigned)b1];
    int v2 = ntVal[(unsigned)seq->dna[i+1]];
    
    if (v1 == C_BASE_VAL && v2 == G_BASE_VAL)
        {
        if (b1 == 'C')
            seq->dna[i] = 'T';
        else
            seq->dna[i] = 't';
        }
    }
}

void forwardStrandNoMethylation(struct dnaSeq *seq)
{
int end = seq->size - 1;
int i;
for (i = 0; i < end; ++i)
    {
    if (seq->dna[i] == 'C')
        seq->dna[i] = 'T';
    else if (seq->dna[i] == 'c')
        seq->dna[i] = 't';
    }
}

void reverseStrandNoMethylation(struct dnaSeq *seq)
{
int end = seq->size - 1;
int i;
for (i = 0; i < end; ++i)
    {
    if (seq->dna[i] == 'G')
        seq->dna[i] = 'A';
    else if (seq->dna[i] == 'g')
        seq->dna[i] = 'a';
    }
}

void forwardStrandWithMethylation(struct dnaSeq *seq)
{
int end = seq->size - 1;
int i;
for (i = 0; i < end; ++i)
    {
    char b1 = seq->dna[i];
    int v1 = ntVal[(unsigned)b1];
    int v2 = ntVal[(unsigned)seq->dna[i+1]];
    
    if (v1 == C_BASE_VAL && v2 != G_BASE_VAL)
        {
            if (b1 == 'C')
                seq->dna[i] = 'T';
            else if (b1 == 'c')
                seq->dna[i] = 't';
        }
    }
}

void reverseStrandWithMethylation(struct dnaSeq *seq)
{
int end = seq->size - 1;
int i;
for (i = 0; i < end; ++i)
    {
    char b1 = seq->dna[i];
    int v1 = ntVal[(unsigned)b1];
    int v2 = ntVal[(unsigned)seq->dna[i+1]];
    
    if (v1 == G_BASE_VAL && v2 != C_BASE_VAL)
        {
            if (b1 == 'G')
                seq->dna[i] = 'A';
            else if (b1 == 'g')
                seq->dna[i] = 'a';
        }
    }
}

void performConversion(char *fileName, char *outputName, void (*replacementPolicy)(struct dnaSeq *))
{
// Open input and output files
struct dnaLoad *dl = dnaLoadOpen(fileName);
FILE *f = mustOpen(outputName, "w");

// Loop over every line in the input file...
struct dnaSeq *seq = NULL;
for (;;)
    {
    // Take every line in the file, break when we're done
    seq = dnaLoadNext(dl);
    if (seq == NULL)
        break;
      
    replacementPolicy(seq);
      
    // Replace all 'CG' (or 'cg') with 'TG' (or 'tg')
    /*int end = seq->size - 1;
    int i;
    for (i = 0; i < end; ++i)
        {
        char b1 = seq->dna[i];
        int v1 = ntVal[(unsigned)b1];
        int v2 = ntVal[(unsigned)seq->dna[i+1]];
        
        if (v1 == C_BASE_VAL && v2 == G_BASE_VAL)
            {
            if (b1 == 'C')
                seq->dna[i] = 'T';
            else
                seq->dna[i] = 't';
            }
        }*/
    
    // Write out the modified line
    faWriteNext(f, seq->name, seq->dna, seq->size);
    }
    
if (fclose(f) != 0)
    errnoAbort("fclose failed");
}

void methylateGenome(char *fileName, char *outputPrefix)
/* methylateGenome - Creates a methylated version of an input genome, in which any occurance of CG becomes TG. */
{
int nameSize = 64;

char forwardNoMethylName[nameSize];
safef(forwardNoMethylName, nameSize, "%s_forwardNoMethyl.fa", outputPrefix);
performConversion(fileName, forwardNoMethylName, &forwardStrandNoMethylation);

char reverseNoMethylName[nameSize];
safef(reverseNoMethylName, nameSize, "%s_reverseNoMethyl.fa", outputPrefix);
performConversion(fileName, reverseNoMethylName, &reverseStrandNoMethylation);

char forwardMethylName[nameSize];
safef(forwardMethylName, nameSize, "%s_forwardMethyl.fa", outputPrefix);
performConversion(fileName, forwardMethylName, &forwardStrandWithMethylation);

char reverseMethylName[nameSize];
safef(reverseMethylName, nameSize, "%s_reverseMethyl.fa", outputPrefix);
performConversion(fileName, reverseMethylName, &reverseStrandWithMethylation);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
dnaUtilOpen();
methylateGenome(argv[1], argv[2]);
return 0;
}
