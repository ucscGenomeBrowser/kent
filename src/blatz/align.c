/* align - blatz alignment module.  The strategy is to scan the index
 * for msps in the gapless module, chain the msps, band extend, and
 * then fill in gaps by doing it again on a more detailed index of
 * the gap region. */
/* Copyright 2005 Jim Kent.  All rights reserved. */


#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "spacedSeed.h"
#include "axt.h"
#include "dnautil.h"
#include "localmem.h"
#include "gapCalc.h"
#include "chain.h"
#include "chainBlock.h"
#include "chainConnect.h"
#include "fuzzyFind.h"
#include "bandExt.h"
#include "boxClump.h"
#include "boxLump.h"
#include "bzp.h"
#include "blatz.h"
#include "dynamic.h" // LX

static struct chain *chainsCreate(struct gapCalc *gapCalc, 
        struct axtScoreScheme *ss,
        struct dnaSeq *query, char strand, 
         struct dnaSeq *target, struct cBlock **pBlockList)
/* Call chainer on blocks and clean things up a bit. */
{
struct chain *chain, *chainList;
struct cBlock *block;
struct chainConnect cc;

/* Set up variables for gap/connect costs. */
ZeroVar(&cc);
cc.target = target;
cc.query = query;
cc.ss = ss;
cc.gapCalc = gapCalc;

/* We might not have scored the block list properly yet. */
for (block = *pBlockList; block != NULL; block = block->next)
    {
    block->score = chainScoreBlock(query->dna + block->qStart,
                                   target->dna + block->tStart,
                                   block->qEnd - block->qStart, ss->matrix);
    }

/* Call the fancy KD-tree based chainer. */
chainList = chainBlocks(query->name, query->size, strand, 
                        target->name, target->size, pBlockList, 
                        (ConnectCost)chainConnectCost, 
                        (GapCost)chainConnectGapCost, 
                        &cc, NULL);

/* Clean up overlaps */
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    chainRemovePartialOverlaps(chain, query, target, ss->matrix);
    chainMergeAbutting(chain);
    }
return chainList;
}


static void insertNewBlocks(struct cBlock *blockList, 
        struct cBlock **pInsertPoint)
/* Insert new block list at insertion point. */
{
if (blockList != NULL)
    {
    struct cBlock *lastInList = slLastEl(blockList);
    lastInList->next = *pInsertPoint;
    *pInsertPoint = blockList;
    }
}

static struct cBlock *extendInRegion(struct bzp *bzp,
                             struct dnaSeq *query, int qStart, int qEnd,
                             struct dnaSeq *target, int tStart, int tEnd,
                             int dir, char *qSym, char *tSym, int symAlloc)
/* Do banded extension in region and return as a list of blocks. */
{
int maxExtend = bzp->maxExtend;
int symCount;
int qSize = qEnd - qStart;
int tSize = tEnd - tStart;
int qs, ts, qExtStart, tExtStart;
if (qSize > 0 && tSize > 0)
    {
    if (qSize > maxExtend) qSize = maxExtend;
    if (tSize > maxExtend) tSize = maxExtend;
    if (dir < 0)
        {
        qStart = qEnd - qSize;
        tStart = tEnd - tSize;
        }
    bandExt(FALSE, bzp->ss, bzp->maxBandGap, 
            query->dna + qStart,  qSize,
            target->dna + tStart, tSize,
            dir, symAlloc, &symCount, qSym, tSym, &qs, &ts);
    if (dir > 0)
        {
        qExtStart = qStart;
        tExtStart = tStart;
        }
    else
        {
        qExtStart = qEnd - qs - 1;
        tExtStart = tEnd - ts - 1;
        }
    return cBlocksFromAliSym(symCount, qSym, tSym, qExtStart, tExtStart);
    }
else
    return NULL;
}

static void chainBandExtend(struct bzp *bzp, struct chain *chain, 
        struct dnaSeq *query, struct dnaSeq *target)
