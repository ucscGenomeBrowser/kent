/* axtChain - Chain together axt alignments.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "axt.h"
#include "boxClump.h"
#include "chainBlock.h"
#include "portable.h"

int minScore = 400;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtChain - Chain together axt alignments.\n"
  "usage:\n"
  "   axtChain in.axt tNibDir qNibDir out.chain\n"
  "options:\n"
  "   -minScore=N  Minimum score for chain, default 400\n"
  );
}

struct seqPair
/* Pair of sequences. */
    {
    struct seqPair *next;
    char *name;	                /* Allocated in hash */
    char *qName;		/* Name of query sequence. */
    char *tName;		/* Name of target sequence. */
    char qStrand;		/* Strand of query sequence. */
    struct boxIn *blockList; /* List of alignments. */
    int axtCount;		/* Count of axt's that make this up (just for stats) */
    };

void addAxtBlocks(struct boxIn **pList, struct axt *axt)
/* Add blocks (gapless subalignments) from axt to block list. */
{
boolean thisIn, lastIn = FALSE;
int qPos = axt->qStart, tPos = axt->tStart;
int qStart = 0, tStart = 0;
int i;

for (i=0; i<=axt->symCount; ++i)
    {
    int advanceQ = (isalpha(axt->qSym[i]) ? 1 : 0);
    int advanceT = (isalpha(axt->tSym[i]) ? 1 : 0);
    thisIn = (advanceQ && advanceT);
    if (thisIn)
        {
	if (!lastIn)
	    {
	    qStart = qPos;
	    tStart = tPos;
	    }
	}
    else
        {
	if (lastIn)
	    {
	    int size = qPos - qStart;
	    assert(size == tPos - tStart);
	    if (size > 0)
	        {
		struct boxIn *b;
		AllocVar(b);
		b->qStart = qStart;
		b->qEnd = qPos;
		b->tStart = tStart;
		b->tEnd = tPos;
		slAddHead(pList, b);
		}
	    }
	}
    lastIn = thisIn;
    qPos += advanceQ;
    tPos += advanceT;
    }
}

void loadIfNewSeq(char *nibDir, char *newName, char strand, 
	char **pName, struct dnaSeq **pSeq, char *pStrand)
/* Load sequence unless it is already loaded.  Reverse complement
 * if necessary. */
{
struct dnaSeq *seq;
if (sameString(newName, *pName))
    {
    if (strand != *pStrand)
        {
	seq = *pSeq;
	reverseComplement(seq->dna, seq->size);
	*pStrand = strand;
	}
    }
else
    {
    char fileName[512];
    freeDnaSeq(pSeq);
    snprintf(fileName, sizeof(fileName), "%s/%s.nib", nibDir, newName);
    *pName = newName;
    *pSeq = seq = nibLoadAllMasked(NIB_MASK_MIXED, fileName);
    *pStrand = strand;
    if (strand == '-')
        reverseComplement(seq->dna, seq->size);
    uglyf("Loaded %d bases in %s\n", seq->size, fileName);
    }
}

int boxInCmpBoth(const void *va, const void *vb)
/* Compare to sort based on query, then target. */
{
const struct boxIn *a = *((struct boxIn **)va);
const struct boxIn *b = *((struct boxIn **)vb);
int dif;
dif = a->qStart - b->qStart;
if (dif == 0)
    dif = a->tStart - b->tStart;
return dif;
}

void removeExactOverlaps(struct boxIn **pBoxList)
/* Remove from list blocks that start in exactly the same
 * place on both coordinates. */
{
struct boxIn *newList = NULL, *b, *next, *last = NULL;
slSort(pBoxList, boxInCmpBoth);
for (b = *pBoxList; b != NULL; b = next)
    {
    next = b->next;
    if (last != NULL && b->qStart == last->qStart && b->tStart == last->tStart)
        {
	/* Fold this block into previous one. */
	if (last->qEnd < b->qEnd) last->qEnd = b->qEnd;
	if (last->tEnd < b->tEnd) last->tEnd = b->tEnd;
	freeMem(b);
	}
    else
	{
	slAddHead(&newList, b);
	last = b;
	}
    }
slReverse(&newList);
*pBoxList = newList;
}

struct scoreData
/* Data needed to score block. */
    {
    struct dnaSeq *qSeq;	/* Query sequence. */
    struct dnaSeq *tSeq;	/* Target sequence. */
    struct axtScoreScheme *ss;  /* Scoring scheme. */
    double gapPower;		/* Power to raise gap size to. */
    };
