/* dynAlign.c - align using dynamic programming */
#include "common.h"
#include "cheapcgi.h"
#include "htmshell.h"

boolean isOnWeb;

struct aliCell
/* This is a single cell in a single state of the matrix. */
    {
    struct aliCell *parent;
    int score;
    };

struct aliState
/* This corresponds to a hidden markov state.  Each one of
 * these has a two dimensional array[targetSize+1][querySize+1]
 * of cells. */
    {
    struct aliCell *cells;
    struct aliCell *endCells;
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
    };

void initAliMatrix(struct aliMatrix *am, int stateCount, char *query, char *target)
/* Allocate all memory required for an aliMatrix. Set up dimensions. */
{
int i;
struct aliCell *allCells;
int allCellSize;

zeroBytes(am, sizeof(*am));
am->query = query;
am->target = target;
am->querySize = strlen(query);
am->targetSize = strlen(target);
am->qDim = am->querySize + 1;
am->tDim = am->targetSize + 1;
am->stateCount = stateCount;
am->stateSize = am->qDim * am->tDim;
am->stateByteSize = am->stateSize * sizeof(struct aliCell);
am->states = needMem(stateCount * sizeof(struct aliState));
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
}

void cleanupAliMatrix(struct aliMatrix *am)
/* Free up memory required for an aliMatrix and make sure
 * nobody reuses it. */
{
freeMem(am->states);
freeMem(am->allCells);
zeroBytes(am, sizeof(*am));
}

struct aliState *nameState(struct aliMatrix *am, int stateIx, char *name, char emitLetter)
/* Give a name to a state and return a pointer to it. */
{
struct aliState *state;
assert(stateIx < am->stateCount);
state = &am->states[stateIx];
state->name = name;
state->emitLetter = emitLetter;
return state;
}

void findMatrixIx(struct aliMatrix *am, struct aliCell *cell, int *retStateIx, int *retTix, int *retQix)
/* Given a cell in matrix return state, query, and target index. */
{
int cellIx = cell - am->allCells;
*retStateIx = cellIx/am->stateSize;
cellIx %= am->stateSize;
*retTix = cellIx / am->qDim;
*retQix = cellIx % am->qDim;
}

