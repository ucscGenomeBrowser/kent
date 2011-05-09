/* dynAlign.c - align using dynamic programming */
#include "common.h"
#include "cheapcgi.h"
#include "htmshell.h"

boolean isOnWeb;

struct dynCell
    {
    struct dynCell *parent;
    int score;
    int qSlipScore;
    int tSlipScore;
    boolean visited;
    };

#define gapOpenCost (4*1024)
#define matchBonus (1024)
#define mismatchCost (1024)

struct aliPair
    {
    struct aliPair *next;
    char querySym;
    char targetSym;
    };

void dumpMatrix(struct dynCell *matrix, int qDim, int tDim, struct dynCell *end)
{
int qIx, tIx;
struct dynCell *cell, *parent;

printf("<A HREF=#end>shortcut to end token</A>\n");
for (qIx = 0; qIx < qDim; ++qIx)
    {
    for (tIx = 0; tIx < tDim; ++tIx)
        {
        cell = &matrix[tIx*qDim + qIx];
        parent = cell->parent;
        if (cell == end)
            uglyf("<A NAME=end></A><span style='color:#FF0000;'>");
        if (parent == NULL)
            uglyf(".");
        else if (parent == cell-1)
            uglyf("^");
        else if (parent == cell-qDim)
            uglyf("<");
        else
            uglyf("\\");
        if (cell == end)
            uglyf("</span>");
        }
    uglyf("\n");
    }
}

void dynAlign(char *query, char *target, int matchMatrix[256][256], int gapFunc[], int gapFuncLen, FILE *f)
/* Do dynamic programming to make alignment between query and target, and print it to stdout. */
{
int querySize = strlen(query);
int targetSize = strlen(target);
int qDim = querySize+1;
int tDim = targetSize+1;
int matrixCellCount = qDim * tDim;
size_t matrixBytes = matrixCellCount * sizeof(struct dynCell);
struct dynCell *matrix = needLargeMem(matrixBytes);
int matrixIx;
int qIx, tIx, diag;
int diagMax = querySize + targetSize + 1;
int badScore = -0x6fffffff;
int tSlipScore, qSlipScore, diagScore;
int score;
int endScore = -1;
struct dynCell *parent, *cell, *newCell, *end = NULL, *start;
struct aliPair *pairList = NULL, *pair;
int qStart, tStart;
int dif;

/* Set first cell to zero */
zeroBytes(matrix, sizeof(matrix[0]));

/* Do Smith-Waterman dynamic programming to find best local alignment. */

/* This combination of diag, qStart, tStart, qIx, and tIx loop control variables
 * is a little complex looking.  All it's doing is letting us go over the rectangle
 * from 0 to querySize by 0 to target diagonally.  If querySize was 5 and targetSize 2
 * we'd traverse the matrix like so:
 *       0 1 2 3 4 5 6
 *       1 2 3 4 5 6 7
 *       2 3 4 5 6 7 8
 */
for (diag = 1; diag < diagMax; ++diag)
    {
    dif = diag - querySize;
    if (dif > 0)
        {
        qStart = querySize;
        tStart = dif;
        }
    else
        {
        qStart = diag;
        tStart = 0;
        }
    for (qIx = qStart, tIx = tStart; qIx >= 0 && tIx < tDim; --qIx, ++tIx)
        {
        newCell = matrix + tIx*qDim + qIx;
        if (qIx > 0 && tIx > 0)
            {
            /* Initially set things to go as if query and target match at this position. */
            parent = cell = newCell - qDim - 1;
            score = diagScore = cell->score + matchMatrix[query[qIx-1]][target[tIx-1]];

            /* Look what happens if insert a gap in query. */
            cell = newCell - qDim;
            qSlipScore = cell->score - gapOpenCost;
            if (qSlipScore > score)
                {
                score = qSlipScore;
                parent = cell;
                }

            /* Look what happens if insert a gap in target. */
            cell = newCell-1;
            tSlipScore = cell->score - gapOpenCost;
            if (tSlipScore > score)
                {
                score = tSlipScore;
                parent = cell;
                }
            /* If can't raise positive score start over. */
            if (score <= 0)
                {
                parent = NULL;
                score = 0;
                }
            /* Keep track of best score. */
            if (score > endScore)
                {
                endScore = score;
                end = newCell;
                }
            newCell->parent = parent;
            newCell->score = score;
            }
        }
    }
start = matrix;

/* Find best scoring position in matrix to use as start. */
parent = end;
for (;;)
    {
    cell = parent;
    if ((parent = cell->parent) == NULL)
        break;
    AllocVar(pair);
    matrixIx = cell-matrix;
    tIx = matrixIx/qDim;
    qIx = matrixIx%qDim;
    if (parent == cell - qDim - 1)  /* Diagonal step */
        {
        pair->querySym = query[qIx-1];
        pair->targetSym = target[tIx-1];
        }
    else if (parent == cell-1)
        {
        pair->querySym = query[qIx-1];
        pair->targetSym = '-';
        }
    else
        {
        assert(parent == cell-qDim);
        pair->querySym = '-';
        pair->targetSym = target[tIx-1];
        }
    slAddHead(&pairList, pair);
    }

printf("%s\n%s\n------------------------------------------------\n", query, target);
for (pair = pairList; pair != NULL; pair = pair->next)
    putc(pair->querySym, f);
putc('\n', f);
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    char c = (pair->targetSym == pair->querySym ? '|' : ' ');
    putc(c, f);
    }
putc('\n', f);
for (pair = pairList; pair != NULL; pair = pair->next)
    putc(pair->targetSym, f);
putc('\n', f);
printf("------------------------------------------------\n");
dumpMatrix(matrix, qDim, tDim, end);
}

static int matchMatrix[256][256];
int gapFuncTable[1024];

void makeGapFuncTable()
{
int i;
int decay = mismatchCost;
int gapCost = gapOpenCost;

gapFuncTable[0] = 0;
gapFuncTable[1] = gapOpenCost;
for (i=2; i<ArraySize(gapFuncTable); ++i)
    {
    gapFuncTable[i] = decay;
    if ((decay >>= 1) == 0)
        decay = 1;
    }
}

void dustData(char *in,char *out)
/* Clean up data by getting rid non-alphabet stuff. */
{
char c;
while ((c = *in++) != 0)
    {
    if (isalpha(c)) *out++ = toupper(c);
    }
*out++ = 0;
}

void doMiddle()
{
char *query = cgiString("query");
char *target = cgiString("target");
dustData(query, query);
dustData(target, target);
printf("<PRE><TT>");
dynAlign(query, target, matchMatrix, gapFuncTable, ArraySize(gapFuncTable), stdout);
}

int main(int argc, char *argv[])
{
char *query, *target;
int i,j;

for (i = 0; i<256; ++i)
    for (j=0; j<256; ++j)
        {
        matchMatrix[i][j] = -mismatchCost;
        }
for (i=0; i<256; ++i)
    matchMatrix[i][i] = matchBonus;
makeGapFuncTable();

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
    dynAlign(query, target, matchMatrix, gapFuncTable, ArraySize(gapFuncTable), stdout);
    }
return 0;
}
