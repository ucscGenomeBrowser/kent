/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* xensmall.c - align using dynamic programming. */
#include "common.h"
#include "memalloc.h"
#include "cheapcgi.h"
#include "dnautil.h"
#include "xenalign.h"

/* Mommy coding scheme - this is how one cell in the dynamic programming table
 * points to it's parent (mommy) cell.  Since these tables are really big,
 * rather than use a simple pointer costing four bytes, we use a encoding
 * scheme that requires only one byte. 
 *
 * Bits 0-4  the "hidden" state of the mommy.  Lets us have 32 hidden states.
 *           currently only using 7.
 * Bit  5    whether or not mommy is in previous cell in query
 * Bit  6    whether or not mommy is in previous cell in target
 * Bit  7    set during trace back for cells along optimal path
 *
 * Since the query and target advancing bits (5 and 6) are never both zero,
 * it is safe to use the value of all-bits-zero as an indicator of
 * no mommy. */

/* Compress state, query, and target offset into one byte. */
#define packMommy(stateIx, qOff, tOff) ((UBYTE)((stateIx) + ((-(qOff))<<5) + ((-(tOff))<<6)))

/* Traceback sets this, really just for debugging. */
#define mommyTraceBit (1<<7)

static UBYTE nullMommy = 0; /* mommy value for orphans.... */

static void unpackMommy(UBYTE mommy, int *retStateIx, int *retQoff, int *retToff)
/* Unpack state, query, and target offset. */
{
*retStateIx = (mommy&31);
*retQoff = -((mommy&32)>>5);
*retToff = -((mommy&64)>>6);
}

struct aliCell
/* This is a single cell in a single state of the matrix. */
    {
    UBYTE mommy; /* Unlike a parent, you can only have one mommy! */
    };

struct aliState
/* This corresponds to a hidden Markov state.  Each one of
 * these has a two dimensional array[targetSize+1][querySize+1]
 * of cells. */
    {
    struct aliCell *cells;
    struct aliCell *endCells;
    int *scores;
    int *lastScores;
    int stateIx;
    char *name;
    char emitLetter;
    };

struct aliMatrix
/* The alignment matrix - has an array of states. */
    {
    char *query;
    char *target;    
    int querySize;
    int targetSize;
    int qDim;
    int tDim;
    int stateCount;
    int stateSize;
    int stateByteSize;
    struct aliState *states;
    struct aliCell *allCells;
    int *allScores;
    };

static void initAliMatrix(struct aliMatrix *am, int stateCount, 
    char *query, int querySize, char *target, int targetSize)
/* Allocate all memory required for an aliMatrix. Set up dimensions. */
{
int i;
struct aliCell *allCells;
int allCellSize;
int rowSize;
int *allScores;

zeroBytes(am, sizeof(*am));
am->query = query;
am->target = target;
am->querySize = querySize;
am->targetSize = targetSize;
am->qDim = rowSize = am->querySize + 1;
am->tDim = am->targetSize + 1;
am->stateCount = stateCount;
am->stateSize = rowSize * am->tDim;
am->stateByteSize = am->stateSize * sizeof(struct aliCell);
am->states = needMem(stateCount * sizeof(struct aliState));

/* Initialize matrix of cells for each state. */
allCellSize = stateCount * am->stateByteSize;
am->allCells = allCells = needLargeMem(allCellSize); 
memset(allCells, 0, allCellSize);
for (i=0; i<stateCount; ++i)
    {
    am->states[i].cells = allCells;
    allCells += am->stateSize;
    am->states[i].endCells = allCells;
    am->states[i].stateIx = i;
    }

/* Initialize two rows of scores for each state. */
allScores = am->allScores = needMem(rowSize * 2 * stateCount * sizeof(int) );
for (i=0; i<stateCount; ++i)
    {
    am->states[i].scores = allScores;
    allScores += rowSize;
    am->states[i].lastScores = allScores;
    allScores += rowSize;
    }
}

static void cleanupAliMatrix(struct aliMatrix *am)
/* Free up memory required for an aliMatrix and make sure
 * nobody reuses it. */
{
freeMem(am->states);
freeMem(am->allCells);
freeMem(am->allScores);
zeroBytes(am, sizeof(*am));
}

static struct aliState *nameState(struct aliMatrix *am, int stateIx, char *name, char emitLetter)
/* Give a name to a state and return a pointer to it. */
{
struct aliState *state;
assert(stateIx < am->stateCount);
state = &am->states[stateIx];
state->name = name;
state->emitLetter = emitLetter;
return state;
}

