/* testLiftRechainer - Work out which chains to use for lift-over purposes using dynamic 
 * programming rather than greedy fill-in-the-gaps method of nets.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dlist.h"
#include "options.h"
#include "sqlNum.h"
#include "chain.h"
#include "twoBit.h"
#include "obscure.h"
#include "dnaseq.h"

boolean noAlt = FALSE, sameChrom = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testLiftRechainer - Work out which chains to use for lift-over purposes using dynamic \n"
  "programming rather than greedy fill-in-the-gaps method of nets.\n"
  "usage:\n"
  "   testLiftRechainer in.chain target.2bit query.2bit out.chain\n"
  "options:\n"
  "   -noAlt - don't include chains involving _alt or _fix chromosome fragments\n"
  "   -sameChrom - only include chains from same chromosome\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"noAlt", OPTION_BOOLEAN},
   {"sameChrom", OPTION_BOOLEAN},
   {NULL, 0},
};

struct chainTarget
/* All the chains associated with a target chromosome. */
    {
    struct chainTarget *next;
    char *name;	    /* Target sequence name */
    struct chain *chainList;	    /* Sorted by tStart */
    struct chain *bigChain;	    /* Biggest scoring chain on list */
    };

boolean isAltName(char *name)
/* Return true if ends with _alt or _fix */
{
return endsWith(name, "_alt") || endsWith(name, "_fix");
}

struct chainTarget *readChainTargets(char *fileName, struct hash *tHash, struct hash *qHash)
/* Return list of chainTargets read from file.  Returned hash will be keyed by
 * target name */
{
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct chainTarget *targetList = NULL, *target;

struct chain *chain;
while ((chain = chainRead(lf)) != NULL)
    {
    if (noAlt && (isAltName(chain->qName) || isAltName(chain->tName)))
        continue;
    char *tName = chain->tName;
    target = hashFindVal(hash, tName);
    if (target == NULL)
         {
	 if (hashLookup(tHash, tName) == NULL)
	     errAbort("Target sequence %s is in chain but not target.2bit", tName);
	 AllocVar(target);
	 target->name = cloneString(tName);
	 hashAdd(hash, tName, target);
	 slAddHead(&targetList, target);
	 }
    if (hashLookup(qHash, chain->qName) == NULL)
	 errAbort("Query sequence %s is in chain but not target.2bit", chain->qName);
    slAddHead(&target->chainList, chain);
    if (target->bigChain == NULL || chain->score > target->bigChain->score)
        target->bigChain = chain;
    }

/* Sort chains within all targets */
for (target = targetList; target != NULL; target = target->next)
    slSort(&target->chainList, chainCmpTarget);

/* Clean up and return */
hashFree(&hash);
lineFileClose(&lf);
slReverse(&targetList);
return targetList;
}

struct crossover
/* List of positions where one chain flips to another */
    {
    struct crossover *next;
    int tPos;	  /* Position in target where crossover occurs */
    struct rechain *rechain;   /* Rechain at this point */
    };

struct crossover *crossoverNew(int tPos, struct rechain *rechain)
/* Allocate and fill in a new crossover */
{
struct crossover *cross;
AllocVar(cross);
cross->tPos= tPos;
cross->rechain = rechain;
return cross;
}


struct rechain
/* An active chain.  Lives on double-linked list */
    {
    int tStart, tEnd;	/* Position covered in target */
    struct chain *chain;    /* Associated full chain */
    struct cBlock *curBlock;  /* Current block */
    struct dlNode *node;    /* dlNode associated with chain */
    DNA *tDna;		    /* Target DNA seq */
    DNA *qDna;		    /* Query DNA seq */
    long long score;	    /* Current score */
    long long lastScore;    /* Score up to last position */
    struct crossover *crossList;   /* List of crossings around chain */
    };

