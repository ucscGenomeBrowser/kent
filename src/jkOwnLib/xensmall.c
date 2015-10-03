/* xensmall.c - align using dynamic programming. */
/* Copyright 2000-2003 Jim Kent.  All rights reserved. */

#include "common.h"
#include "memalloc.h"
#include "cheapcgi.h"
#include "dnautil.h"
#include "xenalign.h"
#include "pairHmm.h"


static double calcGcRatio(DNA *a, int aSize, DNA *b, int bSize)
/* Figure out percentage of g/c in a and b. */
{
int counts[4];
int i;
int total = 4;
DNA base;
int val;
int gcCount;

for (i=0; i<ArraySize(counts); ++i)
    counts[i] = 1;
for (i=0; i<aSize; ++i)
    {
    base = a[i];
    if ((val = ntVal[(int)base]) >= 0)
        {
        counts[val] += 1;
        total += 1;
        }
    }    
for (i=0; i<bSize; ++i)
    {
    base = b[i];
    if ((val = ntVal[(int)base]) >= 0)
        {
        counts[val] += 1;
        total += 1;
        }
    }    
gcCount = counts[C_BASE_VAL] + counts[G_BASE_VAL];
return (double)gcCount/(double)total;
}

/* Our seven matrices. */
enum sevenStateIx
    {
    hiFiIx = 0,
    qSlipIx,
    tSlipIx,
    loFiIx,
    c1Ix,
    c2Ix,
    c3Ix,
    };


static int loFiFromHiFiCost;
static int loFiFromCodingCost;
static int loFiFromInsertCost;

static int hiFiFromLoFiCost;
static int hiFiFromCodingCost;
static int hiFiFromInsertCost;

static int codingFromHiFiCost;
static int codingFromLoFiCost;
static int codingFromInsertCost;

static int insertFromHiFiCost;
static int insertFromLoFiCost;
static int insertFromCodingCost;

static int loFiSmallGapCost;
static int hiFiSmallGapCost;
static int codingSmallGapCost;
static int largeGapExtensionCost;


static int scaledLogOfFraction(double p, double q)
/* return log of fraction scaled by 1000 */
{
return round(1000*log((double)p/q) );
}

static int transitionCost(double prob)
/* Calculates the cost of transition of given probability */
{
return round(1000*(log(prob)));
}

static int c1c2MatchTable[26*32];
static int c3MatchTable[26*32];
static int hiFiMatchTable[26*32];
static int loFiMatchTable[26*32];

#define letterIx(a) ((a)-'a')

static void makeMatchTable(int matchTable[], double matchProb, double gcRatio)
/* Make table of match/mismatch cost/benefits for a given probability of
 * matching and a given % of GC. */
{
double atRatio = 1.0 - gcRatio;
int unlikely = round(1000*log(0.0001));
double mismatchProb = 1.0 - matchProb;
double freq[4];
DNA a,b;
int i,j;

freq[A_BASE_VAL] = freq[T_BASE_VAL] = atRatio/2;
freq[G_BASE_VAL] = freq[C_BASE_VAL] = gcRatio/2;

for (i=0; i<26*32; ++i)
        matchTable[i] = unlikely;

for (i=0; i<4; ++i)
    {
    a =  valToNt[i];
    for (j=0; j<4; ++j)
        {
        b = valToNt[j];
        if (a == b)
            matchTable[32*letterIx(a) + letterIx(b)] = scaledLogOfFraction(matchProb, freq[j]);
        else
            matchTable[32*letterIx(a) + letterIx(b)] = scaledLogOfFraction(mismatchProb, 1.0 - freq[j]);
        }
    }
}

