/* lumpRep - lump together repeating elements of various sizes
 * and strands into (hopefully) fewer repeats. */
#include "common.h"
#include "dnautil.h"
#include "fuzzyFind.h"
#include "portable.h"

struct rawRep
/* Struct read in from repeat.txt. */
    {
    struct rawRep *next;
    int repCount;
    DNA *seq;
    int seqSize;
    };

struct rawRep *readRawReps(char *fileName, int maxCount)
/* Read in raw repeats from file. */
{
struct rawRep *rrList = NULL, *rr;
FILE *f = mustOpen(fileName, "r");
char line[512];
int lineCount=0;
char *words[3];
int wordCount;
char *seq;

while (fgets(line, sizeof(line), f))
    {
    ++lineCount;
    if (--maxCount < 0)
        break;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;   /* Allow blank lines. */
    if (wordCount != 2 || !isdigit(words[0][0]))
        errAbort("Strange line %d of %s\n", lineCount, fileName);
    AllocVar(rr);
    rr->repCount = atoi(words[0]);
    seq = words[1];
    dnaFilter(seq, seq);
    rr->seqSize = strlen(seq);
    rr->seq = cloneStringZ(seq, rr->seqSize);
    slAddHead(&rrList, rr);
    }
fclose(f);
slReverse(&rrList);
return rrList;
}

struct lumpedRep
/* Struct for lumping together raw repeats. */
    {
    struct lumpedRep *next;
    char *seq;
    int seqSize;
    struct rawRep *repList;
    };

void findSkipEnds(struct ffAli *left, struct ffAli *right, 
    DNA *needle, int needleSize, 
    int *retStartSkip, int *retEndSkip,
    int *retFuseStart, int *retFuseEnd)
/* Figure out how much of either end of needle is skipped before reaching solid alignment. */
{
int minMatch = 6;
int blockSize;


/* Skip over bogus bits on left and store start. */
for ( ; left != NULL; left = left->right)
    {
    blockSize = left->nEnd - left->nStart;
    if (blockSize >= minMatch && ffScoreMatch(left->nStart, left->hStart, blockSize) >= minMatch)
        break;
    }
if (left == NULL)
    *retStartSkip = needleSize;
else
    {
    *retStartSkip = left->nStart - needle;
    *retFuseStart = left->hStart - left->nStart;
    }

/* Skip over bogus bits on right and store end. */
for ( ; right != NULL; right = right->left)
    {
    blockSize = right->nEnd - right->nStart;
    if (blockSize >= minMatch && ffScoreMatch(right->nStart, right->hStart, blockSize) >= minMatch)
        break;
    }
if (right == NULL)
    *retEndSkip = needleSize;
else
    {
    *retEndSkip = needle + needleSize - right->nEnd;
    *retFuseEnd = right->hEnd - right->nEnd;
    }
}

const int endMismatchTolerance = 3;

boolean closestLump(struct lumpedRep *lumpedList, DNA *seq, int seqLen,
    struct lumpedRep **retClosest, int *retSkipStart, int *retSkipEnd,
    int *retFuseStart, int *retFuseEnd, boolean *retIsRc, 
    boolean *retTotalIn, boolean *retTotalOut)
/* Return closest lump in list and information needed to fuse
 * sequence to it. */
{
struct lumpedRep *lump;
struct lumpedRep *bestLump = NULL;
int bestSkipStart, bestSkipEnd;
int bestFuseStart, bestFuseEnd;
int bestScore = 0;   /* If best isn't pretty good we're not interested. */
boolean bestTotalIn, bestTotalOut;
boolean bestIsRc;
int score;
int skipStart, skipEnd;
int fuseStart, fuseEnd;
boolean totalIn, totalOut;

for (lump = lumpedList; lump != NULL; lump = lump->next)
    {
    boolean rc;
    struct ffAli *ffAli;
    int lumpSize = lump->seqSize;

    if (ffFindEitherStrandN(seq, seqLen, lump->seq, lump->seqSize, ffTight, &ffAli, &rc) )
        {
        if (rc)
            reverseComplement(seq, seqLen);
        score = ffScoreCdna(ffAli);
        if (score >= bestScore)
            {
            struct ffAli *right, *left = ffAli;
            int nSize, hSize;

            /* Find right end. */
            for (right = ffAli; right->right != NULL; right = right->right)
                ;
            totalIn = totalOut = FALSE;
            nSize = right->nEnd - left->nStart;
            hSize = right->hEnd - left->hStart;
            if (nSize >= seqLen-endMismatchTolerance)
                totalIn = TRUE;
            if (hSize >= lump->seqSize-endMismatchTolerance && score >= lumpSize)
                totalOut = TRUE;
            findSkipEnds(ffAli, right, seq, seqLen, 
                &skipStart, &skipEnd, &fuseStart, &fuseEnd);
            if (totalIn || totalOut || 
                skipStart <= endMismatchTolerance || skipEnd <= endMismatchTolerance)
                {
                int relScore = ((score * 9 + 4) >> 3);
                if (nSize <= relScore && hSize <= relScore)
                    {
                    bestScore = score;
                    bestLump = lump;
                    bestSkipStart = skipStart;
                    bestSkipEnd = skipEnd;
                    bestFuseStart = fuseStart;
                    bestFuseEnd = fuseEnd;
                    bestIsRc = rc;
                    bestTotalIn = totalIn;
                    bestTotalOut = totalOut;
                    }
                }
            }
        if (rc)
            reverseComplement(seq, seqLen);
        ffFreeAli(&ffAli);
        }
    }
if (bestScore < seqLen/2 && !bestTotalOut)
    bestLump = NULL;
if (bestLump == NULL)
    return FALSE;
else
    {
    *retClosest = bestLump;
    *retSkipStart = bestSkipStart;    
    *retSkipEnd = bestSkipEnd;
    *retFuseStart = bestFuseStart;
    *retFuseEnd = bestFuseEnd;
    *retIsRc = bestIsRc;
    *retTotalIn = bestTotalIn;
    *retTotalOut = bestTotalOut;
    return TRUE;
    }
}

