/* chainAntiRepeat - Get rid of chains that are primarily the results of repeats and degenerate DNA. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nibTwo.h"
#include "chain.h"


int minScore = 5000;
int noCheckScore = 200000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainAntiRepeat - Get rid of chains that are primarily the results of repeats and degenerate DNA\n"
  "usage:\n"
  "   chainAntiRepeat tNibDir qNibDir inChain outChain\n"
  "options:\n"
  "   -minScore=N - minimum score (after repeat stuff) to pass\n"
  "   -noCheckScore=N - score that will pass without checks (speed tweak)\n"
  );
}

static struct optionSpec options[] = {
   {"minScore", OPTION_INT},
   {"noCheckScore", OPTION_INT},
   {NULL, 0},
};

static int isLowerDna[256];

boolean degeneracyFilter(struct dnaSeq *tSeq, struct dnaSeq *qSeq, struct chain *chain)
/* Returns FALSE if matches seem to be degenerate mostly. */
{
struct cBlock *b;
int countBuf[5], *counts = countBuf+1;
int totalMatches = 0;
int sum2, best2 = 0;
double okBest2 = 0.80;
double observedBest2, overOk;
double maxOverOk = 1.0 - okBest2;

countBuf[0] = countBuf[1] = countBuf[2] = countBuf[3] = countBuf[4] = 0;
/* Count up number of each nucleotide that is in a match. */
for (b = chain->blockList; b != NULL; b = b->next)
    {
    DNA *q = qSeq->dna + b->qStart - chain->qStart;
    DNA *t = tSeq->dna + b->tStart - chain->tStart;
    int size = b->tEnd - b->tStart;
    int i;
    for (i=0; i<size; ++i)
        {
	int qb = ntVal[(int)q[i]];
	if (qb == ntVal[(int)t[i]])
	    counts[qb] += 1;
	}
    }
totalMatches = counts[0] + counts[1] + counts[2] + counts[3];
best2 = counts[0] + counts[1];
sum2 = counts[0] + counts[2];
if (best2 < sum2) best2 = sum2;
sum2 = counts[0] + counts[3];
if (best2 < sum2) best2 = sum2;
sum2 = counts[1] + counts[2];
if (best2 < sum2) best2 = sum2;
sum2 = counts[1] + counts[3];
if (best2 < sum2) best2 = sum2;
sum2 = counts[2] + counts[3];
if (best2 < sum2) best2 = sum2;

/* We expect the best2 to sum to 60%.  If it sums to more than that
 * we start reducing the score proportionally, and return false if
 * it is less than minScore. */
observedBest2 = (double)best2/(double)totalMatches;
overOk = observedBest2 - okBest2;
if (overOk <= 0)
    return TRUE;
else
    {
    double adjustFactor = 1.01 - overOk/maxOverOk;
    double adjustedScore = chain->score * adjustFactor;
    return adjustedScore >= minScore;
    }
}

boolean repeatFilter(struct dnaSeq *tSeq, struct dnaSeq *qSeq, struct chain *chain)
/* Returns FALSE if matches seem to be almost entirely repeat-driven. */
{
struct cBlock *b;
int repCount = 0, total=0;
double adjustedScore;

/* Count up number of each nucleotide that is in a match. */
for (b = chain->blockList; b != NULL; b = b->next)
    {
    DNA *q = qSeq->dna + b->qStart - chain->qStart;
    DNA *t = tSeq->dna + b->tStart - chain->tStart;
    int size = b->tEnd - b->tStart;
    int i;
    for (i=0; i<size; ++i)
        {
	if (isLowerDna[(int)q[i]] || isLowerDna[(int)t[i]])
	    ++repCount;
	}
    total += size;
    }
adjustedScore = (chain->score * 2.0 * (total - repCount) / total);
return adjustedScore >= minScore;
}

void chainAntiRepeat(char *tNibDir, char *qNibDir, 
	char *inName, char *outName)
/* chainAntiRepeat - Get rid of chains that are primarily the results 
 * of repeats and degenerate DNA. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct chain *chain;
struct nibTwoCache *qc, *tc;
lineFileSetMetaDataOutput(lf, f);

isLowerDna['a'] = isLowerDna['c'] = isLowerDna['g'] = isLowerDna['t'] = 
	isLowerDna['n'] = TRUE;
tc = nibTwoCacheNew(tNibDir);
qc = nibTwoCacheNew(qNibDir);

while ((chain = chainRead(lf)) != NULL)
    {
    boolean pass = TRUE;
    if (chain->score < noCheckScore)
        {
	struct dnaSeq *tSeq = nibTwoCacheSeqPart(tc, chain->tName,
		chain->tStart, chain->tEnd - chain->tStart, NULL);
	struct dnaSeq *qSeq;
	int qFragSize = chain->qEnd - chain->qStart;
	if (chain->qStrand == '-')
	    {
	    int qStart;
	    qStart = chain->qSize - chain->qEnd;
	    qSeq = nibTwoCacheSeqPart(qc, chain->qName, qStart, qFragSize, NULL);
	    reverseComplement(qSeq->dna, qFragSize);
	    }
	else
	    {
	    qSeq = nibTwoCacheSeqPart(qc, chain->qName, chain->qStart, 
	    	qFragSize, NULL);
	    }
	pass = degeneracyFilter(tSeq, qSeq, chain);
	if (pass)
	    pass = repeatFilter(tSeq, qSeq, chain);
	dnaSeqFree(&qSeq);
	dnaSeqFree(&tSeq);
	}
    if (pass)
	{
        chainWrite(chain, f);
	}
    chainFree(&chain);
    }

/* Clean up time. */
nibTwoCacheFree(&tc);
nibTwoCacheFree(&qc);
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
minScore = optionInt("minScore", minScore);
noCheckScore = optionInt("noCheckScore", noCheckScore);
chainAntiRepeat(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