static void makeWobbleMatchTable(int matchTable[], double gcRatio)
/* Make table of match/mismatch costs for the wobble codon. */
{
/* Assume 53% match, 33% wobble-match 14% mismatch. */
double atRatio = 1.0 - gcRatio;
int unlikely = round(1000*log(0.0001));
double freq[4];
int i;
double matchProb = 0.53;
double wobbleProb = 0.33;
double mismatchProb = 0.14*0.5;   /* Two ways to mismatch. */

freq[A_BASE_VAL] = freq[T_BASE_VAL] = atRatio/2;
freq[G_BASE_VAL] = freq[C_BASE_VAL] = gcRatio/2;

for (i=0; i<26*32; ++i)
       matchTable[i] = unlikely;
matchTable[32*letterIx('a') + letterIx('a')] = scaledLogOfFraction(matchProb, freq[A_BASE_VAL]);
matchTable[32*letterIx('a') + letterIx('c')] = scaledLogOfFraction(mismatchProb, freq[C_BASE_VAL]);
matchTable[32*letterIx('a') + letterIx('g')] = scaledLogOfFraction(wobbleProb, freq[G_BASE_VAL]);
matchTable[32*letterIx('a') + letterIx('t')] = scaledLogOfFraction(mismatchProb, freq[T_BASE_VAL]);

matchTable[32*letterIx('c') + letterIx('a')] = scaledLogOfFraction(mismatchProb, freq[A_BASE_VAL]);
matchTable[32*letterIx('c') + letterIx('c')] = scaledLogOfFraction(matchProb, freq[C_BASE_VAL]);
matchTable[32*letterIx('c') + letterIx('g')] = scaledLogOfFraction(mismatchProb, freq[G_BASE_VAL]);
matchTable[32*letterIx('c') + letterIx('t')] = scaledLogOfFraction(wobbleProb, freq[T_BASE_VAL]);

matchTable[32*letterIx('g') + letterIx('a')] = scaledLogOfFraction(wobbleProb, freq[A_BASE_VAL]);
matchTable[32*letterIx('g') + letterIx('c')] = scaledLogOfFraction(mismatchProb, freq[C_BASE_VAL]);
matchTable[32*letterIx('g') + letterIx('g')] = scaledLogOfFraction(matchProb, freq[G_BASE_VAL]);
matchTable[32*letterIx('g') + letterIx('t')] = scaledLogOfFraction(mismatchProb, freq[T_BASE_VAL]);

matchTable[32*letterIx('t') + letterIx('a')] = scaledLogOfFraction(mismatchProb, freq[A_BASE_VAL]);
matchTable[32*letterIx('t') + letterIx('c')] = scaledLogOfFraction(wobbleProb, freq[C_BASE_VAL]);
matchTable[32*letterIx('t') + letterIx('g')] = scaledLogOfFraction(mismatchProb, freq[G_BASE_VAL]);
matchTable[32*letterIx('t') + letterIx('t')] = scaledLogOfFraction(matchProb, freq[T_BASE_VAL]);
}

static void calcCostBenefit(double gcRatio)
/* Figure out weights to put on all the arrows... */
{
loFiFromHiFiCost = transitionCost(1.0/30.0);
codingFromHiFiCost = transitionCost(1.0/60.0);
insertFromHiFiCost = transitionCost(1.0/1000.0);

loFiFromCodingCost = transitionCost(1.0/200.0);
hiFiFromCodingCost = transitionCost(1.0/300.0);
insertFromCodingCost = transitionCost(1.0/1000.0);

hiFiFromLoFiCost = transitionCost(1.0/100.0);
codingFromLoFiCost = transitionCost(1.0/150.0);
insertFromLoFiCost = transitionCost(1.0/400.0);

loFiFromInsertCost = transitionCost(1.0/60.0);
hiFiFromInsertCost = transitionCost(1.0/1000.0);
codingFromInsertCost = transitionCost(1.0/1000.0);

largeGapExtensionCost = scaledLogOfFraction(74,75);

loFiSmallGapCost = scaledLogOfFraction(1,10);
hiFiSmallGapCost = scaledLogOfFraction(1,25);
codingSmallGapCost = scaledLogOfFraction(1,300);

makeMatchTable(c1c2MatchTable, 0.92, gcRatio);
//c1c2matchBonus = scaledLogOfFraction(0.92, 0.25);   /* 1.33 1.28 */
//c1c2mismatchCost = scaledLogOfFraction(0.08, 0.75); /* -2.71 -2.01 */

makeWobbleMatchTable(c3MatchTable, gcRatio);
//c3matchBonus = scaledLogOfFraction(0.6, 0.25);
//assert(c3matchBonus > 0);
//c3mismatchCost = scaledLogOfFraction(0.4, 0.75);

makeMatchTable(loFiMatchTable, 0.5, gcRatio);
//loFiMatchBonus = scaledLogOfFraction(0.5, 0.25);
//loFiMismatchCost = scaledLogOfFraction(0.5, 0.75);

makeMatchTable(hiFiMatchTable, 0.9, gcRatio);
//hiFiMatchBonus = scaledLogOfFraction(0.9, 0.25);
//hiFiMismatchCost = scaledLogOfFraction(0.1, 0.75);
}


int xenAlignSmall(DNA *query, int querySize, DNA *target, int targetSize, FILE *f,
    boolean printExtraAtEnds)