struct rechain *rechainZero(int tSize, struct dlList *list)
/* Make up a psuedo-chain that corresponds to no alignment to target */
{
struct rechain *rechain;
AllocVar(rechain);
rechain->tEnd = tSize;
rechain->node = dlAddValTail(list, rechain);
rechain->crossList = crossoverNew(0, rechain);
return rechain;
}

const long long newChainPenalty = 500;

int chainBlockTotalSize(struct chain *chain)
/* Add up total size of all blocks */
{
int total = 0;
struct cBlock *block;
for (block = chain->blockList; block != NULL; block = block->next)
    total += block->tEnd - block->tStart;
return total;
}

boolean chainOverMinScore(struct chain *chain, struct dnaSeq *tSeq, struct dnaSeq *qSeq, int minScore)
/* Add up total size of all blocks */
{
int total = 0;
DNA *tDna = tSeq->dna, *qDna = qSeq->dna;

struct cBlock *block;
for (block = chain->blockList; block != NULL; block = block->next)
    {
    DNA *t = tDna + block->tStart;
    DNA *q = qDna + block->qStart;
    int blockSize = block->tEnd - block->tStart;
    int i;
    for (i=0; i<blockSize; ++i)
        {
	if (t[i] == q[i])
	   total += 1;
	else
	   total -= 1;
	}
    if (total > minScore)
        return TRUE;
    }
return FALSE;
}

void addNewRechain(struct chain *chain, struct dnaSeq *tSeq, struct hash *qSeqHash,
    struct hash *qRcSeqHash, struct rechain *bestExisting, long long bestExistingScore,
    struct dlList *list)
/* Make up a real rechainer and put it on list */
{
/* First do some optimizations to see if chain could possibly help */
int minScore = newChainPenalty;
if (bestExisting->tEnd > chain->tEnd)
    minScore += newChainPenalty;  // Will go back to old chain we assume

if (chain->tEnd - chain->tStart <= minScore)
    return;
if (chainBlockTotalSize(chain) <= minScore)
    return;

/* Get query sequence */
struct dnaSeq *qSeq = NULL;
if (chain->qStrand == '+')
    {
    qSeq = hashMustFindVal(qSeqHash, chain->qName);
    }
else
    {
    char *qName = chain->qName;
    qSeq = hashFindVal(qRcSeqHash, qName);
    if (qSeq == NULL)
        {
	qSeq = hashMustFindVal(qSeqHash, chain->qName);
	qSeq = cloneDnaSeq(qSeq);
	reverseComplement(qSeq->dna, qSeq->size);
	hashAdd(qRcSeqHash, qName, qSeq);
	}
    }

/* Do more chain scoreing */
if (!chainOverMinScore(chain, tSeq, qSeq, minScore))
    return;

struct rechain *rechain;
AllocVar(rechain);
rechain->tStart = chain->tStart;
rechain->tEnd = chain->tEnd;
rechain->tStart = chain->tStart;
rechain->tEnd = chain->tEnd;
rechain->chain = chain;
rechain->curBlock = chain->blockList;
rechain->tDna = tSeq->dna;
rechain->qDna = qSeq->dna;
rechain->node = dlAddValTail(list, rechain);
rechain->crossList = crossoverNew(chain->tStart, bestExisting);
rechain->score = bestExistingScore - newChainPenalty;
}

int rechainId(struct rechain *rechain)
/* Return ID associated with rechain */
{
struct chain *chain = rechain->chain;
if (chain == NULL)
    return 0;
else
    return chain->id;
}

struct chainPart
/* A part of a chain */
    {
    struct chainPart *next;
    struct chain *chain;
    int tStart, tEnd;
    };

#ifdef DEBUG
void showBacks(struct dlList *list, FILE *f)
/* Show backwards pointers of all chains on list */
{
struct dlNode *el;
for (el = list->head; !dlEnd(el); el = el->next)
    {
    struct rechain *rechain = el->val;
    fprintf(f, "backs for %d:\n", rechainId(rechain));
    struct crossover *cross;
    for (cross = rechain->crossList; cross != NULL; cross = cross->next)
       fprintf(f, "\t%d to %d\n", cross->tPos, rechainId(cross->rechain));
    }
}
#endif /* DEBUG */