/* Call banded extender on each block of chain. */
{
struct cBlock *oldFirst, *mid, *block, *nextBlock, *newList, *endNewList;
int symAlloc = 2 * bzp->maxExtend;
char *qSym, *tSym;

qSym = needMem(symAlloc);
tSym = needMem(symAlloc);

#ifdef DEBUG
uglyf("chainBandExtend t:%d-%d, q:%d-%d\n", chain->tStart, chain->tEnd, 
        chain->qStart, chain->qEnd);
chainWrite(chain, uglyOut);
#endif /* DEBUG */

/* Do extension before first block in chain. */
oldFirst = chain->blockList;
newList = extendInRegion(bzp, query, 0, chain->qStart, target, 0, chain->tStart,
                         -1, qSym, tSym, symAlloc);
#ifdef DEBUG
uglyf("got %d new blocks before first\n", slCount(newList));
#endif /* DEBUG */
if (newList != NULL)
    insertNewBlocks(newList, &chain->blockList);

/* Do extensions between blocks. */
for (block = oldFirst; ; block = nextBlock)
    {
    nextBlock = block->next;
    if (nextBlock == NULL)
        break;

    /* Extend into beginning of block. */
    newList = extendInRegion(bzp, query, block->qEnd, nextBlock->qStart,
                                 target, block->tEnd, nextBlock->tStart,
                             1, qSym, tSym, symAlloc);
    if (newList != NULL)
        {
        endNewList = slLastEl(newList);
        endNewList->next = block->next;
        block->next = newList;
        mid = endNewList;
        }
    else
        mid = block;

    newList = extendInRegion(bzp, query, block->qEnd, nextBlock->qStart,
                                 target, block->tEnd, nextBlock->tStart,
                             -1, qSym, tSym, symAlloc);

#ifdef DEBUG
    uglyf("got %d new blocks in middle\n", slCount(newList));
#endif /* DEBUG */
    if (newList != NULL)
        insertNewBlocks(newList, &mid->next);
    }

/* Do extension after last block in chain. */
newList = extendInRegion(bzp, query, chain->qEnd, query->size, 
                         target, chain->tEnd, target->size,
                         1, qSym, tSym, symAlloc);
#ifdef DEBUG
if (bzpTimeOn) uglyf("got %d new blocks after last\n", slCount(newList));
#endif /* DEBUG */
if (newList != NULL)
    insertNewBlocks(newList, &block->next);

chainMergeAbutting(chain);
chainCalcBounds(chain);

/* Clean up. */
freez(&qSym);
freez(&tSym);
}

static void shrinkBlocks(struct chain *chain, struct dnaSeq *query, 
        struct dnaSeq *target, int matrix[256][256])
/* Trim away up to 50 bp from each end of each block in
 * so that band extension can correct problems introduced
 * by over-aggressive unbanded extension. */
{
struct cBlock *block, *lastBlock = NULL;

for (block = chain->blockList; block != NULL; block = block->next)
    {
    int size = block->qEnd - block->qStart;
    int newSize = size - 200;
    int sizeDiff;
    int addStart, subEnd;
    if (newSize < 1) newSize = 1;
    sizeDiff = size - newSize;
    addStart = (sizeDiff>>1);
    subEnd = sizeDiff - addStart;
    block->qStart += addStart;
    block->tStart += addStart;
    block->qEnd -= subEnd;
    block->tEnd -= subEnd;
    lastBlock = block;
    }
block = chain->blockList;
chain->qStart = block->qStart;
chain->tStart = block->tStart;
chain->qEnd = lastBlock->qEnd;
chain->tEnd = lastBlock->tEnd;
}

static void bandExtendChains(struct bzp *bzp, struct chain *chainList, 
        struct dnaSeq *query, struct dnaSeq *target)
/* Loop through chain list doing banded extensions. */
{
struct chain *chain;
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    shrinkBlocks(chain, query, target, bzp->ss->matrix);
    chainBandExtend(bzp, chain, query, target);
    }
}

static void addBlocksToList(struct cBlock **pList, struct cBlock **toAdd)
/* Add blocks in 'toAdd' to pList.  This will end up scrambling
 * orders. */
{
struct cBlock *block, *next;
for (block = *toAdd; block != NULL; block = next)
    {
    next = block->next;
    slAddHead(pList, block);
    }
*toAdd = NULL;
}
        
static void chainsToBlocks(struct chain **pChainList, 
        struct cBlock **pBlockList)
/* Convert chains to just a list of blocks.  Free chains. */
{
struct chain *chain;
for (chain = *pChainList; chain != NULL; chain = chain->next)
    addBlocksToList(pBlockList, &chain->blockList);
chainFreeList(pChainList);
}

