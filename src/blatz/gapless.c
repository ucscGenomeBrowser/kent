/* gapless - scan index for hits, and do gapless extensions. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "localmem.h"
#include "dlist.h"
#include "obscure.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "seqStats.h"
#include "rbTree.h"
#include "axt.h"
#include "chain.h"
#include "bzp.h"
#include "blatz.h"
#include "dynamic.h" // LX

static struct cBlock *gaplessExtendAndFilter(
    DNA *qDna, int qPos, int qSize, 
    DNA *tDna, int tPos, int tSize,
    int hitSpan,
    int matrix[256][256], int xDrop, int minScore)
/* Extend pos to left and right.  If result is better than minScore
 * then create a block on it. */
{
int totalScore = 0, leftExt=0, rightExt=0;
struct cBlock *block = NULL;

/* Figure score in middle. */
    {
    DNA *q = qDna + qPos;
    DNA *t = tDna + tPos;
    int i;
    for (i=0; i<hitSpan; ++i)
        totalScore += matrix[q[i]][t[i]];
    }

/* Extend to right. */
    {
    DNA *q = qDna + qPos;
    DNA *t = tDna + tPos;
    int qBases = qSize - qPos;
    int tBases = tSize - tPos;
    int maxBases = min(qBases, tBases);
    int i, bestPos=hitSpan-1, score=0, bestScore=0;

    for (i=hitSpan; i<maxBases; ++i)
        {
        if ((score += matrix[q[i]][t[i]]) > bestScore)
            {
            bestPos = i;
            bestScore = score;
            }
        if (score < bestScore - xDrop)
            break;
        }
    totalScore += bestScore;
    rightExt = bestPos+1;
    }

/* Extend to left. */
    {
    DNA *q = qDna + qPos;
    DNA *t = tDna + tPos;
    int maxBases = -min(qPos, tPos);
    int i, bestPos=0, score=0, bestScore=0;
    for (i=-1; i>=maxBases; --i)
        {
        if ((score += matrix[q[i]][t[i]]) > bestScore)
            {
            bestPos = i;
            bestScore = score;
            }
        if (score < bestScore - xDrop)
            break;
        }
    totalScore += bestScore;
    leftExt = bestPos;
    }

if (totalScore >= minScore)
    {
    totalScore *= dnaMatchEntropy(qDna + qPos+leftExt, tDna + tPos + leftExt, rightExt - leftExt);
    if (totalScore >= minScore)
        {
        AllocVar(block);
        block->qStart = qPos + leftExt;
        block->qEnd = qPos + rightExt;
        block->tStart = tPos + leftExt;
        block->tEnd = tPos + rightExt;
        block->score = totalScore;
        }
    }

return block;
}

struct diagonalTrack
/* Keep track of diagonal. */
    {
    int diagonal;          /* Diagonal coordinate (qPos - tPos) */
    struct cBlock *block;  /* Current block. */
    };

static int diagonalTrackCmp(void *va, void *vb)
/* Set up rbTree so as to work on strings. */
{
struct diagonalTrack *a = va, *b = vb;
return a->diagonal - b->diagonal;
}

#define CLOSE_ENOUGH 200
#define invLog2 1.442695

static int calcCloseBufSize(int seedWeight, int tSize)
/* Calculate expected number of hits over CLOSE_ENOUGH bases and
 * grab next power of two, fudging upwards a good bit.  This is
 * a good size for the closeBuf.  This tends to return 1k for small
 * targets and 64k for chromosome-sized targets. */
{
double expected = 200 + (double)CLOSE_ENOUGH * tSize * (seedWeight+1)/ (1<<2*seedWeight);
double minPowerOfTwo = log(expected)*invLog2;
int roundPower = minPowerOfTwo + 3;
int bufSize = (1<<roundPower);
// uglyf("Expected %f, minPowerOfTwo %f, roundPower %d, bufSize %d\n", expected, minPowerOfTwo, roundPower, bufSize);
return bufSize;
}

struct diagNode
/* Track diagonals. */
    {
    struct dlNode node;        /* Val element points to self. */
    int diag;                /* Diagonal value. */
    int qPos;                /* Query position. */
    };
    
