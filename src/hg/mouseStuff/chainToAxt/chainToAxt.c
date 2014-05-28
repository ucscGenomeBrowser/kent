/* chainToAxt - Convert from chain to axt file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "chain.h"
#include "nibTwo.h"
#include "axt.h"
#include "chainToAxt.h"


int maxGap = 100;
int maxChain = BIGNUM;
double minIdRatio = 0.0;
double minScore = 0;
boolean bedOut = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainToAxt - Convert from chain to axt file\n"
  "usage:\n"
  "   chainToAxt in.chain tNibDirOr2bit qNibDirOr2bit out.axt\n"
  "options:\n"
  "   -maxGap=maximum gap sized allowed without breaking, default %d\n"
  "   -maxChain=maximum chain size allowed without breaking, default %d\n"
  "   -minScore=minimum score of chain\n"
  "   -minId=minimum percentage ID within blocks\n"
  "   -bed  Output bed instead of axt\n"
  , maxGap , maxChain
  );
}

static struct optionSpec options[] = {
   {"maxGap", OPTION_INT},
   {"maxChain", OPTION_INT},
   {"minScore", OPTION_FLOAT},
   {"minId", OPTION_FLOAT},
   {"bed", OPTION_BOOLEAN},
   {NULL, 0},
};

struct dnaSeq *loadSeqStrand(struct nibTwoCache *ntc, char *seqName, int start, int end,
                             char strand)
/* Load in a mixed case sequence,  from reverse strand if
 * strand is '-'. */
{
struct dnaSeq *seq;
int size = end - start;
assert(size >= 0);
if (strand == '-')
    {
    reverseIntRange(&start, &end, nibTwoGetSize(ntc, seqName));
    seq = nibTwoCacheSeqPartExt(ntc, seqName, start, size, TRUE, NULL);
    reverseComplement(seq->dna, seq->size);
    }
else
    {
    seq = nibTwoCacheSeqPartExt(ntc, seqName, start, size, TRUE, NULL);
    }
return seq;
}

double axtIdRatio(struct axt *axt)
/* Return the percentage ID ignoring indels. */
{
int match = 0, ali = 0;
int i, symCount = axt->symCount;
for (i=0; i<symCount; ++i)
    {
    char q = toupper(axt->qSym[i]);
    char t = toupper(axt->tSym[i]);
    if (q != '-' && t != '-')
        {
	++ali;
	if (q == t)
	   ++match;
	}
    }
if (match == 0)
    return 0.0;
else
    return (double)match/(double)ali;
}

void bedWriteAxt(struct axt *axt, int qSize, int tSize, double idRatio, FILE *f)
/* Write out bounds of axt to a bed file. */
{
int idPpt = idRatio * 1000;
int qStart = axt->qStart, qEnd = axt->qEnd;
if (axt->qStrand == '-')
    reverseIntRange(&qStart, &qEnd, qSize);
fprintf(f, "%s\t%d\t%d\t", axt->tName, axt->tStart, axt->tEnd);
fprintf(f, "%s\t%d\t%c\n", axt->qName, idPpt, axt->qStrand);
}

static void doAChain(struct chain *chain, struct nibTwoCache *tSeqCache, struct nibTwoCache *qSeqCache,
                     FILE *f)
/* Convert one chain to an axt. */
{
struct dnaSeq *qSeq = loadSeqStrand(qSeqCache, chain->qName, chain->qStart, chain->qEnd, chain->qStrand);
struct dnaSeq *tSeq = loadSeqStrand(tSeqCache, chain->tName, chain->tStart, chain->tEnd, '+');
struct axt *axtList= chainToAxt(chain, qSeq, chain->qStart, tSeq, chain->tStart, maxGap, BIGNUM);
struct axt *axt = NULL;

for (axt = axtList; axt != NULL; axt = axt->next)
    {
    double idRatio = axtIdRatio(axt);
    if (minIdRatio <= idRatio)
        {
        if (bedOut)
            bedWriteAxt(axt, chain->qSize, chain->tSize, idRatio, f);
        else
            axtWrite(axt, f);
        }
    }
axtFreeList(&axtList);
freeDnaSeq(&qSeq);
freeDnaSeq(&tSeq);
}

void doIt(char *inName, char *tNibDirOr2bit, char *qNibDirOr2bit, char *outName)
/* chainToAxt - Convert from chain to axt file. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
struct nibTwoCache *tSeqCache = nibTwoCacheNew(tNibDirOr2bit);
struct nibTwoCache *qSeqCache = nibTwoCacheNew(qNibDirOr2bit);
struct chain *chain = NULL;
FILE *f = mustOpen(outName, "w");

while ((chain = chainRead(lf)) != NULL)
    {
    if (chain->score >= minScore)
        doAChain(chain, tSeqCache, qSeqCache, f);
    chainFree(&chain);
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
maxGap = optionInt("maxGap", maxGap);
maxChain = optionInt("maxChain", maxChain);
minIdRatio = optionFloat("minId", 0.0)/100.0;
minScore = optionFloat("minScore", minScore);
bedOut = optionExists("bed");
if (argc != 5)
    usage();
doIt(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
