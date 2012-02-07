/* chainConnect - Stuff to cope with connect costs and overlaps when 
 * making chains. This works closely with the chainBlock module. */

#include "common.h"
#include "chain.h"
#include "axt.h"
#include "gapCalc.h"
#include "chainConnect.h"


double chainScoreBlock(char *q, char *t, int size, int matrix[256][256])
/* Score block through matrix. */
{
double score = 0;
int i;
for (i=0; i<size; ++i)
    score += matrix[(int)q[i]][(int)t[i]];
return score;
}

double chainCalcScore(struct chain *chain, struct axtScoreScheme *ss, 
	struct gapCalc *gapCalc, struct dnaSeq *query, struct dnaSeq *target)
/* Calculate chain score freshly. */
{
struct cBlock *b1, *b2;
double score = 0;
for (b1 = chain->blockList; b1 != NULL; b1 = b2)
    {
    score += chainScoreBlock(query->dna + b1->qStart, 
    	target->dna + b1->tStart, b1->tEnd - b1->tStart, ss->matrix);
    b2 = b1->next;
    if (b2 != NULL)
        score -=  gapCalcCost(gapCalc, b2->qStart - b1->qEnd, 
		              b2->tStart - b1->tEnd);
    }
return score;
}

double chainCalcScoreSubChain(struct chain *chain, struct axtScoreScheme *ss, 
	struct gapCalc *gapCalc, struct dnaSeq *query, struct dnaSeq *target)
/* Calculate chain score assuming query and target 
   span the chained region rather than entire chrom. */
{
struct cBlock *b1, *b2;
double score = 0;
for (b1 = chain->blockList; b1 != NULL; b1 = b2)
    {
    score += chainScoreBlock(query->dna + (b1->qStart) - chain->qStart, 
    	target->dna + (b1->tStart) - chain->tStart, b1->tEnd - b1->tStart, ss->matrix);
    b2 = b1->next;
    if (b2 != NULL)
        score -=  gapCalcCost(gapCalc, b2->qStart - b1->qEnd, 
		              b2->tStart - b1->tEnd);
    }
return score;
}

void cBlockFindCrossover(struct cBlock *left, struct cBlock *right,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq,  
	int overlap, int matrix[256][256], int *retPos, int *retScoreAdjustment)
/* Find ideal crossover point of overlapping blocks.  That is
 * the point where we should start using the right block rather
 * than the left block.  This point is an offset from the start
 * of the overlapping region (which is the same as the start of the
 * right block). */
{
int bestPos = 0;
char *rqStart = qSeq->dna + right->qStart;
char *lqStart = qSeq->dna + left->qEnd - overlap;
char *rtStart = tSeq->dna + right->tStart;
char *ltStart = tSeq->dna + left->tEnd - overlap;
int i;
double score, bestScore, rScore, lScore;

/* Make sure overlap is not larger than either block size: */
if (overlap > (left->tEnd - left->tStart) ||
    overlap > (right->tEnd - right->tStart))
    errAbort("overlap is %d -- too large for one of these:\n"
	     "qSize=%d  tSize=%d\n"
	     "left: qStart=%d qEnd=%d (%d) tStart=%d tEnd=%d (%d)\n"
	     "right: qStart=%d qEnd=%d (%d) tStart=%d tEnd=%d (%d)\n",
	     overlap, qSeq->size, tSeq->size,
	     left->qStart, left->qEnd, left->qEnd - left->qStart,
	     left->tStart, left->tEnd, left->tEnd - left->tStart,
	     right->qStart, right->qEnd, right->qEnd - right->qStart, 
	     right->tStart, right->tEnd, right->tEnd - right->tStart);

score = bestScore = rScore = chainScoreBlock(rqStart, rtStart, overlap, matrix);
lScore = chainScoreBlock(lqStart, ltStart, overlap, matrix);
for (i=0; i<overlap; ++i)
    {
    score += matrix[(int)lqStart[i]][(int)ltStart[i]];
    score -= matrix[(int)rqStart[i]][(int)rtStart[i]];
    if (score > bestScore)
	{
	bestScore = score;
	bestPos = i+1;
	}
    }
*retPos = bestPos;
*retScoreAdjustment = rScore + lScore - bestScore;
}