/* Use dynamic programming to do small scale (querySize * targetSize < 10,000,000)
 * alignment of DNA. */
{
struct phmmMatrix *a;
struct phmmState *hf, *lf, *iq, *it, *c1, *c2, *c3;
int qIx, tIx, sIx;  /* Query, target, and state indices */
int rowOffset, newCellOffset;
struct phmmAliPair *pairList;
int bestScore = -0x4fffffff;
struct phmmMommy *bestCell = NULL;
int c1c2PairScore, c3PairScore, loFiPairScore, hiFiPairScore;
int matchTableOffset;
double gcRatio;

/* Check that it's not too big. */
if ((double)targetSize * querySize > 1.1E7)
    errAbort("Can't align %d x %d, too big\n", querySize, targetSize);

/* Set up graph edge costs/benefits */
gcRatio = calcGcRatio(query, querySize, target, targetSize);
calcCostBenefit(gcRatio);

/* Initialize 7 state matrix. */
a = phmmMatrixNew(7, query, querySize, target, targetSize);
hf = phmmNameState(a, hiFiIx, "highFi", 'H');
lf = phmmNameState(a, loFiIx, "lowFi", 'L');
iq = phmmNameState(a, qSlipIx, "qSlip", 'Q');
it = phmmNameState(a, tSlipIx, "tSlip", 'T');
c1 = phmmNameState(a, c1Ix, "frame1", '1');
c2 = phmmNameState(a, c2Ix, "frame2", '2');
c3 = phmmNameState(a, c3Ix, "frame3", '3');

// int qSlipOff = -a->qDim;  unnecessary
// int tSlipOff = -1;  unnecessary
// int matchOff = qSlipOff + tSlipOff;  unnecessary

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
        int qBase = letterIx(a->query[qIx-1]);
        int tBase = letterIx(a->target[tIx-1]);

        newCellOffset = rowOffset + qIx;
        
        /* Figure the cost or bonus for pairing target and query residue here. */
        matchTableOffset = (qBase<<5) + tBase;
        c1c2PairScore = c1c2MatchTable[matchTableOffset];
        c3PairScore = c3MatchTable[matchTableOffset];
        hiFiPairScore = hiFiMatchTable[matchTableOffset];
        loFiPairScore = loFiMatchTable[matchTableOffset];

        /* Update hiFi space. */
            {
            startState(hf);
            matchState(hf, hiFiPairScore);
            qSlipState(hf, hiFiSmallGapCost);
            tSlipState(hf, hiFiSmallGapCost);
            matchState(iq, hiFiPairScore + hiFiFromInsertCost);
            matchState(it, hiFiPairScore + hiFiFromInsertCost);
            matchState(lf, hiFiPairScore + hiFiFromLoFiCost);
            matchState(c1, hiFiPairScore + hiFiFromCodingCost);
            matchState(c2, hiFiPairScore + hiFiFromCodingCost);
            matchState(c3, hiFiPairScore + hiFiFromCodingCost);
            endState(hf);
            }

        /* Update loFi space. */
            {
            startState(lf);
            matchState(lf, loFiPairScore);
            qSlipState(lf, loFiSmallGapCost);
            tSlipState(lf, loFiSmallGapCost);
            matchState(iq, loFiPairScore + loFiFromInsertCost);
            matchState(it, loFiPairScore + loFiFromInsertCost);
            matchState(hf, loFiPairScore + loFiFromHiFiCost);
            matchState(c1, loFiPairScore + loFiFromCodingCost);
            matchState(c2, loFiPairScore + loFiFromCodingCost);
            matchState(c3, loFiPairScore + loFiFromCodingCost);
            endState(lf);
            }

        /* Update query slip space. */
            {
            startState(iq);
            qSlipState(iq, largeGapExtensionCost);
            qSlipState(hf, insertFromHiFiCost);            
            qSlipState(lf, insertFromLoFiCost);
            qSlipState(c1, insertFromCodingCost);
            qSlipState(c2, insertFromCodingCost);
            qSlipState(c3, insertFromCodingCost);
	    qSlipState(it, largeGapExtensionCost); /* Allow double gaps, T first always. */
            shortEndState(iq);
            }
        
        /* Update target slip space. */
            {
            startState(it);
            tSlipState(it, largeGapExtensionCost);
            tSlipState(hf, insertFromHiFiCost);            
            tSlipState(lf, insertFromLoFiCost);
            tSlipState(c1, insertFromCodingCost);
            tSlipState(c2, insertFromCodingCost);
            tSlipState(c3, insertFromCodingCost);
            shortEndState(it);
            }

        /* Update coding1 space. */
            {
            startState(c1);
            matchState(c3, c1c2PairScore);
            qSlipState(c1, codingSmallGapCost);
            tSlipState(c1, codingSmallGapCost);
            matchState(iq, c1c2PairScore + codingFromInsertCost);
            matchState(it, c1c2PairScore + codingFromInsertCost);
            matchState(lf, c1c2PairScore + codingFromLoFiCost);
            matchState(hf, c1c2PairScore + codingFromHiFiCost);
            endState(c1);
            }

        /* Update coding2 space. */
            {
            startState(c2);
            matchState(c1, c1c2PairScore);
            qSlipState(c2, codingSmallGapCost);
            tSlipState(c2, codingSmallGapCost);
            matchState(iq, c1c2PairScore + codingFromInsertCost);
            matchState(it, c1c2PairScore + codingFromInsertCost);
            matchState(lf, c1c2PairScore + codingFromLoFiCost);
            matchState(hf, c1c2PairScore + codingFromHiFiCost);
            endState(c2);
            }


        /* Update coding3 space. */
            {
            startState(c3);
            matchState(c2, c3PairScore);
            qSlipState(c3, codingSmallGapCost);
            tSlipState(c3, codingSmallGapCost);
            matchState(iq, c3PairScore + codingFromInsertCost);
            matchState(it, c3PairScore + codingFromInsertCost);
            matchState(lf, c3PairScore + codingFromLoFiCost);
            matchState(hf, c3PairScore + codingFromHiFiCost);
            endState(c3);
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
pairList = phmmTraceBack(a, bestCell);
phmmPrintTrace(a, pairList, TRUE, f, printExtraAtEnds);

slFreeList(&pairList);
phmmMatrixFree(&a);
return bestScore;
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

