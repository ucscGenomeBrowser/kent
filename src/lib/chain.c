/* chain - pairwise alignments that can include gaps in both
 * sequences at once.  This is similar in many ways to psl,
 * but more suitable to cross species genomic comparisons. */
#include "common.h"
#include "linefile.h"
#include "dnautil.h"
#include "chain.h"

void chainFree(struct chain **pChain)
/* Free up a chain. */
{
struct chain *chain = *pChain;
if (chain != NULL)
    {
    slFreeList(&chain->blockList);
    freeMem(chain->qName);
    freeMem(chain->tName);
    freez(pChain);
    }
}

void chainFreeList(struct chain **pList)
/* Free a list of dynamically allocated chain's */
{
struct chain *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    chainFree(&el);
    }
*pList = NULL;
}

int chainCmpScore(const void *va, const void *vb)
/* Compare to sort based on total score. */
{
const struct chain *a = *((struct chain **)va);
const struct chain *b = *((struct chain **)vb);
double diff = b->score - a->score;
if (diff < 0) return -1;
else if (diff > 0) return 1;
else return 0;
}

int chainCmpTarget(const void *va, const void *vb)
/* Compare to sort based on target position. */
{
const struct chain *a = *((struct chain **)va);
const struct chain *b = *((struct chain **)vb);
return a->tStart - b->tStart;
}


static int nextId = 1;

void chainIdSet(int id)
/* Set next chain id. */
{
nextId = id;
}

void chainIdReset()
/* Reset chain id. */
{
chainIdSet(1);
}

void chainIdNext(struct chain *chain)
/* Add an id to a chain if it doesn't have one already */
{
chain->id = nextId++;
}

void chainWriteHead(struct chain *chain, FILE *f)
/* Write chain before block/insert list. */
{
static int id = 1;
if (chain->id == 0)
    chainIdNext(chain);
fprintf(f, "chain %1.0f %s %d + %d %d %s %d %c %d %d %d\n", chain->score,
    chain->tName, chain->tSize, chain->tStart, chain->tEnd,
    chain->qName, chain->qSize, chain->qStrand, chain->qStart, chain->qEnd,
    chain->id);
}

void chainWrite(struct chain *chain, FILE *f)
/* Write out chain to file in usual format*/
{
struct boxIn *b, *nextB;

chainWriteHead(chain, f);
for (b = chain->blockList; b != NULL; b = nextB)
    {
    nextB = b->next;
    fprintf(f, "%d", b->qEnd - b->qStart);
    if (nextB != NULL)
	fprintf(f, "\t%d\t%d", 
		nextB->tStart - b->tEnd, nextB->qStart - b->qEnd);
    fputc('\n', f);
    }
fputc('\n', f);
}

void chainWriteLong(struct chain *chain, FILE *f)
/* Write out chain to file in longer format*/
{
struct boxIn *b, *nextB;

chainWriteHead(chain, f);
for (b = chain->blockList; b != NULL; b = nextB)
    {
    nextB = b->next;
    fprintf(f, "%d\t%d\t", b->tStart, b->qStart);
    fprintf(f, "%d", b->qEnd - b->qStart);
    if (nextB != NULL)
	fprintf(f, "\t%d\t%d", 
		nextB->tStart - b->tEnd, nextB->qStart - b->qEnd);
    fputc('\n', f);
    }
fputc('\n', f);
}

struct chain *chainRead(struct lineFile *lf)
/* Read next chain from file.  Return NULL at EOF. 
 * Note that chain block scores are not filled in by
 * this. */
{
char *row[13];
int wordCount;
struct chain *chain;
int q,t;
static int id = 0;


wordCount = lineFileChop(lf, row);
if (wordCount == 0)
    return NULL;
if (wordCount < 12)
    errAbort("Expecting at least 12 words line %d of %s", 
    	lf->lineIx, lf->fileName);
if (!sameString(row[0], "chain"))
    errAbort("Expecting 'chain' line %d of %s", lf->lineIx, lf->fileName);
AllocVar(chain);
chain->score = atof(row[1]);
chain->tName = cloneString(row[2]);
chain->tSize = lineFileNeedNum(lf, row, 3);
if (wordCount >= 13)
    chain->id = lineFileNeedNum(lf, row, 12);
else
    chainIdNext(chain);

/* skip tStrand for now, always implicitly + */
chain->tStart = lineFileNeedNum(lf, row, 5);
chain->tEnd = lineFileNeedNum(lf, row, 6);
chain->qName = cloneString(row[7]);
chain->qSize = lineFileNeedNum(lf, row, 8);
chain->qStrand = row[9][0];
chain->qStart = lineFileNeedNum(lf, row, 10);
chain->qEnd = lineFileNeedNum(lf, row, 11);
if (chain->qStart >= chain->qEnd || chain->tStart >= chain->tEnd)
    errAbort("End before start line %d of %s", lf->lineIx, lf->fileName);
if (chain->qStart < 0 || chain->tStart < 0)
    errAbort("Start before zero line %d of %s", lf->lineIx, lf->fileName);
if (chain->qEnd > chain->qSize || chain->tEnd > chain->tSize)
    errAbort("Past end of sequence line %d of %s", lf->lineIx, lf->fileName);

/* Now read in block list. */
q = chain->qStart;
t = chain->tStart;
for (;;)
    {
    int wordCount = lineFileChop(lf, row);
    int size = lineFileNeedNum(lf, row, 0);
    struct boxIn *b;
    AllocVar(b);
    slAddHead(&chain->blockList, b);
    b->qStart = q;
    b->tStart = t;
    q += size;
    t += size;
    b->qEnd = q;
    b->tEnd = t;
    if (wordCount == 1)
        break;
    else if (wordCount < 3)
        errAbort("Expecting 1 or 3 words line %d of %s\n", 
		lf->lineIx, lf->fileName);
    t += lineFileNeedNum(lf, row, 1);
    q += lineFileNeedNum(lf, row, 2);
    }
if (q != chain->qEnd)
    errAbort("q end mismatch %d vs %d line %d of %s\n", 
    	q, chain->qEnd, lf->lineIx, lf->fileName);
if (t != chain->tEnd)
    errAbort("t end mismatch %d vs %d line %d of %s\n", 
    	t, chain->tEnd, lf->lineIx, lf->fileName);
slReverse(&chain->blockList);
return chain;
}

