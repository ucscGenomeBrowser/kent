/* axtAffine - do alignment of two (shortish) sequences with
 * affine gap scoring, and return the result as an axt. 
 * This file is copyright 2000-2004 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "pairHmm.h"
#include "axt.h"

static char const rcsid[] = "$Id: axtAffine.c,v 1.2 2004/07/06 16:58:03 kent Exp $";


boolean axtAffineSmallEnough(double querySize, double targetSize)
/* Return TRUE if it is reasonable to align sequences of given sizes
 * with axtAffine. */
{
return targetSize * querySize <= 1.0E8;
}

static void affineAlign(char *query, int querySize, 
	char *target, int targetSize, struct axtScoreScheme *ss,
	struct phmmMatrix **retMatrix, struct phmmAliPair **retPairList,
	int *retScore)
/* Use dynamic programming to do alignment including affine gap
 * scores. */
{
struct phmmMatrix *a;
struct phmmState *hf, *iq, *it;
int qIx, tIx, sIx;  /* Query, target, and state indices */
int rowOffset, newCellOffset;
int matchOff, qSlipOff, tSlipOff;
int bestScore = -0x4fffffff;
struct phmmMommy *bestCell = NULL;
int badScore = -0x3fffffff;
int matchPair;
int gapStart, gapExt;

/* Check that it's not too big. */
if (!axtAffineSmallEnough(querySize, targetSize))
    errAbort("Can't align %d x %d, too big\n", querySize, targetSize);

gapStart = -ss->gapOpen;
gapExt = -ss->gapExtend;

/* Initialize 3 state matrix (match, query insert, target insert). */
a = phmmMatrixNew(3, query, querySize, target, targetSize);
hf = phmmNameState(a, 0, "match", 'M');
iq = phmmNameState(a, 1, "qSlip", 'Q');
it = phmmNameState(a, 2, "tSlip", 'T');

qSlipOff = -a->qDim;
tSlipOff = -1;
matchOff = qSlipOff + tSlipOff;

for (tIx = 1; tIx < a->tDim; tIx += 1)
    {
    UBYTE mommy = 0;
    int score, tempScore;

/* Macros to make me less mixed up when accessing scores from row arrays.*/
#define matchScore lastScores[qIx-1]
#define qSlipScore lastScores[qIx]
#define tSlipScore scores[qIx-1]
#define newScore scores[qIx]

/* Start up state block (with all ways to enter state) */
#define startState(state) \
   score = 0;

/* Define a transition from state while advancing over both
 * target and query. */
#define matchState(state, addScore) \
   { \
   if ((tempScore = state->matchScore + addScore) > score) \
        { \
        mommy = phmmPackMommy(state->stateIx, -1, -1); \
        score = tempScore; \
        } \
   } 

/* Define a transition from state while slipping query
 * and advancing target. */
#define qSlipState(state, addScore) \
   { \
   if ((tempScore = state->qSlipScore + addScore) > score) \
        { \
        mommy = phmmPackMommy(state->stateIx, 0, -1); \
        score = tempScore; \
        } \
   }

/* Define a transition from state while slipping target
 * and advancing query. */
#define tSlipState(state, addScore) \
   { \
   if ((tempScore = state->tSlipScore + addScore) > score) \
        { \
        mommy = phmmPackMommy(state->stateIx, -1, 0); \
        score = tempScore; \
        } \
   }

/* End a block of transitions into state. */
#define endState(state) \
    { \
    struct phmmMommy *newCell = state->cells + newCellOffset; \
    if (score <= 0) \
        { \
        mommy = phmmNullMommy; \
        score = 0; \
        } \
    newCell->mommy = mommy; \
    state->newScore = score; \
    if (score > bestScore) \
        { \
        bestScore = score; \
        bestCell = newCell; \
        } \
    } 

/* End a state that you know won't produce an optimal
 * final score. */
#define shortEndState(state) \
    { \
    struct phmmMommy *newCell = state->cells + newCellOffset; \
    if (score <= 0) \
        { \
        mommy = phmmNullMommy; \
        score = 0; \
        } \
    newCell->mommy = mommy; \
    state->newScore = score; \
    }


    rowOffset = tIx*a->qDim;
    for (qIx = 1; qIx < a->qDim; qIx += 1)
        {
        newCellOffset = rowOffset + qIx;
        
        /* Figure the cost or bonus for pairing target and query residue here. */
        matchPair = ss->matrix[a->query[qIx-1]][a->target[tIx-1]];

        /* Update hiFi space. */
            {
            startState(hf);
            matchState(hf, matchPair);
            matchState(iq, matchPair);
            matchState(it, matchPair);
            endState(hf);
            }

        /* Update query slip space. */
            {
            startState(iq);
            qSlipState(iq, gapExt);
            qSlipState(hf, gapStart);            
	    qSlipState(it, gapStart);	/* Allow double gaps, T first always. */
            shortEndState(iq);
            }
        
        /* Update target slip space. */
            {
            startState(it);
            tSlipState(it, gapExt);
            tSlipState(hf, gapStart);            
            shortEndState(it);
            }

        }
    /* Swap score columns so current becomes last, and last gets
     * reused. */
    for (sIx = 0; sIx < a->stateCount; ++sIx)
        {
        struct phmmState *as = &a->states[sIx];
        int *swapTemp = as->lastScores;
        as->lastScores = as->scores;
        as->scores = swapTemp;
        }
    }

/* Trace back from best scoring cell. */
*retPairList = phmmTraceBack(a, bestCell);
*retMatrix = a;
*retScore = bestScore;

#undef matchScore
#undef qSlipScore
#undef tSlipScore
#undef newScore
#undef startState
#undef matchState
#undef qSlipState
#undef tSlipState
#undef shortEndState
#undef endState
}

struct axt *axtAffine(bioSeq *query, bioSeq *target, struct axtScoreScheme *ss)
/* Return alignment if any of query and target using scoring scheme. */
{
struct axt *axt;
int score;
struct phmmMatrix *matrix;
struct phmmAliPair *pairList;

affineAlign(query->dna, query->size, target->dna, target->size, ss,
	&matrix, &pairList, &score);
axt = phhmTraceToAxt(matrix, pairList, score, query->name, target->name);
phmmMatrixFree(&matrix);
slFreeList(&pairList);
return axt;
}