static void findMatrixIx(struct aliMatrix *am, struct aliCell *cell, int *retStateIx, int *retQix, int *retTix)
/* Given a cell in matrix return state, query, and target index. */
{
int cellIx = cell - am->allCells;
*retStateIx = cellIx/am->stateSize;
cellIx %= am->stateSize;
*retTix = cellIx / am->qDim;
*retQix = cellIx % am->qDim;
}

static struct aliCell *findMommy(struct aliMatrix *am, struct aliCell *baby)
/* Find baby's mommy and return it. */
{
int momStateIx, qOff, tOff;
int babyStateIx, qIx, tIx;
UBYTE mommy;

if ((mommy = baby->mommy) == nullMommy)
    return NULL;
unpackMommy(mommy, &momStateIx, &qOff, &tOff);
findMatrixIx(am, baby, &babyStateIx, &qIx, &tIx);
return am->states[momStateIx].cells + (tOff + tIx) * am->qDim + (qOff + qIx);
}

struct aliPair
/* Each position in alignment gets one of these. */
    {
    struct aliPair *next;
    int queryIx;
    int targetIx;
    UBYTE hiddenIx;  /* If I code one of these with more than 255 states shoot me! */
    char querySym;
    char targetSym;
    char hiddenSym;
    };

static struct aliPair *traceBack(struct aliMatrix *am, struct aliCell *end)
/* Create list of alignment pair by tracing back through matrix from end
 * state back to a start.*/
{
struct aliPair *pairList = NULL, *pair;
struct aliCell *cell, *parent = end;
int parentSix, parentTix, parentQix;
int sIx, tIx, qIx;

findMatrixIx(am, parent, &parentSix, &parentQix, &parentTix);
for (;;)
    {
    cell = parent;
    sIx = parentSix;
    tIx = parentTix;
    qIx = parentQix;

    if ((parent = findMommy(am, cell)) == NULL)
        break;
    findMatrixIx(am, parent, &parentSix, &parentQix, &parentTix);
    
    cell->mommy != mommyTraceBit;

    AllocVar(pair);
    pair->hiddenIx = (UBYTE)sIx;
    pair->hiddenSym = am->states[sIx].emitLetter; 

    if (parentQix == qIx - 1 && parentTix == tIx - 1 )
        {
        pair->queryIx = qIx-1;
        pair->querySym = am->query[qIx-1];
        pair->targetIx = tIx - 1;
        pair->targetSym = am->target[tIx-1];
        }
    else if (parentQix == qIx) /* && parentTix == tIx-1 */
        {
        pair->queryIx = -1;
        pair->querySym = '-';
        pair->targetIx = tIx-1;
        pair->targetSym = am->target[tIx-1];
        }
    else  /* parentTix == tIx  && parentQix == qIx-1 */
        {
        pair->queryIx = qIx-1;
        pair->querySym = am->query[qIx-1];
        pair->targetIx = -1;
        pair->targetSym = '-';
        }
    slAddHead(&pairList, pair);
    }
return pairList;
}

static void printTrace(struct aliMatrix *am, struct aliPair *pairList, boolean showStates, 
    FILE *f, boolean extraAtEnds)
/* Print out trace to file. */
{
struct aliPair *pair;
#define lineLen 50
char asyms[lineLen+1];
char qsyms[lineLen+1];
char tsyms[lineLen+1];
char hsyms[lineLen+1];
int lineIx = 0;
int qs, ts;
boolean gotQs = FALSE;

if ((pair = pairList) == NULL)
    {
    fprintf(f, "Empty pair list\n");
    return;
    }

qs = pair->queryIx;
ts = pair->targetIx;

/* Print out up to 25 bp of initial non-matching parts. */
if (extraAtEnds)
    {
    int qStart = qs;
    int tStart = ts;
    int i;

    for (i= -25; i < 0; ++i)
        {
        int qIx = qStart + i;
        int tIx = tStart + i;
        qsyms[lineIx] = (qIx >= 0 ? am->query[qIx] : ' ');
        tsyms[lineIx] = (tIx >= 0 ? am->target[tIx] : ' ' );
        asyms[lineIx] = hsyms[lineIx] = ' ';
        if (qIx >= 0 || tIx >= 0)
            {
            ++lineIx;
            if (!gotQs)
                {
                gotQs = TRUE;
                qs = qIx;
                ts = tIx;
                }
            }
        }
    }

/* Print out matching parts */
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    qsyms[lineIx] = pair->querySym;
    tsyms[lineIx] = pair->targetSym;
    asyms[lineIx] = (pair->querySym == pair->targetSym ? '|' : ' ');
    hsyms[lineIx] = pair->hiddenSym;
    if (++lineIx == lineLen)
        {
        qsyms[lineIx] = asyms[lineIx] = tsyms[lineIx] = hsyms[lineIx] = 0;
        fprintf(f, "%5d %s\n", qs, qsyms);
        fprintf(f, "%5s %s\n", "", asyms);
        fprintf(f, "%5d %s\n", ts, tsyms);
        if (showStates)
            fprintf(f, "%5s %s\n", "", hsyms);
        fputc('\n', f);
        lineIx = 0;
        if (pair->next)
            {
            qs = pair->next->queryIx;
            ts = pair->next->targetIx;
            }
        }
    }

