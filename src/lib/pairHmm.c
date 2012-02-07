/* pairHmm - stuff to help implement pairwise hidden markov models,
 * which are useful ways of aligning two sequences. 
 *
 * This file is copyright 2000-2004 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "axt.h"
#include "pairHmm.h"


UBYTE phmmNullMommy = 0; /* mommy value for orphans.... */

void phmmUnpackMommy(UBYTE mommy, int *retStateIx, int *retQoff, 
	int *retToff)
/* Unpack state, query, and target offset. */
{
*retStateIx = (mommy&31);
*retQoff = -((mommy&32)>>5);
*retToff = -((mommy&64)>>6);
}

struct phmmMatrix *phmmMatrixNew(int stateCount,
    char *query, int querySize, char *target, int targetSize)
/* Allocate all memory required for an phmmMatrix. Set up dimensions. */
{
int i;
struct phmmMommy *allCells;
int allCellSize;
int rowSize;
int *allScores;
struct phmmMatrix *am;

AllocVar(am);
am->query = query;
am->target = target;
am->querySize = querySize;
am->targetSize = targetSize;
am->qDim = rowSize = am->querySize + 1;
am->tDim = am->targetSize + 1;
am->stateCount = stateCount;
am->stateSize = rowSize * am->tDim;
am->stateByteSize = am->stateSize * sizeof(struct phmmMommy);
am->states = needMem(stateCount * sizeof(struct phmmState));

/* Initialize matrix of cells for each state. */
allCellSize = stateCount * am->stateByteSize;
am->allCells = allCells = needLargeMem(allCellSize); 
memset(allCells, 0, allCellSize);
for (i=0; i<stateCount; ++i)
    {
    am->states[i].cells = allCells;
    allCells += am->stateSize;
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
return am;
}

void phmmMatrixFree(struct phmmMatrix **pAm)
/* Free up memory required for an phmmMatrix and make sure
 * nobody reuses it. */
{
struct phmmMatrix *am = *pAm;
if (am != NULL)
    {
    freeMem(am->states);
    freeMem(am->allCells);
    freeMem(am->allScores);
    freez(pAm);
    }
}

struct phmmState *phmmNameState(struct phmmMatrix *am, int stateIx, 
	char *name, char emitLetter)
/* Give a name to a state and return a pointer to it. */
{
struct phmmState *state;
assert(stateIx < am->stateCount);
state = &am->states[stateIx];
state->name = name;
state->emitLetter = emitLetter;
return state;
}

static void phmmFindMatrixIx(struct phmmMatrix *am, struct phmmMommy *cell, 
	int *retStateIx, int *retQix, int *retTix)
/* Given a cell in matrix return state, query, and target index. */
{
int cellIx = cell - am->allCells;
*retStateIx = cellIx/am->stateSize;
cellIx %= am->stateSize;
*retTix = cellIx / am->qDim;
*retQix = cellIx % am->qDim;
}

static struct phmmMommy *phmmFindMommy(struct phmmMatrix *am, 
	struct phmmMommy *baby)
/* Find baby's mommy and return it. */
{
int momStateIx, qOff, tOff;
int babyStateIx, qIx, tIx;
UBYTE mommy;

if ((mommy = baby->mommy) == phmmNullMommy)
    return NULL;
phmmUnpackMommy(mommy, &momStateIx, &qOff, &tOff);
phmmFindMatrixIx(am, baby, &babyStateIx, &qIx, &tIx);
return am->states[momStateIx].cells + (tOff + tIx) * am->qDim + (qOff + qIx);
}

struct phmmAliPair
/* Each position in alignment gets one of these. */
    {
    struct phmmAliPair *next;
    int queryIx;
    int targetIx;
    UBYTE hiddenIx;  /* If I code one of these with more than 255 states shoot me! */
    char querySym;
    char targetSym;
    char hiddenSym;
    };

struct phmmAliPair *phmmTraceBack(struct phmmMatrix *am, struct phmmMommy *end)
/* Create list of alignment pair by tracing back through matrix from end
 * state back to a start.*/
{
struct phmmAliPair *pairList = NULL, *pair;
struct phmmMommy *cell, *parent = end;
int parentSix, parentTix, parentQix;
int sIx, tIx, qIx;

phmmFindMatrixIx(am, parent, &parentSix, &parentQix, &parentTix);
for (;;)
    {
    cell = parent;
    sIx = parentSix;
    tIx = parentTix;
    qIx = parentQix;

    if ((parent = phmmFindMommy(am, cell)) == NULL)
        break;
    phmmFindMatrixIx(am, parent, &parentSix, &parentQix, &parentTix);
    
    cell->mommy |= phmmMommyTraceBit;

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

void phmmPrintTrace(struct phmmMatrix *am, struct phmmAliPair *pairList, 
	boolean showStates, FILE *f, boolean extraAtEnds)
/* Print out trace to file. */
{
struct phmmAliPair *pair;
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

struct axt *phhmTraceToAxt(struct phmmMatrix *am, struct phmmAliPair *pairList, 
	int score, char *qName, char *tName)
/* Convert alignment from traceback format to axt. */
{
struct axt *axt;
struct phmmAliPair *pair;
int qEnd, tEnd;
char *qSym, *tSym;
int i;

/* Make sure got something real. */
if ((pair = pairList) == NULL)
    return NULL;

/* Allocate memory for axt. */
AllocVar(axt);
axt->symCount = slCount(pairList);
axt->qSym = AllocArray(qSym, axt->symCount+1);
axt->tSym = AllocArray(tSym, axt->symCount+1);

/* Fill in basic fields. */
axt->qName = cloneString(qName);
axt->tName = cloneString(tName);
axt->qStrand = axt->tStrand = '+';
axt->qStart = qEnd = pair->queryIx;
axt->tStart = tEnd = pair->targetIx;
axt->score = score;

/* Store alignment symbols and keep track of last symbol used. */
for (i=0, pair = pairList; pair != NULL; pair = pair->next, ++i)
    {
    qSym[i] = pair->querySym;
    tSym[i] = pair->targetSym;
    qEnd = pair->queryIx;
    tEnd = pair->targetIx;
    }

/* Store end and return. */
axt->qEnd = qEnd + 1;
axt->tEnd = tEnd + 1;
return axt;
}