static void thresholdChains(struct chain **pChainList, int minScore)
/* Remove chains under the threshold from chainList. */
{
struct chain *chain, *chainList = NULL, *next;
for (chain = *pChainList; chain != NULL; chain = next)
    {
    next = chain->next;
    if (chain->score >= minScore)
        {
        slAddHead(&chainList, chain);
        }
    else
        chainFree(&chain);
    }
slReverse(&chainList);
*pChainList = chainList;
}

static void blatzChainAndExtend(struct bzp *bzp, 
        struct dnaSeq *query, char strand, struct dnaSeq *target, 
        struct cBlock **pBlockList, struct chain **retChainList)
/* Process msp block list into chains. */
{
struct cBlock *blockList = NULL;
struct chain *chain, *chainList = NULL, *next;

#ifdef DEBUG
struct cBlock *block;
    {
    for (block = *pBlockList; block != NULL; block = block->next)
        fprintf(uglyOut, "block: %s %d-%d, %s %d-%d\n",
                query->name, block->qStart, block->qEnd,
                target->name, block->tStart, block->tEnd);
    }
#endif /* DEBUG */

chainList = chainsCreate(bzp->cheapGap, bzp->ss, 
        query, strand, target, pBlockList);
bzpTime("chainsCreate %d initial chains", slCount(chainList));

if (bzp->bestChainOnly)
    {
    if (chainList != NULL)
	chainFreeList(&chainList->next);
    }
else if (bzp->maxChainsToExplore > 0)
    {
    struct chain *lastChain = slElementFromIx(chainList, 
    	bzp->maxChainsToExplore-1);
    if (lastChain != NULL)
	{
	verbose(2, "Trimming chain list from %d", slCount(chainList));
        chainFreeList(&lastChain->next);
	verbose(2, " to %d\n", slCount(chainList));
	}
    }


thresholdChains(&chainList, bzp->minChain);
bzpTime("%d chains passed threshold of %d", slCount(chainList), bzp->minChain);

if (bzp->maxExtend > 0)
    {
    bandExtendChains(bzp, chainList, query, target);
    bzpTime("banded extension");
    }

#ifdef DEBUG
    {
    fprintf(uglyOut, "---- Chains after banded extension ----\n");
    for (chain = chainList; chain != NULL; chain = chain->next)
        chainWrite(chain, uglyOut);
    }
#endif /* DEBUG */

chainsToBlocks(&chainList, &blockList);

chainList = chainsCreate(bzp->gapCalc, bzp->ss,
        query, '+', target, &blockList);
bzpTime("rechaining with normal gap costs");

#ifdef OLD
/* Move to cumulative chain list. */
for (chain = chainList; chain != NULL; chain = next)
    {
    next = chain->next;
    if (chain->score >= bzp->minScore)
        {
        slAddHead(retChainList, chain);
        }
    else
        chainFree(&chain);
    }
#endif /* OLD */

/* Move to cumulative chain list. */
for (chain = chainList; chain != NULL; chain = next)
    {
    next = chain->next;
    slAddHead(retChainList, chain);
    }

chainList = NULL;
}

static struct chain *blatzChainAgainstIndex(struct bzp *bzp, 
        struct blatzIndex *index, struct dnaSeq *query, char strand)
/* Align query sequences against indexed target. */
{
struct cBlock *blockList = NULL;
struct chain *chainList = NULL;              /* Chains here. */

/* Collect MSPs. */
bzpTime("Before index");
blatzGaplessScan(bzp, index, query, &blockList);
bzpTime("Scanned index");

if (blockList != NULL)
    blatzChainAndExtend(bzp, query, strand, index->target, 
          &blockList, &chainList);
slReverse(&chainList);
return chainList;
}

int uglyCount, uglyTotalQ, uglyTotalT;
double uglyArea;

static void expandInRegion(struct bzp *bzp,
                             struct dnaSeq *query, int qStart, int qEnd, 
                             char strand,
                             struct dnaSeq *target, int tStart, int tEnd,
                             struct cBlock **pBlockList)