void rechainOneTarget(struct chainTarget *target, struct dnaSeq *tSeq, struct hash *qSeqHash,
    struct hash *qRcSeqHash, FILE *f)
/* Do rechaining on a single target */
{
int tSize = tSeq->size;

/* Make rechain lists and put in the "no chain" on on the active list. */
struct dlList *archiveList = dlListNew();   /* List of rechains no longer active */
struct dlList *activeList = dlListNew();    /* List of active rechains. */
rechainZero(tSize, activeList);

struct chain *chain = target->chainList;
int tPos;
for (tPos=0; tPos < tSize; ++tPos)
    {
    /* Go through active list removing chains that we are past */
    DNA tBase = tSeq->dna[tPos];
    struct dlNode *el, *next;
    for (el = activeList->head; !dlEnd(el); el = next)
	{
	next = el->next;
	struct rechain *rechain = el->val;
	if (tPos >= rechain->tEnd)
	    {
	    dlRemove(el);
	    dlAddTail(archiveList, el);
	    }
	}

    /* Add new chains */
    struct rechain *bestChain = NULL;
    long long bestScore = -BIGNUM;
    /* Add new chains to active list */
    while (chain != NULL && chain->tStart == tPos)
	{
	if (!sameChrom || sameString(chain->qName, target->bigChain->qName))
	    {
	    if (bestChain == NULL)
		{
		struct dlNode *el;
		for (el = activeList->head; !dlEnd(el); el = el->next)
		    {
		    struct rechain *rechain = el->val;
		    if (rechain->score > bestScore)
			{
			bestScore = rechain->score;
			bestChain = rechain;
			}
		    }
		}

	    addNewRechain(chain, tSeq, qSeqHash, qRcSeqHash, bestChain, bestScore, activeList);
	    }
	chain = chain->next;
	}

    /* Go through active list updating scores */
    for (el = activeList->head; !dlEnd(el); el = el->next)
        {
	/* Get rechain structure and point block to relevant one for current tPos */
	struct rechain *rechain = el->val;
	struct cBlock *block = rechain->curBlock;
	while (block != NULL && tPos >= block->tEnd)
	     {
	     block = block->next;
	     }
	rechain->curBlock = block;

	/* Figure out score we get if we match current sequence */
	long long matchScore = 0;
	DNA qBase = ' ';
	if (block != NULL && block->tStart <= tPos)
	    {
	    int qIx = tPos - block->tStart + block->qStart;
	    qBase = rechain->qDna[qIx];
	    if (qBase == tBase)
		matchScore += 1;
	    else
	        matchScore -= 1;
	    }

	/* Find best previous sequence to link to */
	long long bestScore = -BIGNUM;
	struct rechain *bestSource = NULL;
	struct dlNode *el2;
	for (el2 = activeList->head; !dlEnd(el2); el2 = el2->next)
	    {
	    struct rechain *rechain2 = el2->val;
	    long long newScore = rechain2->lastScore + matchScore;
	    if (rechain2 != rechain)
	        newScore -= newChainPenalty;
	    if (bestScore < newScore)
	        {
		bestScore = newScore;
		bestSource = rechain2;
		}
	    }

	/* Update current score and links if any */
	rechain->score = bestScore;
	if (rechain->crossList->rechain != bestSource)
	    {
	    struct crossover *cross = crossoverNew(tPos, bestSource);
	    slAddHead(&rechain->crossList, cross);
	    }
	}

    /* Go through active list one last time updating last scores */
    for (el = activeList->head; !dlEnd(el); el = el->next)
        {
	struct rechain *rechain = el->val;
	rechain->lastScore = rechain->score;
	}
    }

verbose(2, "qRcSeqHash has %d elements\n", qRcSeqHash->elCount);
verbose(2, "%d chains on active list, %d on archive list\n", 
    dlCount(activeList), dlCount(archiveList));


/* Find best scoreing chain that is still active at end */
struct dlNode *el;
struct rechain *bestChain = NULL;
long long bestScore = -BIGNUM;
for (el = activeList->head; !dlEnd(el); el = el->next)
    {
    struct rechain *rechain = el->val;
    verbose(2, "active %d: score %lld last from %d\n", rechainId(rechain), rechain->score, 
	rechainId(rechain->crossList->rechain));
    if (rechain->score > bestScore)
        {
	bestChain = rechain;
	bestScore = rechain->score;
	}
    }

/* Do traceback */
struct chainPart *partList = NULL, *part;
struct rechain *rechain;
int tEnd = tSeq->size;
for (rechain = bestChain; rechain != NULL; )
    {
    struct rechain *prev = NULL;
    struct crossover *cross, *nextCross = NULL;
    int tStart = 0;
    for (cross = rechain->crossList; cross != NULL; cross = nextCross)
        {
	nextCross = cross->next;
	if (cross->tPos <= tEnd)
	    {
	    tStart = cross->tPos;
	    if (nextCross != NULL)
		prev = nextCross->rechain;
	    rechain->crossList = cross;
	    break;
	    }
	}
    chain = rechain->chain;
    if (chain != NULL)
        {
	AllocVar(part);
	part->chain = chain;
	part->tStart = tStart;
	part->tEnd = tEnd;
	slAddHead(&partList, part);
	}
    rechain = prev;
    tEnd = tStart;
    }
verbose(1, "  score %lld in %d parts\n", bestScore, slCount(partList));
for (part = partList; part != NULL; part = part->next)
    {
    struct chain *chainToFree = NULL, *subchain;
    chainSubsetOnT(part->chain, part->tStart, part->tEnd, &subchain,  &chainToFree);
    if (subchain != NULL)
	chainWrite(subchain, f);
    chainFree(&chainToFree);
    }
slFreeList(&partList);
}