int chainConnectGapCost(int dq, int dt, struct chainConnect *cc)
/* Calculate cost of non-overlapping gap. */
{
return gapCalcCost(cc->gapCalc, dq, dt);
}

int chainConnectCost(struct cBlock *a, struct cBlock *b, 
	struct chainConnect *cc)
/* Calculate connection cost - including gap score
 * and overlap adjustments if any. */
{
int dq = b->qStart - a->qEnd;
int dt = b->tStart - a->tEnd;
int overlapAdjustment = 0;

if (a->qStart >= b->qStart || a->tStart >= b->tStart)
    {
    errAbort("a (%d %d) not strictly before b (%d %d)",
    	a->qStart, a->tStart, b->qStart, b->tStart);
    }
if (dq < 0 || dt < 0)
   {
   int bSize = b->qEnd - b->qStart;
   int aSize = a->qEnd - a->qStart;
   int overlap = -min(dq, dt);
   int crossover;
   if (overlap >= bSize || overlap >= aSize) 
       {
       /* One of the blocks is enclosed completely on one dimension
        * or other by the other.  Discourage this overlap. */
       overlapAdjustment = 100000000;
       }
   else
       {
       cBlockFindCrossover(a, b, cc->query, cc->target, overlap, 
       		cc->ss->matrix, &crossover, &overlapAdjustment);
       dq += overlap;
       dt += overlap;
       }
   }
return overlapAdjustment + gapCalcCost(cc->gapCalc, dq, dt);
}

static void checkStartBeforeEnd(struct chain *chain, char *message)
/* Check qStart < qEnd, tStart < tEnd for each block. */
{
struct cBlock *b;
for (b = chain->blockList; b != NULL; b = b->next)
    {
    if (b->qStart >= b->qEnd || b->tStart >= b->tEnd)
	errAbort("Start after end in (%d %d) to (%d %d) %s",
	    b->qStart, b->tStart, b->qEnd, b->tEnd, message);
    }
}

static void checkChainGaps(struct chain *chain, char *message)
/* Check that gaps between blocks are non-negative. */
{
struct cBlock *a, *b;
a = chain->blockList;
if (a == NULL)
    return;
for (b = a->next; b != NULL; b = b->next)
    {
    if (a->qEnd > b->qStart || a->tEnd > b->tStart)
	{
	errAbort("Negative gap between (%d %d - %d %d) and (%d %d - %d %d) %s",
	    a->qStart, a->tStart, a->qEnd, a->tEnd, 
	    b->qStart, b->tStart, b->qEnd, b->tEnd, message);
	}
    a = b;
    }
}

static void checkChainIncreases(struct chain *chain, char *message)
/* Check that qStart and tStart both strictly increase
 * in chain->blockList. */
{
struct cBlock *a, *b;
a = chain->blockList;
if (a == NULL)
    return;
for (b = a->next; b != NULL; b = b->next)
    {
    if (a->qStart >= b->qStart || a->tStart >= b->tStart)
	{
	errAbort("a (%d %d) not before b (%d %d) %s",
	    a->qStart, a->tStart, b->qStart, b->tStart, message);
	}
    a = b;
    }
}

void chainCalcBounds(struct chain *chain)
/* Recalculate chain boundaries - setting qStart/qEnd/tStart/tEnd from
 * a scan of blockList. */
{
struct cBlock *b = chain->blockList;
if (chain->blockList == NULL)
    return;
chain->qStart = b->qStart;
chain->tStart = b->tStart;
b = slLastEl(chain->blockList);
chain->qEnd = b->qEnd;
chain->tEnd = b->tEnd;
}

