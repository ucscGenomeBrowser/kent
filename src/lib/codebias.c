/* codebias.c - stuff for managing codons and codon bias. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "dnautil.h"
#include "hmmstats.h"
#include "codebias.h"



struct codonBias *codonLoadBias(char *fileName)
/* Create scaled log codon bias tables based on .cod file.  
 * You can freeMem it when you're done. */
{
struct codonBias *cb;
char line[1024];
int lineCount = 0;
char *words[128];
int wordCount;
int i = 0, j = 0;
int skip = 0;
boolean getMark0 = FALSE;
boolean getMark1 = FALSE;
FILE *f = mustOpen(fileName, "r");
int val;

AllocVar(cb);
while (fgets(line, sizeof(line), f) )
    {
    ++lineCount;
    if (skip)
        {
        skip -= 1;
        continue;
        }
    if (getMark1)
        {
        wordCount = chopLine(line, words);
        if (wordCount != 65)
            errAbort("Bad line %d of %s\n", lineCount, fileName);
        for (j=0; j<64; ++j)
            {
            val = atoi(words[j+1]);
            if (val == 0)
                cb->mark1[i][j] = scaledLog(1.0E-20);
            else
                cb->mark1[i][j] = scaledLog(0.001*val);
            }
        if ((i += 1) == 64)
            getMark1 = FALSE;
        }
    else if (getMark0)
        {
        wordCount = chopLine(line, words);
        if (wordCount != 64)
            errAbort("Bad line %d of %s\n", lineCount, fileName);
        for (j=0; j<64; ++j)
            {
            val = atoi(words[j]);
            if (val == 0)
                cb->mark0[j] = scaledLog(1.0E-20);
            else
                cb->mark0[j] = scaledLog(0.001*val);
            }
        getMark0 = FALSE;
        }
    else if (startsWith("Markov", line))
        {
        wordCount = chopLine(line, words);
        if (wordCount != 2)
            errAbort("Bad line %d of %s\n", lineCount, fileName);
        if (sameString(words[1], "0"))
            getMark0 = TRUE;
        else if (sameString(words[1], "1"))
            getMark1 = TRUE;
        else
            errAbort("Bad line %d of %s\n", lineCount, fileName);
        skip = 3;
        }
    }
fclose(f);
return cb;
}
   
static void unN(DNA *dna, int dnaSize)
/* Turn N's into T's. */
{
int i;
int val;
for (i=0; i<dnaSize; ++i)
    {
    if ((val = ntVal[(int)dna[i]]) < 0)
        dna[i] = 't';
    }
}

int codonFindFrame(DNA *dna, int dnaSize, struct codonBias *forBias)
/* Assuming a stretch of DNA is an exon, find most likely frame that it's in. 
 * Beware this routine will replace N's with T's in the input dna.*/
{
double logOneFourth = log(0.25);
double logProbs[3];
int frame;
int dnaIx;
double logP;
double bestLogP = -0x6fffffff;
int bestFrame = -1;
int lastDnaStart = dnaSize-3;
DNA *d;
int codon = 0, lastCodon; 

unN(dna, dnaSize);
for (frame=0; frame<3; ++frame)
    {
    /* Partial first codon just gets even background score. */
    logP = frame*logOneFourth;
    /* 1st order model on first full codon. */
    if (frame <= lastDnaStart)
        {
        d = dna+frame;
        codon = (ntVal[(int)d[0]]<<4) + (ntVal[(int)d[1]]<<2) + ntVal[(int)d[2]];
        logP += forBias->mark0[codon];
        }
    /* 2nd order model on subsequent full codons. */
    for (dnaIx = frame+3; dnaIx <= lastDnaStart; dnaIx += 3)
        {
        lastCodon = codon;
        d = dna+dnaIx;
        codon = (ntVal[(int)d[0]]<<4) + (ntVal[(int)d[1]]<<2) + ntVal[(int)d[2]];
        logP += forBias->mark1[lastCodon][codon];
        }
    /* Partial last codon gets even background score. */
    logP += (dnaSize-dnaIx)*logOneFourth;
    logProbs[frame] = logP;
    if (logP > bestLogP)
        {
        bestLogP = logP;
        bestFrame = frame;
        }
    }
return bestFrame;
}