// LX BEG
int doDynaCounting(int targetPos, int queryPos)
    {
    int retval = 0;
                    // Count all hits to report statistics later on
                    dynaHits++;
                    // Now let's see if dynaLimitT has been set
                    if(targetHitDLimit<VERY_LARGE_NUMBER){
                      // If we are over the limit, we will stop here
                      // and continue at the next target pos
                      if(globalCounter[targetPos] > targetHitDLimit){
                        dynaCountTarget++; // statistics
                        retval = 1;
                        //continue; 
                      } else {
                        // ... otherwise keep counting the hits at this position
                        // but only if query pos that matters 
                        if(queryHitDLimit<VERY_LARGE_NUMBER){
                          // is not over its hit limit
                          if(dynaCountQ[queryPos] <= queryHitDLimit){
                            globalCounter[targetPos]++;
                          }
                        } else {
                          globalCounter[targetPos]++;
                        }
                      }
                    }
                    // Now let's see if dynaLimitQ has been set
                    if(queryHitDLimit<VERY_LARGE_NUMBER){
                      // If we reached the limit, we will stop here
                      // and continue evaluating the next query pos
                      if(dynaCountQ[queryPos] > queryHitDLimit){
                        dynaCountQuery++; // statistics
                        dynaBreak = 1;
                        retval = 2;
                        //break; // go to the outer loop to get the next query pos
                      } else {
                        // ... otherwise keep counting the hits at this position
                        //dynaCountQ[queryPos]++; // postponing setting the mask
                        dynaCountQtemp[queryPos]++;
                      }
                    }
    return retval;
    }
// LX END

void blatzGaplessScan(struct bzp *bzp, struct blatzIndex *index, 
        struct dnaSeq *query, struct cBlock **pMsps)
