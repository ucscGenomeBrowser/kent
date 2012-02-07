/* chainToAxt - convert from chain to axt format. */

#include "common.h"
#include "chain.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "axt.h"
#include "chainToAxt.h"


static struct axt *axtFromBlocks(
	struct chain *chain,
	struct cBlock *startB, struct cBlock *endB,
	struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset)
/* Convert a list of blocks (guaranteed not to have inserts in both
 * strands between them) to an axt. */
{
int symCount = 0;
int dq, dt, blockSize = 0, symIx = 0;
struct cBlock *b, *a = NULL;
struct axt *axt;
char *qSym, *tSym;

/* Make a pass through figuring out how big output will be. */
for (b = startB; b != endB; b = b->next)
    {
    if (a != NULL)
        {
	dq = b->qStart - a->qEnd;
	dt = b->tStart - a->tEnd;
	symCount += dq + dt;
	}
    blockSize = b->qEnd - b->qStart;
    symCount += blockSize;
    a = b;
    }

/* Allocate axt and fill in most fields. */
AllocVar(axt);
axt->qName = cloneString(chain->qName);
axt->qStart = startB->qStart;
axt->qEnd = a->qEnd;
axt->qStrand = chain->qStrand;
axt->tName = cloneString(chain->tName);
axt->tStart = startB->tStart;
axt->tEnd = a->tEnd;
axt->tStrand = '+';
axt->symCount = symCount;
axt->qSym = qSym = needLargeMem(symCount+1);
qSym[symCount] = 0;
axt->tSym = tSym = needLargeMem(symCount+1);
tSym[symCount] = 0;

/* Fill in symbols. */
a = NULL;
for (b = startB; b != endB; b = b->next)
    {
    if (a != NULL)
        {
	dq = b->qStart - a->qEnd;
	dt = b->tStart - a->tEnd;
	if (dq == 0)
	    {
	    memset(qSym+symIx, '-', dt);
	    memcpy(tSym+symIx, tSeq->dna + a->tEnd - tOffset, dt);
	    symIx += dt;
	    }
	else
	    {
	    assert(dt == 0);
	    memset(tSym+symIx, '-', dq);
	    memcpy(qSym+symIx, qSeq->dna + a->qEnd - qOffset, dq);
	    symIx += dq;
	    }
	}
    blockSize = b->qEnd - b->qStart;
    memcpy(qSym+symIx, qSeq->dna + b->qStart - qOffset, blockSize);
    memcpy(tSym+symIx, tSeq->dna + b->tStart - tOffset, blockSize);
    symIx += blockSize;
    a = b;
    }
assert(symIx == symCount);

/* Fill in score and return. */
axt->score = axtScoreDnaDefault(axt);
return axt;
}

struct axt *chainToAxt(struct chain *chain, 
	struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset, int maxGap, int maxChain)
/* Convert a chain to a list of axt's.  This will break
 * where there is a double-sided gap in chain, or 
 * where there is a single-sided gap greater than maxGap, or 
 * where there is a chain longer than maxChain.
 */
{
struct cBlock *startB = chain->blockList, *a = NULL, *b;
struct axt *axtList = NULL, *axt;

for (b = chain->blockList; b != NULL; b = b->next)
    {
    if (a != NULL)
        {
	int dq = b->qStart - a->qEnd;
	int dt = b->tStart - a->tEnd;
	if ((dq > 0 && dt > 0) || dt > maxGap || dq > maxGap || (b->tEnd - startB->tStart) > maxChain)
	    {
	    axt = axtFromBlocks(chain, startB, b, qSeq, qOffset, tSeq, tOffset);
	    slAddHead(&axtList, axt);
	    startB = b;
	    }
	}
    a = b;
    }
axt = axtFromBlocks(chain, startB, NULL, qSeq, qOffset, tSeq, tOffset);
slAddHead(&axtList, axt);
slReverse(&axtList);
return axtList;
}