/* Do alignment on a region inside of query/target governed by bzp parameters. */
{
int qSize = qEnd - qStart;
int tSize = tEnd - tStart;
struct dnaSeq qNew, tNew;
struct blatzIndex *index;
struct chain *chainList, *chain;

#ifdef DEBUG
uglyf("----- expandInRegion t:%d-%d, q:%d-%d weight %d  -----\n", tStart, tEnd, qStart, qEnd, bzp->weight);
#endif /* DEBUG */
    {
    int qSize = qEnd - qStart;
    int tSize = tEnd - tStart;
    uglyCount += 1;
    uglyTotalQ += qSize;
    uglyTotalT += tSize;
    uglyArea += (double)qSize * tSize;
    }

/* Create subsequences of query and target. */
ZeroVar(&qNew);
ZeroVar(&tNew);
qNew.dna = query->dna + qStart;
qNew.size = qSize;
qNew.name = query->name;
tNew.dna = target->dna + tStart;
tNew.size = tSize;
tNew.name = target->name;

index = blatzIndexOne(&tNew, tStart, tEnd, target->size, bzp->weight);
chainList = blatzChainAgainstIndex(bzp, index, &qNew, strand);
blatzIndexFree(&index);

for (chain = chainList; chain != NULL; chain = chain->next)
    {
    cBlocksAddOffset(chain->blockList, qStart, tStart);
    addBlocksToList(pBlockList, &chain->blockList);
    }
chainFreeList(&chainList);
}


static struct cBlock *focusOnClumps(struct bzp *bzp, struct dnaSeq *query, 
        char strand, struct dnaSeq *target, struct boxClump *clumpList)
/* Do alignment process on each clump at parameters more sensitive than
 * the passed in bzp.  Return list of blocks. */
{
struct boxClump *clump;
struct bzp bzpNew = *bzp;
struct cBlock *blockList = NULL;

/* Create more sensitive alignment seed parameters (but less aggressive extension). */
if (bzpNew.weight >= 9)
    bzpNew.weight = bzpNew.weight - 3;
else
    bzpNew.weight = bzpNew.weight - 2;
bzpNew.minScore -= 500;
bzpNew.minGapless -= 500;
bzpNew.minChain -= 500;
bzpNew.maxExtend /= 6;
bzpNew.maxBandGap /= 6;
bzpNew.bestChainOnly = TRUE;

for (clump = clumpList; clump != NULL; clump = clump->next)
    {
#ifdef DEBUG
    uglyf("---- focusing on clump t:%d-%d (%d) q:%d-%d (%d) ----\n", 
            clump->tStart, clump->tEnd, clump->tEnd - clump->tStart, 
        clump->qStart, clump->qEnd, clump->qEnd - clump->qStart);
#endif /* DEBUG */
    expandInRegion(&bzpNew, query, clump->qStart, clump->qEnd, strand,
                   target, clump->tStart, clump->tEnd, &blockList);
    }
return blockList;
}

static void addBox(struct boxIn **pList, int qs, int qe, int ts, int te)
/* Allocate box and add to list.  */
{
struct boxIn *box;
AllocVar(box);
box->qStart = qs;
box->qEnd = qe;
box->tStart = ts;
box->tEnd = te;
slAddHead(pList, box);
}

static void addClippedBox(struct boxIn **pList, int minSize, int winSize, 
        int qStart, int qEnd, int tStart, int tEnd)
/* Add box if it is big enough. Split and add if too big. */
{
int qSize = qEnd - qStart, tSize = tEnd - tStart;
if (qSize >= minSize && tSize >= minSize)
    {
    if (qSize > winSize || tSize > winSize)
        {
        /* Must split box in two. */
        int qe,qs,te,ts;

        /* Add box at beginning. */
        qs = qStart;
        qe = qStart + winSize;
        if (qe > qEnd) qe = qEnd;
        ts = tStart;
        te = tStart + winSize;
        if (te > tEnd) te = tEnd;
        addBox(pList, qs, qe, ts, te);

        /* Add box at end. */
        qe = qEnd;
        qs = qEnd - winSize;
        if (qs < qStart) qs = qStart;
        te = tEnd;
        ts = tEnd - winSize;
        if (ts < tStart) ts = tStart;
        addBox(pList, qs, qe, ts, te);
        }
    else
        addBox(pList, qStart, qEnd, tStart, tEnd);
    }
}

struct boxClump *boxFindLumps(struct boxIn **pBoxList);	// uglyf

static struct boxClump *clumpGapsAndEnds(struct bzp *bzp, 
        struct chain *chainList)
