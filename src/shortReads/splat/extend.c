/* extend - extend tag matches to full sequence matches, making use of
 * banded Smith-Waterman. */

#include "common.h"
#include "chain.h"
#include "axt.h"
#include "bandExt.h"
#include "splix.h"
#include "splat.h"

static struct splatAlign *tagToAlign(struct splatTag *tag, 
	struct dnaSeq *qSeqF,  struct dnaSeq *qSeqR,
	struct splix *splix, struct axtScoreScheme *scoreScheme)
/* Convert splatTag to splatAlign on the basic level.  Don't (yet) 
 * fill in score field or do extension. */
{
char strand = tag->strand;
struct dnaSeq *qSeq = (strand == '-' ? qSeqR : qSeqF);
int chromIx = splixOffsetToChromIx(splix, tag->t1);
int chromOffset = splix->chromOffsets[chromIx];
DNA *q = qSeq->dna;
DNA *t = splix->allDna + chromOffset;

/* Allocate and fill  out cBlock structure on first alignment block. */
struct cBlock *block1;
AllocVar(block1);
block1->qStart = tag->q1;
block1->qEnd = tag->q1 + tag->size1;
block1->tStart = tag->t1 - chromOffset;
block1->tEnd = tag->t1 + tag->size1;

/* Allocate and fill in chain structure. */
struct chain *chain;
AllocVar(chain);
chain->tName = cloneString(splix->chromNames[chromIx]);
chain->tSize = splix->chromSizes[chromIx];
chain->tStart = block1->tStart;
chain->tEnd = block1->tEnd;
chain->qName = cloneString(qSeq->name);
chain->qSize = qSeq->size;
chain->qStrand = strand;
chain->qStart = block1->qStart;
chain->qEnd = block1->qEnd;
chain->blockList = block1;

/* Allocate and fill out splatAlign struct. */
struct splatAlign *align;
AllocVar(align);
align->chain = chain;
align->qDna = q;
align->tDna = t;

/* If need be add second block to alignment. */
if (tag->size2 > 0)
    {
    struct cBlock *block2;
    AllocVar(block2);
    block2->qStart = tag->q2;
    chain->qEnd = block2->qEnd = tag->q2 + tag->size2;
    block2->tStart = tag->t2 - chromOffset;
    chain->tEnd = block2->tEnd = tag->t2 + tag->size2;
    block1->next = block2;
    }
return align;
}


double chainAddAxtScore(struct chain *chain, DNA *qDna, DNA *tDna, 
	struct axtScoreScheme *scoreScheme)
/* Score chain according to scoring scheme. */
{
double score = 0;
struct cBlock *b, *bNext;
for (b = chain->blockList; b != NULL; b = bNext)
    {
    int bSize = b->tEnd - b->tStart;
    b->score = axtScoreUngapped(scoreScheme, qDna + b->qStart, tDna + b->tStart, bSize);
    score += b->score;
    bNext = b->next;
    if (bNext != NULL)
        {
	int qGap = bNext->qStart - b->qEnd;
	int tGap = bNext->tStart - b->tEnd;
	int gap = qGap + tGap;
	score -= gap * scoreScheme->gapExtend - scoreScheme->gapOpen;
	}
    }
chain->score = score;
return score;
}


struct splatAlign *splatExtendTag(struct splatTag *tag,
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix,
	struct axtScoreScheme *scoreScheme)
/* Convert a single tag to an alignment. */
{
int maxSingleGap = 9;
int maxTotalGap = maxSingleGap * 3;
struct splatAlign *ali = tagToAlign(tag, qSeqF, qSeqR, splix, scoreScheme);
struct chain *chain = ali->chain;
int qxSize = chain->qSize - chain->qEnd;	/* extra q size. */
int txSize = chain->tSize - chain->tEnd;	/* extra t size. */
if (qxSize > 0 && txSize > 0)
    {
    int qxStart = chain->qEnd;
    int qxEnd = chain->qSize;
    int txStart = chain->tEnd;
    int txEnd = chain->tSize;
    int txMaxSize = qxSize + maxTotalGap;
    if (txSize > txMaxSize)
        txSize = txMaxSize;
    int symAlloc = qxSize + txSize;
    char *qSym, *tSym;
    AllocArray(qSym, symAlloc);
    AllocArray(tSym, symAlloc);
    int symCount, revStartQ, revStartT;
    if (bandExt(FALSE, scoreScheme, 9, 
    		ali->qDna + qxStart, qxSize,  
		ali->tDna + txStart, txSize, 1,
		symAlloc, &symCount, qSym, tSym, &revStartQ, &revStartT))
	{
	struct cBlock *newBlocks = cBlocksFromAliSym(symCount, 
		qSym, tSym, qxStart, txStart);
	struct cBlock *lastOldBlock = slLastEl(chain->blockList);
	struct cBlock *lastNewBlock = slLastEl(newBlocks);
	chain->qEnd = lastNewBlock->qEnd;
	chain->tEnd = lastNewBlock->tEnd;
	if (lastOldBlock->qEnd == newBlocks->qStart && lastOldBlock->tEnd == newBlocks->tStart)
	    {
	    /* Merge in first new block to last old block, and dispose of first new block. */
	    lastOldBlock->qEnd = newBlocks->qEnd;
	    lastOldBlock->tEnd = newBlocks->tEnd;
	    lastOldBlock->next = newBlocks->next;
	    freeMem(newBlocks);
	    }
	else
	    lastOldBlock->next = newBlocks;
	}
    freeMem(qSym);
    freeMem(tSym);
    }
chainAddAxtScore(ali->chain, ali->qDna, ali->tDna, scoreScheme);
return ali;
}

struct splatAlign *splatExtendTags(struct splatTag *tagList, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix,
	struct axtScoreScheme *scoreScheme)
/* Convert a list of tags to a list of alignments. */
{
struct splatTag *tag;
struct splatAlign *aliList = NULL;
for (tag = tagList; tag != NULL; tag = tag->next)
    {
    struct splatAlign *ali = splatExtendTag(tag, qSeqF, qSeqR, splix, scoreScheme);
    slAddHead(&aliList, ali);
    }
slReverse(&aliList);
return aliList;
}

