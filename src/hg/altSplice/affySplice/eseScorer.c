/* eseScorer.c - Program to score a set of sequences
   for different exonic splicing motifs. */
#include "common.h"
#include "dnaseq.h"
#include "fa.h"
#include "obscure.h"
#include "options.h"
#include "linefile.h"

void usage()
/* Print help and quit. */
{
errAbort("eseScorer - Program to score a set of sequences\n"
	 "for different exonic splicing motifs. Takes as input\n"
	 "a list of motifs and a fasta file with sequences.\n"
	 "Outputs counts above threshold for each motifs.\n"
	 "usage:\n   "
	 "eseScorer motifFile.score sequenceFile.fa resultsOut.tab");
}

struct motif 
/* Data to encapsulate a motif, basically
   a float matrix with a name and threshold. */
{
    struct motif *next; /* Next in list. */
    char *name;         /* Name of motif. */
    int size;           /* Size in bp. */
    double threshold;  /* Threshold at which motif is a hit. */
    double **mat;       /* Matrix bases (rows) by size (cols) of
			 * motif. Indexed as A=0, C=1, G=2, T=3 in the
			 * rows. */
};

char baseForNum(int num) 
/* Return the base for this index into the
   motif mat matrix. A=0, C=1, G=2, T=3 */
{
switch (num) 
    {
    case 0 :
	return 'a';
    case 1 :
	return 'c';
    case 2 :
	return 'g';
    case 3 :
	return 't';
    default :
	errAbort("Unknown index %d for base", num);
    }
return 'z';
}

int numForBase(char base) 
/* Return the index for this base into the
   motif mat matrix. A=0, C=1, G=2, T=3 */
{
switch (base) 
    {
    case 'a' :
	return 0;
    case 'A' :
	return 0;
    case 'c' :
	return 1;
    case 'C' :
	return 1;
    case 'g' :
	return 2;
    case 'G' :
	return 2;
    case 't' :
	return 3;
    case 'T' :
	return 3;
    default:
	errAbort("Unknown base: %c", base);
    }
return 0;
}

struct motif *readMotifs(char *file)
/* Read all of the motifs out of a file. Expecting format like:
   motif: name threshold
   A 	-1.14 	0.62 	-1.58 	1.32 	-1.58 	-1.58 	0.62
   C 	1.37 	-1.1 	0.73 	0.33 	0.94 	-1.58 	-1.58
   G 	-0.21 	0.17 	0.48 	-1.58 	0.33 	0.99 	-0.11
   T 	-1.58 	-0.5 	-1.58 	-1.13 	-1.58 	-1.13 	0.27   
*/
{
struct lineFile *lf = lineFileOpen(file, TRUE);
char *words[128];
char *tmp = NULL;
struct motif *mList = NULL, *m = NULL;
int size = 0;
int i = 0; 
while(lineFileChopNext(lf, words, ArraySize(words)) > 2) 
    {
    AllocVar(m);
    if(differentWord(words[0], "motif:")) 
	errAbort("Expecting: 'motif:' - Got '%s'. Aborting", words[0]);
    m->name = cloneString(words[1]);
    m->threshold = atof(words[2]);

    /* Read in first row of motif matrix. */
    m->size = lineFileChopNext(lf, words, ArraySize(words)) - 1;
    if(differentWord(words[0], "A"))
	errAbort("Expecting 'A' at start of line, got '%s'. Aborting", words[0]);
    /* Allocate matrix. */
    AllocArray(m->mat, 4);
    for(i = 0; i < 4; i++) 
	AllocArray(m->mat[i], m->size);
    
    /* copy in A's. */
    for(i = 0; i < m->size; i++) 
	m->mat[0][i] = atof(words[i+1]);
    
    /* Read in C's. */
    size = lineFileChopNext(lf, words, ArraySize(words)) - 1;
    if(differentWord(words[0], "C") || size != m->size)
	errAbort("Expecting 'c' at start of line, got '%s' (size off?). Aborting", words[0]);
    for(i = 0; i < size; i++) 
	m->mat[1][i] = atof(words[i+1]);

    /* Read in G's. */
    size = lineFileChopNext(lf, words, ArraySize(words)) - 1;
    if(differentWord(words[0], "G") || size != m->size)
	errAbort("Expecting 'g' at start of line, got '%s' (size off?). Aborting", words[0]);
    for(i = 0; i < size; i++) 
	m->mat[2][i] = atof(words[i+1]);

    /* Read in T's. */
    size = lineFileChopNext(lf, words, ArraySize(words)) - 1;
    if(differentWord(words[0], "T") || size != m->size)
	errAbort("Expecting 't' at start of line, got '%s' (size off?). Aborting", words[0]);
    for(i = 0; i < size; i++) 
	m->mat[3][i] = atof(words[i+1]);
    slAddHead(&mList, m);
    }
lineFileClose(&lf);
slReverse(&mList);
return mList;
}

double scoreForMotif(struct motif *m, char *seq)
/* Return the score for the motif on sequence seq. */
{
int i = 0;
int baseIx = 0;
double score = 0;
for(i = 0; i < m->size; i++)
    {
    char base = seq[i];
    assert(base);
    if(base != 'n' && base != 'N') 
	{
	baseIx = numForBase(base);
	score += m->mat[baseIx][i];
	}
    }

return score;
}

void analyzeSequence(struct dnaSeq *seq, struct motif *mList, FILE *out)
/* Look for each of the motifs in the sequence provided
   write the output to out. */
{
struct motif *motif = NULL;
double score = 0;
fprintf(out, "%s\t%d", seq->name, seq->size);
for(motif = mList; motif != NULL; motif = motif->next) 
    {
    int i = 0;
    int hitCount = 0;
    for(i = 0; i < seq->size - motif->size; i++) 
	{
	score = scoreForMotif(motif, seq->dna+i);
	if(score >=  motif->threshold) 
	    {
/* 	    fprintf(stderr, "%s\t%d\t%.6f\n", motif->name, i, score); */
	    hitCount++;
	    }
	}
    fprintf(out, "\t%d", hitCount);
    }
fprintf(out, "\n");
}

void eseScorer(char *motifFile, char *faFile, char *outFile)
/* High level function to open files and set up run. */
{
struct motif *mList = NULL, *m = NULL;
struct dnaSeq *seqList = NULL, *seq = NULL;
FILE *out = NULL;
mList = readMotifs(motifFile);
warn("Read %d motifs from %s.", slCount(mList), motifFile);
seqList = faReadAllDna(faFile);
warn("Read %d sequences from %s.", slCount(seqList), faFile);
out = mustOpen(outFile, "w");

/* Print out header for output file. */
fprintf(out, "size");
for(m = mList; m != NULL; m = m->next) 
    fprintf(out, "\t%s", m->name);
fprintf(out, "\n");

/* Score sequences. */
dotForUserInit(max(1,slCount(seqList) / 10));
for(seq = seqList; seq != NULL; seq = seq->next) 
    {
    dotForUser();
    analyzeSequence(seq, mList, out);
    }

/* Clean up. */
carefulClose(&out);
dnaSeqFreeList(&seqList);
warn("\nDone.\n");
}

int main(int argc, char *argv[]) 
/* Everybodys favorite function. */
{
if(argc != 4)
    usage();
eseScorer(argv[1], argv[2], argv[3]);
return 0;
}