/* Print out last bit - first get last pair. */
if (extraAtEnds)
    {
    for (pair = pairList; pair->next != NULL; pair = pair->next)
        ;

        {
        int qIx, tIx, i;
        int residue = lineLen - lineIx;
        for (i=1; i<=residue; i++)
            {
            if ((qIx = pair->queryIx+i) >= am->querySize)
                qsyms[lineIx] = ' ';
            else
                qsyms[lineIx] = am->query[qIx];
            if ((tIx = pair->targetIx+i) >= am->targetSize)
                tsyms[lineIx] = ' ';
            else
                tsyms[lineIx] = am->target[tIx];
            asyms[lineIx] = ' ';
            hsyms[lineIx] = ' ';
            ++lineIx;
            assert(lineIx <= lineLen);
            }
        }
    }

/* Print out anything not printed out yet. */
if (lineIx != 0)
    {
    qsyms[lineIx] = asyms[lineIx] = tsyms[lineIx] = hsyms[lineIx] = 0;
    fprintf(f, "%5d %s\n", qs, qsyms);
    fprintf(f, "%5s %s\n", "", asyms);
    fprintf(f, "%5d %s\n", ts, tsyms);
    if (showStates)
        fprintf(f, "%5s %s\n", "", hsyms);
    fputc('\n', f);
    lineIx = 0;
    }
#undef lineLen
}


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
    if ((val = ntVal[base]) >= 0)
        {
        counts[val] += 1;
        total += 1;
        }
    }    
for (i=0; i<bSize; ++i)
    {
    base = b[i];
    if ((val = ntVal[base]) >= 0)
        {
        counts[val] += 1;
        total += 1;
        }
    }    
gcCount = counts[C_BASE_VAL] + counts[G_BASE_VAL];
return (double)gcCount/(double)total;
}

