/* countNib - count ACGT. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dnaseq.h"
#include "nib.h"


void countNib(char *nibFile)
/* countNib - Count ACGT. */
{
struct dnaSeq *seq;
char *ptr;
int i = 0;
int a = 0;
int c = 0;
int g = 0;
int t = 0;

seq = nibLoadAll(nibFile);
ptr = seq->dna;
for (i=0; i<seq->size; i++)
    {
    if (ptr[i] == 'a') a++;
    else if (ptr[i] == 'c') c++;
    else if (ptr[i] == 'g') g++;
    else if (ptr[i] == 't') t++;
    }

printf("a = %d\n", a);
printf("c = %d\n", c);
printf("g = %d\n", g);
printf("t = %d\n", t);
dnaSeqFree(&seq);  
}

int main(int argc, char *argv[])
/* Process command line. */
{
countNib(argv[1]);
return 0;
}
