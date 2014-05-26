/* aliFragScore - For all possible dna fragments up to n long, figure out score of all 
 * places they hit in target.. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "axt.h"

int fragSize = 6;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "aliFragScore - For all possible dna fragments of certain size, figure out score of all \n"
  "places they hit in target.\n"
  "usage:\n"
  "   aliFragScore targetDna output\n"
  "Where targetDna can be a .fa file, a .nib file, a .2bit file, or a file containing\n"
  "names of these types of files\n"
  "options:\n"
  "   -fragSize=N - Controls size of fragment.  Default %d\n"
  , fragSize
  );
}

static struct optionSpec options[] = {
   {"fragSize", OPTION_INT},
   {NULL, 0},
};

void ixToDna(int ix, char *dna, int dnaSize)
/* Convert binary representation 2-bits-per-base to
 * ascii. */
{
int i;
for (i=dnaSize-1; i>=0; i -= 1)
    {
    dna[i] = valToNt[ix&3];
    ix >>= 2;
    }
}

struct scoredFrag
/* A structure that holds scores of a bunch of frags. */
    {
    struct scoredFrag *next;	/* Next in list. */
    int perfectCount;/* Number of perfect matches */
    int posCount;    /* Number of matches with positive total. */
    double total;    /* Total score of all hits to target dna. */
    double posTotal; /* Total score of all positive scoring hits. */
    char frag[1];    /* Sequence associated with fragment, dynamically allocated to right size */
    };

struct scoredFrag *makeFragsOfSize(int size)
/* Return list of scored frags, one for each sequence of
 * given size. */
{
struct scoredFrag *list = NULL, *frag;
int maxIx = (1 << (size+size));
int ix;

for (ix = 0; ix<maxIx; ++ix)
    {
    frag = needMem(sizeof(struct scoredFrag) + size);
    ixToDna(ix, frag->frag, size);
    slAddHead(&list, frag);
    }
slReverse(&list);
return list;
}

boolean allReal(DNA *dna, int size)
/* Return TRUE if sequence is all acgt. */
{
int i;
for (i=0; i<size; ++i)
    if (ntVal[dna[i]] < 0)
        return FALSE;
return TRUE;
}

void scanSeq(struct axtScoreScheme *ss, struct dnaSeq *seq, 
	struct scoredFrag *fragList, int fragSize)
/* Scan sequence, updating scores and counts in fragList. */
{
DNA *dna = seq->dna;
int seqEnd = seq->size - fragSize;
int seqIx;

for (seqIx=0; seqIx <= seqEnd; ++seqIx)
    {
    if (allReal(dna, fragSize))
        {
	struct scoredFrag *frag;
	for (frag = fragList; frag != NULL; frag = frag->next)
	    {
	    double score = axtScoreUngapped(ss, frag->frag, dna, fragSize);
	    frag->total += score;
	    if (score > 0)
	        {
		frag->posCount += 1;
		frag->posTotal += score;
		if (memcmp(frag->frag, dna, fragSize) == 0)
		   frag->perfectCount += 1;
		}
	    }
	}
    ++dna;
    }
}

void aliFragScore(char *targetDna, char *output)
/* aliFragScore - For all possible dna fragments up to n long, figure out score of all 
 * places they hit in target.. */
{
struct dnaLoad *dl = dnaLoadOpen(targetDna);
struct dnaSeq *seq;
unsigned long totalBases = 0;
struct axtScoreScheme *ss = axtScoreSchemeDefault();
struct scoredFrag *frag, *fragList = makeFragsOfSize(fragSize);
FILE *f = mustOpen(output, "w");
uglyf("Got %d frags of size %d\n", slCount(fragList), fragSize);
while ((seq = dnaLoadNext(dl)) != NULL)
    {
    verbose(1, "Scanning %s of %d bases\n", seq->name, seq->size);
    toLowerN(seq->dna, seq->size);
    scanSeq(ss, seq, fragList, fragSize);
    totalBases += seq->size;
    dnaSeqFree(&seq);
    }
dnaLoadClose(&dl);

verbose(1, "%lu total bases\n", totalBases);
for (frag = fragList; frag != NULL; frag = frag->next)
    {
    fprintf(f, "%s\t%d\t%d\t%f\t%f\n", frag->frag, frag->perfectCount, frag->posCount,
    	frag->posTotal, frag->total);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionInit(&argc, argv, options);
fragSize = optionInt("fragSize", fragSize);
if (fragSize < 1 || fragSize > 10)
    errAbort("The fragSize option needs to be between 1 and 10");
if (argc != 3)
    usage();
aliFragScore(argv[1], argv[2]);
return 0;
}
