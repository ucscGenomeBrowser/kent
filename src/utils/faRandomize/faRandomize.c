/* faRandomize - Program to create random fasta records using
   same base frequency as seen in original fasta records. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "obscure.h"
#include "options.h"

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"seed", OPTION_INT},
    {NULL, 0}
};
    
void usage() 
/* Report usage and quit. */
{
errAbort("faRandomize - Program to create random fasta records\n"
    "usage:\n"
    "  faRandomize [-seed=N] in.fa randomized.fa\n"
    "    Use optional -seed argument to specify seed (integer) for random\n"
    "    number generator (rand).  Generated sequence has the\n"
    "    same base frequency as seen in original fasta records.");
}

struct dnaSeq *randomizedSequence(struct dnaSeq *seq)
/* Create a randomized version of the sequence in seq and
   return it. */
{
struct dnaSeq *randSeq = NULL;
int dnaCounts[4];
double dnaFreqs[4];
int i = 0, j = 0;;
double r = 0;
double last = 0;
/* Set up background to draw sequences from. */
dnaBaseHistogram(seq->dna, seq->size, dnaCounts);

/* Seed the randome number generator. */
if(optionExists("seed")) 
    srand(optionInt("seed",0));
else
    srand(time(NULL));

/* Generate sequence. */
for(i = 0; i < ArraySize(dnaCounts); i++) 
    {
    dnaFreqs[i] = last + (double) dnaCounts[i] / seq->size;
    last = dnaFreqs[i];
    }

/* Create randomized sequence. */
AllocVar(randSeq);
randSeq->name = cloneString(seq->name);
randSeq->size = seq->size;
randSeq->dna = cloneString(seq->dna);
for(i = 0; i < randSeq->size; i++) 
    {
    r = (double) rand() / RAND_MAX;
    /* r should be between 0 and 1 now. Frequencies
       are cumulative i.e. for T,C,A,G:
       T = 0 - .25
       C = 0 - .5
       A = 0 - .75
       G = 0 - 1
       so go up until r is less than cumulative fequency. */
    for(j = 0; j < ArraySize(dnaFreqs); j++) 
	{
	if(r <= dnaFreqs[j])
	    {
	    randSeq->dna[i] = valToNt[j];
	    break;
	    }
	}
    }
return randSeq;
}

void faRandomize(char *fileIn, char *fileOut)
/* Read in all of the fasta records from fileIn and write out
   randomized versions to fileOut. */
{
struct dnaSeq *seq = NULL, *seqList = NULL;
struct dnaSeq *randSeq = NULL;
FILE *randOut = NULL;
warn("Reading records from %s", fileIn);
seqList = faReadAllDna(fileIn);
dotForUserInit(max(1,slCount(seqList) / 10));
warn("Writing randomized records.");

dnaUtilOpen();
randOut = mustOpen(fileOut, "w");

/* Loop through and randomize each entry individually. */
for(seq = seqList; seq != NULL; seq = seq->next) 
    {
    dotForUser();
    randSeq = randomizedSequence(seq);
    faWriteNext(randOut, randSeq->name, randSeq->dna, randSeq->size);
    dnaSeqFree(&randSeq);
    }
carefulClose(&randOut);
dnaSeqFreeList(&seqList);
warn("\nDone.");
}

int main(int argc, char *argv[])
/* Everybody's favorite function. */
{
optionInit(&argc, argv, optionSpecs);
if(argc < 3)
    usage();
faRandomize(argv[1], argv[2]);
return 0;
}

