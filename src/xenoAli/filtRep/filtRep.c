/* filtRep - filter repeats out of all.st file. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "xa.h"
#include "fuzzyFind.h"


void xaWriteOne(struct xaAli *xa, FILE *f)
/* Write one xa to file. */
{
fprintf(f, "%s align %3.1f%% of %d %s:%d-%d %c %s:%d-%d %c\n",
    xa->name, xa->milliScore*0.1, xa->symCount, 
    xa->query, xa->qStart, xa->qEnd, xa->qStrand,
    xa->target, xa->tStart, xa->tEnd, xa->tStrand);
fprintf(f, "%s\n%s\n%s\n", xa->qSym, xa->tSym, xa->hSym);
}

int matchScores[4];
int mismatchCost = -4;
int shortGapCost = -12;


void initMatchScores()
/* Initialize match scores so G/C is better than A/T */
{
matchScores[A_BASE_VAL] = matchScores[T_BASE_VAL] = 4;
matchScores[G_BASE_VAL] = matchScores[C_BASE_VAL] = 6;
}

int wormXaScore(struct xaAli *xa)
/* Quickly and crudely score an alignment. */
{
int score = 0;
int i;
int symCount = xa->symCount;
boolean lastGap = FALSE;
boolean longGap = FALSE;
char *ts = xa->tSym;
char *qs = xa->qSym;
char t,q;

for (i=0; i<symCount; ++i)
    {
    t = ts[i];
    q = qs[i];
    if (t == '-' || q == '-')
        {
        if (!lastGap)
            {
            lastGap = TRUE;
            longGap = FALSE;
            score += shortGapCost;
            }
        else if (!longGap)
            {
            longGap = TRUE;
            score += shortGapCost;
            }
        }
    else
        {
        lastGap = FALSE;
        if (t == q)
            score += matchScores[ntVal[t]];
        else
            score += mismatchCost;
        }
    }
return score;
}

void ungapTarget(char *in, int inCount, char **retOut, int *retOutCount)
/* Remove '-' characters from in. */
{
int outCount = 0;
char c;
int i;

/* Manage buffer for output. */
static char *outBuf = NULL;
static int outBufSize = 0;
if (outBufSize < inCount+1)
    {
    freeMem(outBuf);
    outBufSize = inCount + 10000;
    outBuf = needMem(outBufSize);
    }
for (i=0; i<inCount; ++i)
    {
    if ((c = in[i]) != '-')
        outBuf[outCount++] = c;
    }
*retOut = outBuf;
*retOutCount = outCount;
}


int filtSub = 0;
int filtPassWayBig = 0;
int filtPassXaScore = 0;
int filtPassPlentyBig = 0;
int filtPassRep = 0;

boolean filterXa(struct xaAli *xa, struct dnaSeq *repList)
/* Returns TRUE if xa should pass through filter.  Filters
 * out weak sequences and those that match repeats. */
{
static const int minXaScore = 100;
static const int plentyBig = 80;
static const int wayPlentyBig = 1000;
static const double tooReppyRatio = 0.75;
int xaScore;
int targetSize;
int maxRepScore;
DNA *target;
boolean isRc;
int score;
struct ffAli *ffAli;
struct dnaSeq *rep;

++filtSub;
if (xa->symCount >= wayPlentyBig)
    return TRUE;
++filtPassWayBig;
//if ((xaScore = wormXaScore(xa)) < minXaScore)
//    return FALSE;
++filtPassXaScore;
ungapTarget(xa->tSym, xa->symCount, &target, &targetSize);
if (targetSize >= plentyBig)
    return TRUE;
++filtPassPlentyBig;
maxRepScore = round(targetSize*tooReppyRatio);
for (rep = repList; rep != NULL; rep = rep->next)
    {
    if (ffFindAndScore(rep->dna, rep->size, target, targetSize, ffTight, &ffAli, &isRc, &score))
        {
        ffFreeAli(&ffAli);
        if (score > maxRepScore)
            return FALSE;
        }
    }
++filtPassRep;
return TRUE;
}

int main(int argc, char *argv[])
{
char *inName = "c:/biodata/cbriggsae/test/unfiltered.st";
char *outName = "c:/biodata/celegans/xeno/cbriggsae/all.st";
char *repName = "c:/biodata/celegans/features/repeats.fa";
FILE *in, *out;
struct xaAli *xa;
int xaCount = 0;
int newXaCount = 0;
struct dnaSeq *repList;

dnaUtilOpen();
initMatchScores();
in = xaOpenVerify(inName);
out = mustOpen(outName, "w");

repList = faReadAllDna(repName);
while ((xa = xaReadNext(in, FALSE)) != NULL)
    {
    if ( ++xaCount % 1000 == 0)
        printf("xa %d\n", xaCount);
    if (filterXa(xa, repList))
        {
        ++newXaCount;
        xaWriteOne(xa, out);
        }
    xaAliFree(xa);
    }

printf("filtSub = %d\n", filtSub);
printf("filtPassWayBig = %d\n", filtPassWayBig);
printf("filtPassXaScore = %d\n", filtPassXaScore);
printf("filtPassPlentyBig = %d\n", filtPassPlentyBig);
printf("filtPassRep = %d\n", filtPassRep);
printf("oldXaCount %d newXaCount %d\n", xaCount, newXaCount);
return 0;
}