/* Given a list of chains (all on same target and query) create a list of
 * regions where we'll want redo with smaller seeds.  Clump these regions
 * together. */
{
struct chain *chain;
struct boxIn *boxList = NULL;
struct cBlock *b1, *b2;
struct boxClump *clumpList = NULL;
int winSize = bzp->expandWindow;
int minSize = bzp->weight * 2;

for (chain = chainList; chain != NULL; chain = chain->next)
    {
    if (chain->score >= bzp->minExpand)
        {
        int qs, qe, ts, te;
        qe = chain->qStart;
        qs = qe - winSize;
        if (qs < 0) qs = 0;
        te = chain->tStart;
        ts = te - winSize;
        if (ts < 0) ts = 0;
        addClippedBox(&boxList, minSize, winSize, qs, qe, ts, te);
        b1 = chain->blockList;
        for (b2 = b1->next; b2 != NULL; b2 = b2->next)
            {
            addClippedBox(&boxList, minSize, winSize, 
                    b1->qEnd, b2->qStart, b1->tEnd, b2->tStart);
            b1 = b2;
            }
        qs = chain->qEnd;
        qe = qs + winSize;
        if (qe > chain->qSize) qe = chain->qSize;
        ts = chain->tEnd;
        te = ts + winSize;
        if (te > chain->tSize) te = chain->tSize;
        addClippedBox(&boxList, minSize, winSize, qs, qe, ts, te);
        }
    }
clumpList = boxLump(&boxList);
    { /* uglyf */
    struct boxClump *clump;
    int boxCount = 0;
    int clumpCount = 0;
    long clumpArea = 0;
    for (clump = clumpList; clump != NULL; clump = clump->next)
        {
	boxCount += clump->boxCount;
	clumpArea += (clump->qEnd - clump->qStart) * (clump->tEnd - clump->tStart);
	clumpCount += 1;
	}
    bzpTime("%d boxes, %d clumps, %ld clump area", boxCount, clumpCount, clumpArea);
    } /* uglyf */
return clumpList;
}

static void removeSimpleOverlaps(struct cBlock **pBlockList)
/* Remove from list blocks that overlap and are on same diagonal. */
{
struct cBlock *newList = NULL, *b, *next, *last = NULL;
slSort(pBlockList, cBlockCmpDiagQuery);
for (b = *pBlockList; b != NULL; b = next)
    {
    next = b->next;
    if (last != NULL)
        {
        /* Check same diagonal. */
        if ((b->tStart - b->qStart) == (last->tStart - last->qStart))
            {
            /* Check for overlap. */
            if (last->qEnd >= b->qStart)
                {
                /* Fold this block into previous one. */
                if (last->qEnd < b->qEnd) last->qEnd = b->qEnd;
                if (last->tEnd < b->tEnd) last->tEnd = b->tEnd;
                freez(&b);
                }
            }
        }
    if (b != NULL)
        {
        slAddHead(&newList, b);
        last = b;
        }
    }
slReverse(&newList);
*pBlockList = newList;
}

static void findBestGapPos(struct axtScoreScheme *ss,  
               DNA *qStart, int qGap, int qSize,
               DNA *tStart, int tGap, int tSize,
               int *retBestPos, int *retBestScore)
/* Figure out where to position gap for maximal score. */
{
int maxSize = max(qSize, tSize);
int gapSize = qGap + tGap;  /* One of qGap or tGap is zero */
int matchSize = maxSize - gapSize;
int pos, bestPos = 0, rightPos;
int bestScore = -BIGNUM;
int score;

for (pos=0; pos<=matchSize; ++pos)
    {
    rightPos = matchSize-pos;
    score = axtScoreUngapped(ss, qStart, tStart, pos);
    score += axtScoreUngapped(ss, qStart+qSize-rightPos, 
    			tStart+tSize-rightPos, rightPos);
    if (score > bestScore)
        {
        bestScore = score;
        bestPos = pos;
        }
    }
*retBestPos = bestPos;
*retBestScore = bestScore;
}


static void reduceGaps(struct bzp *bzp, 
        struct dnaSeq *query, struct dnaSeq *target, struct chain *chain)