struct aliCell *bestCellInState(struct aliMatrix *am, struct aliState *state)
/* Return highest scoring cell in a particular state. */
{
struct aliCell *bestCell = NULL, *cell;
int bestScore = -0x7fffffff, score;
struct aliCell *endCells = state->endCells;

for (cell = state->cells + am->qDim; cell < endCells; cell += 1)
    {
    if ((score = cell->score) > bestScore)
        {
        bestScore = score;
        bestCell = cell;
        }
    }
return bestCell;
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

struct aliPair *traceBack(struct aliMatrix *am, struct aliCell *end)
/* Create list of alignment pair by tracing back through matrix from end
 * state back to a start.*/
{
struct aliPair *pairList = NULL, *pair;
struct aliCell *cell, *parent = end;
int parentSix, parentTix, parentQix;
int sIx, tIx, qIx;

findMatrixIx(am, parent, &parentSix, &parentTix, &parentQix);
for (;;)
    {
    cell = parent;
    sIx = parentSix;
    tIx = parentTix;
    qIx = parentQix;

    if ((parent = cell->parent) == NULL)
        break;
    findMatrixIx(am, parent, &parentSix, &parentTix, &parentQix);
    
    AllocVar(pair);
    pair->hiddenIx = (UBYTE)sIx;
    pair->hiddenSym = am->states[parentSix].emitLetter; 

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

void printTrace(struct aliMatrix *am, struct aliPair *pairList, FILE *f)
/* Print out trace to file. */
{
struct aliPair *pair;
#define lineLen 50
char asyms[lineLen+1];
char qsyms[lineLen+1];
char tsyms[lineLen+1];
char hsyms[lineLen+1];
int lineIx = 0;

if ((pair = pairList) == NULL)
    fprintf(f, "Empty pair list");

/* Print out up to 25 bp of initial non-matching parts. */
    {
    int qStart = pair->queryIx;
    int tStart = pair->targetIx;
    int i;

    for (i= -25; i < 0; ++i)
        {
        int qIx = qStart + i;
        int tIx = tStart + i;
        qsyms[lineIx] = (qIx >= 0 ? am->query[qIx] : ' ');
        tsyms[lineIx] = (tIx >= 0 ? am->target[tIx] : ' ' );
        asyms[lineIx] = hsyms[lineIx] = ' ';
        if (qIx >= 0 || tIx >= 0)
            ++lineIx;
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
        fprintf(f, "%s\n%s\n%s\n%s\n\n", qsyms, asyms, tsyms, hsyms);
        lineIx = 0;
        }
    }

/* Print out last bit - first get last pair. */
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

/* Print out anything not printed out yet. */
if (lineIx != 0)
    {
    qsyms[lineIx] = asyms[lineIx] = tsyms[lineIx] = hsyms[lineIx] = 0;
    fprintf(f, "%s\n%s\n%s\n%s\n\n", qsyms, asyms, tsyms, hsyms);
    lineIx = 0;
    }
#undef lineLen
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

#define smallGapCost (8*1024)
#define largeGapOpenCost (12*1024)
#define largeGapExtensionCost (256)
#define matchBonus (1024)
#define mismatchCost (2048)

void dynAlign(char *query, char *target, int blowSome[256][256], FILE *f)
/* Do dynamic programming to make alignment between query and target, and print it to stdout. 
 * At the moment this algorithm is symmetrical - it doesn't matter which sequence is query
 * and which is target. */
{
struct aliMatrix a;
struct aliState *hf, *lf, *iq, *it, *c1, *c2, *c3;
struct aliState *iq;
struct aliState *it;
int qIx, tIx, diag, diagMax, diagDif, qStart, tStart;
int newCellOffset;
int score, tempScore, blowScore;
struct aliCell *cell, *newCell, *parent, *end;
struct aliPair *pairList;

/* Initialize 7 state matrix. */
initAliMatrix(&a, 7, query, target);
hf = nameState(&a, hiFiIx, "highFi", 'H');
lf = nameState(&a, loFoIx, "lowFi", 'L');
iq = nameState(&a, qSlipIx, "qSlip", 'Q');
it = nameState(&a, tSlipIx, "tSlip", 'T');
c1 = nameState(&a, c1Ix, "frame1", '1');
c2 = nameState(&a, c2Ix, "frame2", '2');
c3 = nameState(&a, c3Ix, "frame3", '3');

diagMax = a.querySize + a.targetSize + 1;
for (diag = 2; diag < diagMax; ++diag)
    {
    diagDif = diag - a.querySize;
    if (diagDif > 0)
        {
        qStart = a.querySize;
        tStart = diagDif;
        }
    else
        {                         
        qStart = diag-1;
        tStart = 1;
        }
    for (qIx = qStart, tIx = tStart; qIx >= 0 && tIx < a.tDim; --qIx, ++tIx)
        {
        newCellOffset = tIx*a.qDim + qIx;
        assert(qIx > 0 && tIx > 0)
            { 
            /* Figure the cost or bonus for pairing target and query residue here. */
            blowScore = blowSome[a.query[qIx-1]][a.target[tIx-1]];
    
            /* Update match space. */
                {
                newCell = hf->cells + newCellOffset;

                /* Initially set things to go as if query and target match at this position. */
                parent = cell = newCell - a.qDim - 1;
                score = cell->score + blowScore;

                /* Look what happens if insert a small gap in query. */
                cell = newCell - a.qDim;
                tempScore = cell->score - smallGapCost;
                if (tempScore > score)
                    {
                    score = tempScore;
                    parent = cell;
                    }
        
                /* Look what happens if insert a small gap in target. */
                cell = newCell-1;
                tempScore = cell->score - smallGapCost;
                if (tempScore > score)
                    {
                    score = tempScore;
                    parent = cell;
                    }
            
                /* Look what happens if insert a large gap in query. */
                cell = iq->cells + newCellOffset - a.qDim;
                tempScore = cell->score - largeGapOpenCost;
                if (tempScore > score)
                    {
                    score = tempScore;
                    parent = cell;
                    }
            
                /* Look what happens if insert a large gap in target. */
                cell = it->cells + newCellOffset - 1;
                tempScore = cell->score - largeGapOpenCost;
                if (tempScore > score)
                    {
                    score = tempScore;
                    parent = cell;
                    }

                /* If can't raise positive score start over. */
                if (score <= 0)
                    {
                    parent = NULL;
                    score = 0;
                    }
                newCell->parent = parent;
                newCell->score = score;
                }

            /* Update query slip space. */
                {
                /* See if we can go back into match space. */
                newCell = iq->cells + newCellOffset;
                cell = parent = hf->cells + newCellOffset - a.qDim - 1;
                score = parent->score + blowScore;

                /* Figure out cost of continued business in query slip space. */
                cell = newCell - a.qDim;
                tempScore = cell->score - largeGapExtensionCost;
                if (tempScore > score)
                    {
                    score = tempScore;
                    parent = cell;
                    }                                

                /* If can't raise positive score start over. */
                if (score <= 0)
                    {
                    parent = NULL;
                    score = 0;
                    }
                newCell->parent = parent;
                newCell->score = score;
                }
            
            /* Update target slip space. */
                {
                /* See if we can go back into match space. */
                newCell = it->cells + newCellOffset;
                cell = parent = hf->cells + newCellOffset - a.qDim - 1;
                score = parent->score + blowScore;

                /* Figure out cost of continued business in target slip space. */
                cell = newCell - 1;
                tempScore = cell->score - largeGapExtensionCost;
                if (tempScore > score)
                    {
                    score = tempScore;
                    parent = cell;
                    }                                

                /* If can't raise positive score start over. */
                if (score <= 0)
                    {
                    parent = NULL;
                    score = 0;
                    }
                newCell->parent = parent;
                newCell->score = score;
                }
            }
        }
    }
end = bestCellInState(&a, hf);
pairList = traceBack(&a, end);
printTrace(&a, pairList, stdout);
slFreeList(&pairList);
cleanupAliMatrix(&a);
}

static int blowSome[256][256];  /* If this were proteins it'd be the Blosum matrix. */

void dustData(char *in,char *out)
/* Clean up data by getting rid non-alphabet stuff. */
{
char c;
while ((c = *in++) != 0)
    if (isalpha(c)) *out++ = toupper(c);
*out++ = 0;
}

void doMiddle()
{
char *query = cgiString("query");
char *target = cgiString("target");
dustData(query, query);
dustData(target, target);
printf("<PRE><TT>");
dynAlign(query, target, blowSome, stdout);
}

int main(int argc, char *argv[])
{
char *query, *target;
int i,j;

for (i = 0; i<256; ++i)
    for (j=0; j<256; ++j)
        {
        blowSome[i][j] = -mismatchCost;
        }
for (i=0; i<256; ++i)
    blowSome[i][i] = matchBonus;

if ((isOnWeb = cgiIsOnWeb()) == TRUE)
    {
    htmShell("DynaAlign Results", doMiddle, NULL);
    }
else
    {
    if (argc != 3)
        {
        errAbort("dynAlign - used dynamic programming to find alignment between two strings\n"
                 "usage:\n"
                 "    dynAlign string1 string2");
        }
    query = argv[1];
    target = argv[2];
    dynAlign(query, target, blowSome, stdout);
    }
return 0;
}