struct scoreData scoreData;

int connCount = 0;

int connectCost(struct boxIn *a, struct boxIn *b)
/* Calculate connection cost. */
{
int dq = b->qStart - a->qEnd;
int dt = b->tStart - a->tEnd;
int overlapPenalty = 0;
if (dq < 0 && dt < 0)
   {
   /* Life gets complex...  For now break. */
   dq = dt = 0;
   overlapPenalty = 1000000;
   }
++connCount;
// return scoreData.ss->gapOpen + (dt+dq-1)*scoreData.ss->gapExtend;
return 400 * pow(dt+dq, scoreData.gapPower);
}

void chainPair(struct seqPair *sp,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, FILE *f)
/* Chain up blocks and output. */
{
struct chain *chainList, *chain;
struct boxIn *b, *nextB;
long startTime, dt;

/* Set up info for connect function. */
scoreData.qSeq = qSeq;
scoreData.tSeq = tSeq;
scoreData.ss = axtScoreSchemeDefault();
scoreData.gapPower = 1.0/2.5;

uglyf("chainPair %s\n", sp->name);
/* Score blocks. */
for (b = sp->blockList; b != NULL; b = b->next)
    {
    int size = b->qEnd - b->qStart;
    b->score = axtScoreUngapped(scoreData.ss, 
    	qSeq->dna + b->qStart, tSeq->dna + b->tStart, size);
    }


startTime = clock1000();
chainList = chainBlocks(&sp->blockList, connectCost);
dt = clock1000() - startTime;
uglyf("Made %d chains in %5.3f s\n", slCount(chainList), dt*0.001);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    if (chain->score >= minScore)
	{
	fprintf(f, "chain %d %s %d + %d %d %s %d %c %d %d\n", chain->score,
	    sp->tName, tSeq->size, chain->tStart, chain->tEnd,
	    sp->qName, qSeq->size, sp->qStrand, chain->qStart, chain->qEnd);
	for (b = chain->blockList; b != NULL; b = nextB)
	    {
	    nextB = b->next;
	    fprintf(f, "%d", b->qEnd - b->qStart);
	    if (nextB != NULL)
		fprintf(f, "\t%d\t%d", nextB->tStart - b->tEnd, nextB->qStart - b->qEnd);
	    fputc('\n', f);
	    }
	fputc('\n', f);
	}
    }
chainFreeList(&chainList);
}

void axtChain(char *axtIn, char *tNibDir, char *qNibDir, char *chainOut)
/* axtChain - Chain together axt alignments.. */
{
struct hash *pairHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct dyString *dy = newDyString(512);
struct lineFile *lf = lineFileOpen(axtIn, TRUE);
struct axt *axt;
struct seqPair *spList = NULL, *sp;
FILE *f = mustOpen(chainOut, "w");
char *qName = "",  *tName = "";
struct dnaSeq *qSeq = NULL, *tSeq = NULL;
char qStrand = 0, tStrand = 0;

/* Read input file and divide alignments into various parts. */
while ((axt = axtRead(lf)) != NULL)
    {
    struct boxIn *block;
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", axt->qName, axt->qStrand, axt->tName);
    sp = hashFindVal(pairHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pairHash, dy->string, sp, &sp->name);
	sp->qName = cloneString(axt->qName);
	sp->tName = cloneString(axt->tName);
	sp->qStrand = axt->qStrand;
	}
    addAxtBlocks(&sp->blockList, axt);
    sp->axtCount += 1;
    axtFree(&axt);
    }
for (sp = spList; sp != NULL; sp = sp->next)
    {
    slReverse(&sp->blockList);
    uglyf("Got %d blocks, %d axts in %s\n", slCount(sp->blockList), sp->axtCount, sp->name);
    removeExactOverlaps(&sp->blockList);
    uglyf("%d blocks after duplicate removal\n", slCount(sp->blockList));
    loadIfNewSeq(qNibDir, sp->qName, sp->qStrand, &qName, &qSeq, &qStrand);
    loadIfNewSeq(tNibDir, sp->tName, '+', &tName, &tSeq, &tStrand);
    chainPair(sp, qSeq, tSeq, f);
    }
dyStringFree(&dy);
uglyf("ConnCount = %d\n", connCount);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
minScore = optionInt("minScore", minScore);
dnaUtilOpen();
if (argc != 5)
    usage();
axtChain(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