/* Go through chain looking for small double-sided gaps that
 * might be turned into single-sided gaps (or even eliminated)
 * in a way that would improve the score. */
{
struct cBlock *b1, *b2;

#ifdef DEBUG
chain->score = chainCalcScore(chain, bzp->ss, bzp->gapCalc, query, target); // uglyf
uglyf("score %f vs %f before reduce gaps\n", chain->score, 
    chainCalcScore(chain, bzp->ss, bzp->gapCalc, query, target) );
#endif /* DEBUG */

b1 = chain->blockList;
for (b2 = b1->next; b2 != NULL; b2 = b2->next)
    {
    int dq = b2->qStart - b1->qEnd;
    int dt = b2->tStart - b1->tEnd;
    if (dq != 0 && dt != 0)
        {
        if (dt == dq && dt < 200)
            {
            int curScore = -gapCalcCost(bzp->gapCalc, dq, dt);
            int score = axtScoreUngapped(bzp->ss, 
                query->dna + b1->qEnd, target->dna + b1->tEnd, dt);
            if (score >= curScore)
                {
                /* Merge b2 into b1, pop b2 out of list, and free it. */
                b1->qEnd = b2->qEnd;
                b1->tEnd = b2->tEnd;
                b1->next = b2->next;
                freeMem(b2);
                b2 = b1;
                chain->score += score - curScore;
                }
            }
        else if (dq < 30 || dt < 30)
            {
            int curScore = -gapCalcCost(bzp->gapCalc, dq, dt);
            int minGap = min(dq,dt);
            int newDq = dq - minGap;
            int newDt = dt - minGap;
            int newGapScore = -gapCalcCost(bzp->gapCalc, newDq, newDt);
            int matchScore, bestPos;
            int scoreImprovement;

            findBestGapPos(bzp->ss, query->dna + b1->qEnd, newDq, dq,
                           target->dna + b1->tEnd , newDt, dt,
                           &bestPos, &matchScore);
            scoreImprovement = (newGapScore + matchScore) - curScore;
#ifdef DEBUG
            uglyf(" score improvement %d\n", scoreImprovement);
#endif /* DEBUG */
            if (scoreImprovement >= 0)
                {
                b1->qEnd += bestPos;
                b1->tEnd += bestPos;
                b2->qStart = b1->qEnd + newDq;
                b2->tStart = b1->tEnd + newDt;
                chain->score += scoreImprovement;
                }
            }
        }
    b1 = b2;
    }
#ifdef DEBUG
uglyf("score %f vs %f after reduce gaps\n", chain->score, 
    chainCalcScore(chain, bzp->ss, bzp->gapCalc, query, target) );
#endif /* DEBUG */
}

struct ffAli *chainToFfAli(struct chain *chain, 
        struct dnaSeq *query, struct dnaSeq *target)
/* Construct ffAli's from chain's blocks. */
{
struct ffAli *ff, *ffList = NULL;
DNA *q = query->dna, *t = target->dna;
struct cBlock *block;

for (block = chain->blockList; block != NULL; block = block->next)
    {
    AllocVar(ff);
    ff->nStart = q + block->qStart;
    ff->nEnd = q + block->qEnd;
    ff->hStart = t + block->tStart;
    ff->hEnd = t + block->tEnd;
    ff->left = ffList;
    ffList = ff;
    }
ffList = ffMakeRightLinks(ffList);
return ffList;
}

static void slideIntronsInChain(struct chain *chain, 
        struct dnaSeq *query, struct dnaSeq *target)
/* Slide around gaps to maximize gt/ag-ness of ends. */
{
if (chain->blockList->next != NULL)        /* Don't waste time on single-exons */
    {
    /* Call existing code that works on ffAli's. */
    struct ffAli *ff, *ffList = chainToFfAli(chain, query, target);
    struct cBlock *block;
    DNA *q = query->dna, *t = target->dna;

    ffSlideIntrons(ffList);
    for (ff = ffList, block = chain->blockList; 
            ff != NULL; 
            ff = ff->right, block = block->next)
        {
        block->qStart =  ff->nStart - q;
        block->qEnd =  ff->nEnd - q;
        block->tStart =  ff->hStart - t;
        block->tEnd =  ff->hEnd - t;
        }
    ffFreeAli(&ffList);
    }
}

static struct chain *blatzAlignOne(struct bzp *bzp, struct blatzIndex *index,
        struct dnaSeq *query, char strand)