/* Scan index for hits, do gapless extensions into maximally
 * scoring segment pairs (MSPs), and put MSPs over threshold
 * onto pBlockList. */
{
int lastBase = query->size - index->seedSpan;
struct dnaSeq *target = index->target;
int queryPos;
int i;
struct rbTree *tree = rbTreeNew(diagonalTrackCmp);
int hitCount = 0, mspCount = 0, doubleHitCount = 0;
int nbdToggleStart;
boolean multiHits = bzp->multiHits;

int diagBufSize = 0;
int diagMask = 0;
struct dlList *diagLists = NULL;
struct diagNode *diagNodes = NULL, *diagNode = NULL;
int diagCircIx = 0;

if (bzp->transition)
    nbdToggleStart = (1 << (2*(index->seedWeight-1)));
else
    nbdToggleStart = 0;
if (multiHits)
    {
    diagBufSize = calcCloseBufSize(index->seedWeight, target->size);
    diagMask = diagBufSize - 1;
    AllocArray(diagNodes, diagBufSize);
    AllocArray(diagLists, diagBufSize);
    for (i=0; i<diagBufSize; ++i)
        {
        dlListInit(&diagLists[i]);
        diagNode = &diagNodes[i];
        diagNode->node.val = diagNode;
        }
    }

// LX BEG
dynaBreak = 0;
// LX END
 
/* Scan through query collecting hits. */
for (queryPos=0; queryPos<=lastBase; ++queryPos)
    {
    int key = blatzIndexKey(query->dna + queryPos, 
                            index->seedOffsets, index->seedWeight);
    if ((key >= 0) && ((dynaWordLimit == 0 ) || (dynaWordCount[key]<=2*dynaWordLimit)))
    //if (key >= 0) // LX
        {
        int tog;
        if(dynaWordLimit>0) dynaWordCount[key]++; // LX
        /* Add key, and everything that differs by a single transition 
         * from key to index.  This relies on the fact that the binary
         * representation we've chosen for DNA is a little unusual, and
         * has the property that flipping the least significant bit
         * is equivalent to a transition (A-G or C-T) mutation. */
        for (tog = nbdToggleStart; ; tog >>= 2)
            {
            struct blatzIndexPos *iPos = &index->slots[tog^key];
            bits32 *pos = iPos->pos;
            int i, count = iPos->count;
            int dynaRet; // LX
            if (multiHits)
                {
                for (i=0; i<count; ++i)
                    {
                    int targetPos = pos[i];
                    int diagonal = queryPos - targetPos;
                    int diagMod = (diagonal & diagMask);
                    struct dlList *diagList = &diagLists[diagMod];
                    struct dlNode *node;
                    boolean gotDoubleHit = FALSE;
                    dynaRet = doDynaCounting(targetPos, queryPos); // LX
                    if(dynaRet == 1) continue; // LX
                    if(dynaRet == 2) break; // LX
                    ++hitCount;
                    for (node = diagList->head;  !dlEnd(node); node = node->next)
                        {
                        diagNode = node->val;
                        // if (bzpTimeOn) uglyf("  diagMod %d, diagNode->diag = %d, diagNode->qPos = %d, queryPos=%d, qDif %d\n", diagMod, diagNode->diag, diagNode->qPos, queryPos, queryPos - diagNode->qPos);
                        if (diagNode->diag == diagonal 
                            && queryPos - diagNode->qPos <= CLOSE_ENOUGH)
                            {
                            dlRemove(node);        /* We'll put one back shortly! */
                            gotDoubleHit = TRUE;
                            break;
                            }
                        }
                    // if (bzpTimeOn) uglyf("qPos %d, tPos %d, diagonal %d, diagMod %d, diagMask %d, doubleHit %d\n", queryPos, targetPos, diagonal, diagMod, diagMask, gotDoubleHit);
                    if (gotDoubleHit)
                        {
                        struct diagonalTrack sdt, *dt;
                        ++doubleHitCount;
                        sdt.diagonal = queryPos - targetPos;
                        dt = rbTreeFind(tree, &sdt);
                        if (dt == NULL || dt->block->qEnd < queryPos)
                            {
                            struct cBlock *block = gaplessExtendAndFilter(
                                 query->dna, queryPos, query->size,
                                 target->dna, targetPos, target->size,
                                 index->seedSpan, bzp->ss->matrix, 
                                 bzp->maxDrop, bzp->minGapless);
                            if (block != NULL)
                                {
                                ++mspCount;
                                slAddHead(pMsps, block);
                                if (dt == NULL)
                                    {
                                    lmAllocVar(tree->lm, dt);
                                    dt->diagonal = sdt.diagonal;
                                    rbTreeAdd(tree, dt);
                                    }
                                dt->block = block;
                                }
                            }
                        }
                    diagNode = &diagNodes[diagCircIx];
                    diagCircIx += 1;
                    diagCircIx &= diagMask;
                    if (diagNode->node.next != NULL)
                        dlRemove(&diagNode->node);
                    diagNode->diag = diagonal;
                    diagNode->qPos = queryPos;
                    dlAddHead(diagList, &diagNode->node);
                    }
                    // LX BEG
                    // Check to see if we got here from the inner loop
                    if(dynaBreak == 1){
                      break; // we need to break again
                    }
                    // LX END
                }
            else
                {
                /* This is a simpler version of the multi-hit loop.
                 * Duplicating a bit of code here to avoid putting
                 * more branches in the time critical inner loop. */
                for (i=0; i<count; ++i)
                    {
                    int targetPos = pos[i];
                    int diagonal = queryPos - targetPos;
                    struct diagonalTrack sdt, *dt;
                    int dynaRet;
                    dynaRet = doDynaCounting(targetPos, queryPos); // LX
                    if(dynaRet == 1) continue; // LX
                    if(dynaRet == 2) break; // LX
                    ++hitCount;
                    sdt.diagonal = queryPos - targetPos;
                    dt = rbTreeFind(tree, &sdt);
                    if (dt == NULL || dt->block->qEnd < queryPos)
                        {
                        struct cBlock *block = gaplessExtendAndFilter(
                             query->dna, queryPos, query->size,
                             target->dna, targetPos, target->size,
                             index->seedSpan, bzp->ss->matrix, 
                             bzp->maxDrop, bzp->minGapless);
                        if (block != NULL)
                            {
                            ++mspCount;
                            slAddHead(pMsps, block);
                            if (dt == NULL)
                                {
                                lmAllocVar(tree->lm, dt);
                                dt->diagonal = sdt.diagonal;
                                rbTreeAdd(tree, dt);
                                }
                            dt->block = block;
                            }
                        }
                    }
                }
            if (tog == 0)
                break;
            }
            // LX BEG
            // Check to see if we got here from the middle loop
            if(dynaBreak == 1){
              dynaBreak=0;
              continue; // we need the next query position then
            }
            // LX END
        }
    }
if (bzpTimeOn) verbose(2, "%d hits, %d double hits, %d MSPs, %d diagonals\n", hitCount, doubleHitCount, mspCount, tree->n);
rbTreeFree(&tree);
freez(&diagNodes);
freez(&diagLists);
}