void testLiftRechainer(char *chainFile, char *tTwoBit, char *qTwoBit, char *out)
/* Rework chains into lift only chain */
{
dnaUtilOpen();
struct hash *tSizeHash = twoBitChromHash(tTwoBit);
struct hash *qSizeHash = twoBitChromHash(qTwoBit);
struct chainTarget *targetList = readChainTargets(chainFile, tSizeHash, qSizeHash);
verbose(1, "Read %d chains targets to go against %d target sequences and %d query sequences\n",
    slCount(targetList), tSizeHash->elCount, qSizeHash->elCount);

/* Load up query DNA */
struct dnaSeq *qSeqList = twoBitLoadAll(qTwoBit);
struct hash *qSeqHash = hashNew(0), *qRcSeqHash = hashNew(0);
struct dnaSeq *qSeq;
long long qTotal = 0;
for (qSeq = qSeqList; qSeq != NULL; qSeq = qSeq->next)
    {
    toLowerN(qSeq->dna, qSeq->size);
    hashAdd(qSeqHash, qSeq->name, qSeq);
    qTotal += qSeq->size;
    }
verbose(1, "Read %lld query bases in %d sequences\n", qTotal, qSeqHash->elCount);

/* Open output now that all input has been at least checked */
FILE *f = mustOpen(out, "w");

struct twoBitFile *targetTwoBit = twoBitOpen(tTwoBit);
struct chainTarget *target;
for (target = targetList; target != NULL; target = target->next)
    {
    int tSize = hashIntVal(tSizeHash, target->name);
    struct dnaSeq *tSeq = twoBitReadSeqFragLower(targetTwoBit, target->name, 0, tSize);
    verbose(1, "%s has %d bases and %d chains, biggest going to %s\n", 
	target->name, tSeq->size, slCount(target->chainList), target->bigChain->qName);
    rechainOneTarget(target, tSeq, qSeqHash, qRcSeqHash, f);
    dnaSeqFree(&tSeq);
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
noAlt = optionExists("noAlt");
sameChrom = optionExists("sameChrom");
testLiftRechainer(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