/* Align query sequence against indexed target sequence. */
{
struct chain *chain, *chainList;
struct dnaSeq *target = index->target;
struct cBlock *blockList = NULL;

/* Do main alignment and chaining. */
chainList = blatzChainAgainstIndex(bzp, index, query, strand);

/* Build up list of regions between blocks and before and after
 * chains and run aligner at more sensitive settings on these regions. */
if (bzp->expandWindow > 0)
    {
    struct boxClump *missingClumps = clumpGapsAndEnds(bzp, chainList);
    bzpTimeOn = FALSE;
    blockList = focusOnClumps(bzp, query, strand, target, missingClumps);
    bzpTimeOn = TRUE;
    boxClumpFreeList(&missingClumps);
#ifdef DEBUG
    uglyf("focus stats:  count %d, area %f, tTotal %d, qTotal %d\n",
               uglyCount, uglyArea, uglyTotalT, uglyTotalQ);
#endif /* DEBUG */
    bzpTime("expand chains from finer seeds - got %d new blocks", 
            slCount(blockList));
    }

/* Add blocks from original chains to the mix. */
chainsToBlocks(&chainList, &blockList);

/* Create final list of chains. */
removeSimpleOverlaps(&blockList);
chainList = chainsCreate(bzp->gapCalc, bzp->ss,
        query, strand, target, &blockList);
thresholdChains(&chainList, bzp->minScore);
bzpTime("final chaining");

for (chain = chainList; chain != NULL; chain = chain->next)
    {
    reduceGaps(bzp, query, target, chain);
    if (bzp->rna)
        slideIntronsInChain(chain, query, target);
    }

#ifdef DEBUG
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    double oldScore = chain->score;
    chain->score = chainCalcScore(chain, bzp->ss, bzp->gapCalc, query, target); 
    if (oldScore != chain->score)
         warn("score inconsistency %f vs %f", oldScore, chain->score);
    }
#endif /* DEBUG */

bzpTime("reduced double to single gaps");
return chainList;
}

static struct chain *blatzAlignStrand(struct bzp *bzp, 
        struct blatzIndex *indexList,
        struct dnaSeq *query, char strand)
/* Align query sequence against list of indexed target sequences. 
 * The order of chains returned is close to random. */
{
struct blatzIndex *index;
struct chain *chainList = NULL;
int j; // LX

/* Loop through indexes, aligning against each, and collecting results
 * into chainList. */
for (index = indexList; index != NULL; index = index->next)
    {
    struct chain *oneList = NULL, *chain, *nextChain;
    // LX BEG
    if (bzp->dynaLimitT<VERY_LARGE_NUMBER)
       {
       targetHitDLimit = bzp->dynaLimitT;
       // allocate zeroed memory for hit counters if necessary
       if (index->counter == NULL)
          {
          // index->counter could be set later?
          AllocArray(index->counter, index->target->size);
          globalCounter = index->counter;
          }
       }
    if (bzp->dynaLimitQ<VERY_LARGE_NUMBER)
       {
       // allocate zeroed memory for hit counters
       AllocArray(dynaCountQtemp, query->size); 
       }
    // LX END
    oneList = blatzAlignOne(bzp, index, query, strand);
    // LX BEG
    // Add the contents of dynaCountQtemp to dynaCountQ
    if (bzp->dynaLimitQ<VERY_LARGE_NUMBER)
       {
       for (j=0;j<query->size;j++)
           {
           dynaCountQ[j] = dynaCountQ[j]+dynaCountQtemp[j];
           }
       }
    freez(&dynaCountQtemp); 
    // LX END
    for (chain = oneList; chain != NULL; chain = nextChain)
        {
        nextChain = chain->next;
        slAddHead(&chainList, chain);
        }
    }
return chainList;
}

struct chain *blatzAlign(struct bzp *bzp, struct blatzIndex *indexList, 
        struct dnaSeq *query)
/* Align both strands of query against index using parameters in bzp.
 * Return chains sorted by score (highest scoring first) */
{
struct chain *chainList, *plusChains, *minusChains;
plusChains = blatzAlignStrand(bzp, indexList, query, '+');
reverseComplement(query->dna, query->size);
minusChains = blatzAlignStrand(bzp, indexList, query, '-');
reverseComplement(query->dna, query->size);
chainList = slCat(plusChains, minusChains);
slSort(&chainList, chainCmpScore);
return chainList;
}