static boolean removeNegativeBlocks(struct chain *chain)
/* Removing the partial overlaps occassional results
 * in all of a block being removed.  This routine
 * removes the dried up husks of these blocks 
 * and returns TRUE if it finds any. */
{
struct cBlock *newList = NULL, *b, *next;
boolean gotNeg = FALSE;
for (b = chain->blockList; b != NULL; b = next)
    {
    next = b->next;
    if (b->qStart >= b->qEnd || b->tStart >= b->tEnd)
	{
        gotNeg = TRUE;
	freeMem(b);
	}
    else
        {
	slAddHead(&newList, b);
	}
    }
slReverse(&newList);
chain->blockList = newList;
if (gotNeg)
    chainCalcBounds(chain);
return gotNeg;
}

void setChainBounds(struct chain *chain)
/* Set chain overall bounds to fit blocks. */
{
struct cBlock *b = chain->blockList;
chain->qStart = b->qStart;
chain->tStart = b->tStart;
while (b->next != NULL)
     b = b->next;
chain->qEnd = b->qEnd;
chain->tEnd = b->tEnd;
}

void chainRemovePartialOverlaps(struct chain *chain, 
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, int matrix[256][256])
/* If adjacent blocks overlap then find crossover points between them. */
{
struct cBlock *a, *b;
boolean totalTrimA, totalTrimB;

assert(chain->blockList != NULL);

/* Do an internal sanity check to make sure that things
 * really are sorted by both qStart and tStart. */
checkChainIncreases(chain, "before removePartialOverlaps");

/* Remove overlapping portion of blocks.  In some
 * tricky repeating regions this can result in 
 * complete blocks being removed.  This complicates
 * the loop below in some cases, forcing us to essentially
 * start over when the first of the two blocks we
 * are examining gets trimmed out completely. */
for (;;)
    {
    totalTrimA = totalTrimB = FALSE;
    a = chain->blockList;
    b = a->next;
    for (;;)
        {
	int dq, dt;
	if (b == NULL)
	    break;
	dq = b->qStart - a->qEnd;
	dt = b->tStart - a->tEnd;
	if (dq < 0 || dt < 0)
	   {
	   int overlap = -min(dq, dt);
	   int aSize = a->qEnd - a->qStart;
	   int bSize = b->qEnd - b->qStart;
	   int crossover, invCross, overlapAdjustment;
	   if (overlap >= aSize || overlap >= bSize)
	       {
	       totalTrimB = TRUE;
	       }
	   else
	       {
	       cBlockFindCrossover(a, b, qSeq, tSeq, overlap, matrix, 
	                     &crossover, &overlapAdjustment);
	       b->qStart += crossover;
	       b->tStart += crossover;
	       invCross = overlap - crossover;
	       a->qEnd -= invCross;
	       a->tEnd -= invCross;
	       if (b->qEnd <= b->qStart)
		   {
		   totalTrimB = TRUE;
		   }
	       else if (a->qEnd <= a->qStart)
		   {
		   totalTrimA = TRUE;
		   }
	       }
	   }
	if (totalTrimA == TRUE)
	    {
	    removeNegativeBlocks(chain);
	    break;
	    }
	else if (totalTrimB == TRUE)
	    {
	    b = b->next;
	    freez(&a->next);
	    a->next = b;
	    totalTrimB = FALSE;
	    }
	else
	    {
	    a = b;
	    b = b->next;
	    }
	}
    if (!totalTrimA)
        break;
    }

/* Reset chain bounds - may have clipped them in this
 * process. */
setChainBounds(chain);

/* Do internal sanity checks. */
checkChainGaps(chain, "after removePartialOverlaps");
checkStartBeforeEnd(chain, "after removePartialOverlaps");
}

void chainMergeAbutting(struct chain *chain)
/* Merge together blocks in a chain that abut each
 * other exactly. */
{
struct cBlock *newList = NULL, *b, *last = NULL, *next;
for (b = chain->blockList; b != NULL; b = next)
    {
    next = b->next;
    if (last == NULL || last->qEnd != b->qStart || last->tEnd != b->tStart)
	{
	slAddHead(&newList, b);
	last = b;
	}
    else
        {
	last->qEnd = b->qEnd;
	last->tEnd = b->tEnd;
	freeMem(b);
	}
    }
slReverse(&newList);
chain->blockList = newList;
}