void chainSwap(struct chain *chain)
/* Swap target and query side of chain. */
{
struct chain old = *chain;
struct boxIn *b;

/* Copy basic stuff swapping t and q. */
chain->qName = old.tName;
chain->tName = old.qName;
chain->qStart = old.tStart;
chain->qEnd = old.tEnd;
chain->tStart = old.qStart;
chain->tEnd = old.qEnd;
chain->qSize = old.tSize;
chain->tSize = old.qSize;

/* Swap t and q in blocks. */
for (b = chain->blockList; b != NULL; b = b->next)
    {
    struct boxIn old = *b;
    b->qStart = old.tStart;
    b->qEnd = old.tEnd;
    b->tStart = old.qStart;
    b->tEnd = old.qEnd;
    }

/* Cope with the minus strand. */
if (chain->qStrand == '-')
    {
    /* chain's are really set up so that the target is on the
     * + strand and the query is on the minus strand.
     * Therefore we need to reverse complement both 
     * strands while swapping to preserve this. */
    for (b = chain->blockList; b != NULL; b = b->next)
        {
	reverseIntRange(&b->tStart, &b->tEnd, chain->tSize);
	reverseIntRange(&b->qStart, &b->qEnd, chain->qSize);
	}
    reverseIntRange(&chain->tStart, &chain->tEnd, chain->tSize);
    reverseIntRange(&chain->qStart, &chain->qEnd, chain->qSize);
    slReverse(&chain->blockList);
    }
}

void chainSubsetOnT(struct chain *chain, int subStart, int subEnd, 
    struct chain **retSubChain,  struct chain **retChainToFree)
/* Get subchain of chain bounded by subStart-subEnd on 
 * target side.  Return result in *retSubChain.  In some
 * cases this may be the original chain, in which case
 * *retChainToFree is NULL.  When done call chainFree on
 * *retChainToFree.  The score and id fields are not really
 * properly filled in. */
{
struct chain *sub = NULL;
struct boxIn *oldB, *b, *bList = NULL;
int qStart = BIGNUM, qEnd = -BIGNUM;
int tStart = BIGNUM, tEnd = -BIGNUM;

/* Check for easy case. */
if (subStart <= chain->tStart && subEnd >= chain->tEnd)
    {
    *retSubChain = chain;
    *retChainToFree = NULL;
    return;
    }

/* Build new block list and calculate bounds. */
for (oldB = chain->blockList; oldB != NULL; oldB = oldB->next)
    {
    if (oldB->tEnd <= subStart)
        continue;
    if (oldB->tStart >= subEnd)
        break;
    b = CloneVar(oldB);
    if (b->tStart < subStart)
        {
	b->qStart += subStart - b->tStart;
	b->tStart = subStart;
	}
    if (b->tEnd > subEnd)
        {
	b->qEnd -= b->tEnd - subEnd;
	b->tEnd = subEnd;
	}
    slAddHead(&bList, b);
    if (qStart > b->qStart)
        qStart = b->qStart;
    if (qEnd < b->qEnd)
        qEnd = b->qEnd;
    if (tStart > b->tStart)
        tStart = b->tStart;
    if (tEnd < b->tEnd)
        tEnd = b->tEnd;
    }
slReverse(&bList);

/* Make new chain based on old. */
if (bList != NULL)
    {
    AllocVar(sub);
    sub->blockList = bList;
    sub->qName = cloneString(chain->qName);
    sub->qSize = chain->qSize;
    sub->qStrand = chain->qStrand;
    sub->qStart = qStart;
    sub->qEnd = qEnd;
    sub->tName = cloneString(chain->tName);
    sub->tSize = chain->tSize;
    sub->tStart = tStart;
    sub->tEnd = tEnd;
    sub->id = chain->id;
    }
*retSubChain = *retChainToFree = sub;
}