void addToLump(struct lumpedRep *lump, struct rawRep *raw,
    int skipStart, int skipEnd, int fuseStart, int fuseEnd)
/* Add rawRep to lump - fusing at start or end depending on
 * whether skipStart or skipEnd is better. */
{
DNA *n, *h;
int nSize, hSize, newSize;
/* Make sure we have a processing buffer big enough. */
static char *fuseBuf = NULL;
static int fuseAlloced = 0;
int fuseNeeded = lump->seqSize + raw->seqSize + 1;
if (fuseNeeded > fuseAlloced)
    {
    freeMem(fuseBuf);
    fuseAlloced = fuseNeeded*2;
    fuseBuf = needMem(fuseAlloced*sizeof(fuseBuf[0]));
    }

if (skipStart <= skipEnd)
    {
    n = raw->seq + skipStart;
    h = n + fuseStart;
    nSize = raw->seqSize - skipStart;
    hSize = h - lump->seq;
    newSize = nSize + hSize;
    if (newSize > lump->seqSize)
        {
        memcpy(fuseBuf, lump->seq, hSize);
        memcpy(fuseBuf+hSize, n, nSize);
        fuseBuf[newSize] = 0;
        lump->seq = cloneStringZ(fuseBuf, newSize);
        lump->seqSize = newSize;
        }
    }
else
    {
    n = raw->seq + raw->seqSize - skipEnd;
    h = n + fuseEnd;
    nSize = n - raw->seq;
    hSize = lump->seq + lump->seqSize - h;
    assert(nSize >= 0 && hSize >= 0);
    newSize = nSize + hSize;
    if (newSize > lump->seqSize)
        {
        memcpy(fuseBuf, raw->seq, nSize);
        memcpy(fuseBuf+nSize, h, hSize);
        fuseBuf[newSize] = 0;
        lump->seq = cloneStringZ(fuseBuf, newSize);
        lump->seqSize = newSize;
        }    
    }
slAddHead(&lump->repList, raw);
}

struct lumpedRep *lumpRaw(struct rawRep *rawList)
/* Lump together repeats that share sequence. */
{
struct lumpedRep *lumpedList = NULL, *lump;
struct rawRep *raw, *rawNext;
int skipStart, skipEnd, fuseStart, fuseEnd;
boolean isRc, totalIn, totalOut;
int rawCount = slCount(rawList);
int rawIx = 0;
int rawMod = 0;

for (raw = rawList; raw != NULL; raw = rawNext)
    {
    ++rawIx;
    if (++rawMod == 10)
        {
        printf("lumping %d of %d\n", rawIx, rawCount);
        rawMod = 0;
        }
    rawNext = raw->next;
    if (closestLump(lumpedList, raw->seq, raw->seqSize, &lump, 
        &skipStart, &skipEnd, &fuseStart, &fuseEnd, 
        &isRc, &totalIn, &totalOut))
        {
        if (isRc)
            reverseComplement(raw->seq, raw->seqSize);
        if (totalIn)
            {
            slAddHead(&lump->repList, raw);
            }
        else if (totalOut)
            {
            slAddHead(&lump->repList, raw);
            lump->seqSize = raw->seqSize;
            lump->seq = raw->seq;
            }
        else
            {
            addToLump(lump, raw, skipStart, skipEnd, fuseStart, fuseEnd);
            }
        }
    else
        {
        AllocVar(lump);
        lump->seq = raw->seq;
        lump->seqSize = raw->seqSize;
        lump->repList = raw;
        raw->next = NULL;
        slAddHead(&lumpedList, lump);
        }
    }
slReverse(&lumpedList);
return lumpedList;
}

void printLumps(struct lumpedRep *lumpList, FILE *f)
/* Print out lump list to file. */
{
struct lumpedRep *lump;
struct rawRep *raw;

for (lump = lumpList; lump != NULL; lump = lump->next)
    {
    fprintf(f, "%s\n", lump->seq);
    for (raw = lump->repList; raw != NULL; raw = raw->next)
        {
        fprintf(f, "   %d %s\n", raw->repCount, raw->seq);
        }
    }
}

int main(int argc, char *argv[])
{
char *inName = "repeats.in";
char *outName = "repeats.out";
FILE *out;
struct rawRep *rawList;
struct lumpedRep *lumpList;
int rawCount, lumpedCount;
long startTime;

dnaUtilOpen();
rawList = readRawReps(inName, 100000);
rawCount = slCount(rawList);
startTime = clock1000();
lumpList = lumpRaw(rawList);
lumpedCount = slCount(lumpList);
out = mustOpen(outName, "w");
printf("%f seconds to lump %d down to %d\n", (clock1000()-startTime)*0.001, rawCount, lumpedCount);
printLumps(lumpList, out);
return 0;
}