/* Our three matrices. */
enum matIx
    {
    hiFiIx = 0,
    loFiIx,
    qSlipIx,
    tSlipIx,
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

#define nucVal(a) ((a)-'a')

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
            matchTable[32*nucVal(a) + nucVal(b)] = scaledLogOfFraction(matchProb, freq[j]);
        else
            matchTable[32*nucVal(a) + nucVal(b)] = scaledLogOfFraction(mismatchProb, 1.0 - freq[j]);
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
matchTable[32*nucVal('a') + nucVal('a')] = scaledLogOfFraction(matchProb, freq[A_BASE_VAL]);
matchTable[32*nucVal('a') + nucVal('c')] = scaledLogOfFraction(mismatchProb, freq[C_BASE_VAL]);
matchTable[32*nucVal('a') + nucVal('g')] = scaledLogOfFraction(wobbleProb, freq[G_BASE_VAL]);
matchTable[32*nucVal('a') + nucVal('t')] = scaledLogOfFraction(mismatchProb, freq[T_BASE_VAL]);

matchTable[32*nucVal('c') + nucVal('a')] = scaledLogOfFraction(mismatchProb, freq[A_BASE_VAL]);
matchTable[32*nucVal('c') + nucVal('c')] = scaledLogOfFraction(matchProb, freq[C_BASE_VAL]);
matchTable[32*nucVal('c') + nucVal('g')] = scaledLogOfFraction(mismatchProb, freq[G_BASE_VAL]);
matchTable[32*nucVal('c') + nucVal('t')] = scaledLogOfFraction(wobbleProb, freq[T_BASE_VAL]);

matchTable[32*nucVal('g') + nucVal('a')] = scaledLogOfFraction(wobbleProb, freq[A_BASE_VAL]);
matchTable[32*nucVal('g') + nucVal('c')] = scaledLogOfFraction(mismatchProb, freq[C_BASE_VAL]);
matchTable[32*nucVal('g') + nucVal('g')] = scaledLogOfFraction(matchProb, freq[G_BASE_VAL]);
matchTable[32*nucVal('g') + nucVal('t')] = scaledLogOfFraction(mismatchProb, freq[T_BASE_VAL]);

matchTable[32*nucVal('t') + nucVal('a')] = scaledLogOfFraction(mismatchProb, freq[A_BASE_VAL]);
matchTable[32*nucVal('t') + nucVal('c')] = scaledLogOfFraction(wobbleProb, freq[C_BASE_VAL]);
matchTable[32*nucVal('t') + nucVal('g')] = scaledLogOfFraction(mismatchProb, freq[G_BASE_VAL]);
matchTable[32*nucVal('t') + nucVal('t')] = scaledLogOfFraction(matchProb, freq[T_BASE_VAL]);
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
struct aliMatrix a;
struct aliState *hf, *lf, *iq, *it, *c1, *c2, *c3;
int qIx, tIx, sIx;  /* Query, target, and state indices */
int rowOffset, newCellOffset;
struct aliPair *pairList;
int matchOff, qSlipOff, tSlipOff;
int bestScore = -0x4fffffff;
struct aliCell *bestCell = NULL;
int badScore = -0x3fffffff;
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
initAliMatrix(&a, 7, query, querySize, target, targetSize);
hf = nameState(&a, hiFiIx, "highFi", 'H');
lf = nameState(&a, loFiIx, "lowFi", 'L');
iq = nameState(&a, qSlipIx, "qSlip", 'Q');
it = nameState(&a, tSlipIx, "tSlip", 'T');
c1 = nameState(&a, c1Ix, "frame1", '1');
c2 = nameState(&a, c2Ix, "frame2", '2');
c3 = nameState(&a, c3Ix, "frame3", '3');

qSlipOff = -a.qDim;
tSlipOff = -1;
matchOff = qSlipOff + tSlipOff;

for (tIx = 1; tIx < a.tDim; tIx += 1)
    {
    UBYTE mommy;
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
        mommy = packMommy(state->stateIx, -1, -1); \
        score = tempScore; \
        } \
   } 

/* Define a transition from state while slipping query
 * and advancing target. */
#define qSlipState(state, addScore) \
   { \
   if ((tempScore = state->qSlipScore + addScore) > score) \
        { \
        mommy = packMommy(state->stateIx, 0, -1); \
        score = tempScore; \
        } \
   }

/* Define a transition from state while slipping target
 * and advancing query. */
#define tSlipState(state, addScore) \
   { \
   if ((tempScore = state->tSlipScore + addScore) > score) \
        { \
        mommy = packMommy(state->stateIx, -1, 0); \
        score = tempScore; \
        } \
   }

/* End a block of transitions into state. */
#define endState(state) \
    { \
    struct aliCell *newCell = state->cells + newCellOffset; \
    if (score <= 0) \
        { \
        mommy = nullMommy; \
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
    struct aliCell *newCell = state->cells + newCellOffset; \
    if (score <= 0) \
        { \
        mommy = nullMommy; \
        score = 0; \
        } \
    newCell->mommy = mommy; \
    state->newScore = score; \
    }


    rowOffset = tIx*a.qDim;
    for (qIx = 1; qIx < a.qDim; qIx += 1)
        {
        int qBase = nucVal(a.query[qIx-1]);
        int tBase = nucVal(a.target[tIx-1]);

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
    for (sIx = 0; sIx < a.stateCount; ++sIx)
        {
        struct aliState *as = &a.states[sIx];
        int *swapTemp = as->lastScores;
        as->lastScores = as->scores;
        as->scores = swapTemp;
        }
    }

/* Trace back from best scoring cell. */
pairList = traceBack(&a, bestCell);
printTrace(&a, pairList, TRUE, f, printExtraAtEnds);

slFreeList(&pairList);
cleanupAliMatrix(&a);
return bestScore;
#undef matchScore
#undef qSlipScore
#undef tSlipScore
#undef newScore
}
