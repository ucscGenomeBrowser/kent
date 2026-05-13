/* chainToMaf - Convert from chain to maf format. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "chain.h"
#include "nibTwo.h"
#include "axt.h"
#include "chainToAxt.h"
#include "maf.h"

#undef BIGNUM
#define BIGNUM 0x7fffffff

int maxGap = 100;
double minIdRatio = 0.0;
double minScore = 0;
char *qPrefix = "";
char *tPrefix = "";
boolean rescore = FALSE;
boolean rescoreZero = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainToMaf - Convert from chain to maf format\n"
  "usage:\n"
  "   chainToMaf in.chain tNibDirOr2bit qNibDirOr2bit out.maf\n"
  "options:\n"
  "   -maxGap=N      maximum gap allowed without breaking, default %d\n"
  "   -minScore=N    minimum chain score\n"
  "   -minId=N       minimum percentage ID within blocks (0-100)\n"
  "   -qPrefix=XX.   prepend XX. to query sequence name in maf\n"
  "   -tPrefix=YY.   prepend YY. to target sequence name in maf\n"
  "   -score         recalculate score for each maf block\n"
  "   -scoreZero     recalculate score only when chain score is zero\n"
  , maxGap
  );
}

static struct optionSpec options[] = {
   {"maxGap", OPTION_INT},
   {"minScore", OPTION_FLOAT},
   {"minId", OPTION_FLOAT},
   {"qPrefix", OPTION_STRING},
   {"tPrefix", OPTION_STRING},
   {"score", OPTION_BOOLEAN},
   {"scoreZero", OPTION_BOOLEAN},
   {NULL, 0},
};

static struct dnaSeq *loadSeqStrand(struct nibTwoCache *ntc, char *seqName, int start, int end,
                                    char strand)
/* Load mixed case sequence, reverse-complemented if strand is '-'. */
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

static double axtIdRatio(struct axt *axt)
/* Return percentage ID ignoring indels. */
{
int match = 0, ali = 0;
int i, symCount = axt->symCount;
for (i = 0; i < symCount; ++i)
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
return (double)match / (double)ali;
}

static struct mafAli *axtToMafAli(struct axt *axt, int tSize, int qSize)
/* Build a mafAli from an axt. Steals qSym/tSym from axt. */
{
struct mafAli *ali;
struct mafComp *comp;
char srcName[256];

AllocVar(ali);
ali->score = axt->score;
if ((ali->score == 0 && rescoreZero) || rescore)
    ali->score = axtScoreDnaDefault(axt);

AllocVar(comp);
safef(srcName, sizeof(srcName), "%s%s", qPrefix, axt->qName);
comp->src = cloneString(srcName);
comp->srcSize = qSize;
comp->strand = axt->qStrand;
comp->start = axt->qStart;
comp->size = axt->qEnd - axt->qStart;
comp->text = axt->qSym;
axt->qSym = NULL;
slAddHead(&ali->components, comp);

AllocVar(comp);
safef(srcName, sizeof(srcName), "%s%s", tPrefix, axt->tName);
comp->src = cloneString(srcName);
comp->srcSize = tSize;
comp->strand = axt->tStrand;
comp->start = axt->tStart;
comp->size = axt->tEnd - axt->tStart;
comp->text = axt->tSym;
axt->tSym = NULL;
slAddHead(&ali->components, comp);

return ali;
}

static void doAChain(struct chain *chain, struct nibTwoCache *tSeqCache,
                     struct nibTwoCache *qSeqCache, FILE *f)
/* Convert one chain to one or more maf blocks. */
{
struct dnaSeq *qSeq = loadSeqStrand(qSeqCache, chain->qName, chain->qStart, chain->qEnd,
                                    chain->qStrand);
struct dnaSeq *tSeq = loadSeqStrand(tSeqCache, chain->tName, chain->tStart, chain->tEnd, '+');
struct axt *axtList = chainToAxt(chain, qSeq, chain->qStart, tSeq, chain->tStart, maxGap, BIGNUM);
struct axt *axt;

for (axt = axtList; axt != NULL; axt = axt->next)
    {
    if (minIdRatio > 0.0 && axtIdRatio(axt) < minIdRatio)
        continue;
    struct mafAli *ali = axtToMafAli(axt, chain->tSize, chain->qSize);
    mafWrite(f, ali);
    mafAliFree(&ali);
    }
axtFreeList(&axtList);
freeDnaSeq(&qSeq);
freeDnaSeq(&tSeq);
}

static void chainToMaf(char *inName, char *tNibDirOr2bit, char *qNibDirOr2bit, char *outName)
/* chainToMaf - Convert from chain to maf format. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
struct nibTwoCache *tSeqCache = nibTwoCacheNew(tNibDirOr2bit);
struct nibTwoCache *qSeqCache = nibTwoCacheNew(qNibDirOr2bit);
FILE *f = mustOpen(outName, "w");
struct chain *chain;

mafWriteStart(f, "blastz");
while ((chain = chainRead(lf)) != NULL)
    {
    if (chain->score >= minScore)
        doAChain(chain, tSeqCache, qSeqCache, f);
    chainFree(&chain);
    }
mafWriteEnd(f);
carefulClose(&f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
maxGap = optionInt("maxGap", maxGap);
minIdRatio = optionFloat("minId", 0.0) / 100.0;
minScore = optionFloat("minScore", minScore);
qPrefix = optionVal("qPrefix", qPrefix);
tPrefix = optionVal("tPrefix", tPrefix);
rescore = optionExists("score");
rescoreZero = optionExists("scoreZero");
if (argc != 5)
    usage();
chainToMaf(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
