/* chainToPsl - convert between chains and psl.  Both of these
 * are alignment formats that can handle gaps in both strands
 * and do not include the sequence itself. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "psl.h"
#include "chain.h"



struct psl *chainToPsl(struct chain *chain)
/* chainToPsl - convert chain to psl.  This does not fill in
 * the match, repMatch, mismatch, and N fields since it needs
 * the sequence for that.  It does fill in the rest though. */
{
struct psl *psl;
int blockCount, i;
struct cBlock *b, *nextB;

blockCount = slCount(chain->blockList);
AllocVar(psl);
AllocArray(psl->blockSizes, blockCount);
AllocArray(psl->qStarts, blockCount);
AllocArray(psl->tStarts, blockCount);
psl->strand[0] = chain->qStrand;
psl->qName = cloneString(chain->qName);
psl->qSize = chain->qSize;
if (chain->qStrand == '-')
    {
    psl->qStart = chain->qSize - chain->qEnd;
    psl->qEnd = chain->qSize - chain->qStart;
    }
else
    {
    psl->qStart = chain->qStart;
    psl->qEnd = chain->qEnd;
    }
psl->tName = cloneString(chain->tName);
psl->tSize = chain->tSize;
psl->tStart = chain->tStart;
psl->tEnd = chain->tEnd;
psl->blockCount = blockCount;
for (i=0, b=chain->blockList; i < blockCount; ++i, b = nextB)
    {
    nextB = b->next;
    psl->tStarts[i] = b->tStart;
    psl->qStarts[i] = b->qStart;
    psl->blockSizes[i] = b->qEnd - b->qStart;
    if (nextB != NULL)
        {
	int qGap = nextB->qStart - b->qEnd;
	int tGap = nextB->tStart - b->tEnd;
	if (qGap != 0)
	    {
	    psl->qBaseInsert += qGap;
	    psl->qNumInsert += 1;
	    }
	if (tGap != 0)
	    {
	    psl->tBaseInsert += tGap;
	    psl->tNumInsert += 1;
	    }
	}
    }
return psl;
}

static void fillInMatchEtc(struct chain *chain, 
	struct dnaSeq *query, struct dnaSeq *target, struct psl *psl)
/* Fill in psl->match,mismatch,repMatch, and nCount fields.
 * Assumes that query and target are on correct strands already. */
{
struct cBlock *block;
unsigned match = 0, misMatch=0, repMatch=0, nCount=0;
for (block = chain->blockList; block != NULL; block = block->next)
    {
    DNA *qDna = query->dna + block->qStart;
    DNA *tDna = target->dna + block->tStart;
    int i, size = block->qEnd - block->qStart;
    for (i=0; i<size; ++i)
        {
	DNA q,t;
	int qv, tv;
	q = qDna[i];
	t = tDna[i];
	qv = ntVal[(int)q];
	tv = ntVal[(int)t];
	if (qv < 0 || tv < 0)
	    ++nCount;
	else if (qv == tv)
	    {
	    if (isupper(q) && isupper(t))
	        ++match;
	    else
	        ++repMatch;
	    }
	else 
	    ++misMatch;
        }
    }
psl->match = match;
psl->misMatch = misMatch;
psl->repMatch = repMatch;
psl->nCount = nCount;
}

struct psl *chainToFullPsl(struct chain *chain, 
	struct dnaSeq *query,   /* Forward query sequence. */
	struct dnaSeq *rQuery,	/* Reverse complemented query sequence. */
	struct dnaSeq *target)
/* Convert chainList to pslList, filling in matches, N's etc. */
{
struct psl *psl = chainToPsl(chain);
struct dnaSeq *qSeq = (chain->qStrand == '-' ? rQuery : query);
fillInMatchEtc(chain, qSeq, target, psl);
return psl;
}



