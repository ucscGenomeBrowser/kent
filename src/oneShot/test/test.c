/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"
#include "axt.h"

int clMaxInsert = 3;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test cnda.ss a.fa b.fa\n"
  "options:\n"
  "   -maxInsert=N Width of band on either side, default %d\n"
  ,	clMaxInsert
  );
}

enum parentPos
/* Record of parant positions. */
   {
   ppNone,	/* Starting fresh from here. */
   ppUp,	/* Parent is up in graph (corresponds to insert) */
   ppDiag,	/* Parent is diagonal in graph (corresponds to match) */
   ppLeft,	/* Parent is left in graph (corresponds to insert) */
   };

void bandAli(struct axtScoreScheme *ss, int maxInsert,
	char *aStart, int aSize, char *bStart, int bSize)
/* Try to align a to b forcing the first letter of a and
 * b to align. */
{
int *bOffsets = NULL;	/* Offset of top of band. */
UBYTE **parents = NULL;	/* Array of parent positions. */
int *curScores = NULL;	/* Scores for current column. */
int *prevScores = NULL;	/* Scores for previous column. */
int *swapScores;	/* Helps to swap cur & prev column. */
int bandSize = 2*maxInsert + 1;	 /* Size of band including middle */
int maxIns1 = maxInsert + 1;   	 /* Max insert plus one. */
int bandPlus = bandSize + 2*maxIns1;  /* Band plus sentinels on either end. */
int bestScore = 0;	 	 /* Best score so far. */
int aPos, aBestPos = -1;	 /* Current and best position in a. */
int bPos, bBestPos = -1;	 /* Current and best position in b. */
int bandCenter = 0;	/* b Coordinate of current band center. */
int colShift = 1;	/* Vertical shift between this column and previous. */
int prevScoreOffset;	/* Offset into previous score column. */
int curScoreOffset;	/* Offset into current score column. */
int i;
char *aSym, *bSym;
int symCount = 0, symAlloc;
int gapPenalty = ss->gapOpen;
int badScore = -gapPenalty*10;	 /* A score bad enough noone will want to link with us. */
int maxDrop = -gapPenalty*maxInsert; /* Max score drop allowed before giving up. */
int score;
int midScoreOff;


/* Allocate data structures. */
AllocArray(bOffsets, aSize);
AllocArray(parents, bandSize);
for (i=0; i<bandSize; ++i)
    {
    AllocArray(parents[i], aSize);
    }
AllocArray(curScores, bandPlus);
AllocArray(prevScores, bandPlus);
symAlloc = 2*aSize;
AllocArray(aSym, symAlloc);
AllocArray(bSym, symAlloc);

/* Set up scoring arrays so that stuff outside of the band boundary 
 * looks bad.  There will be maxIns+1 of these sentinel values at
 * the start and at the beginning. */
for (i=0; i<bandPlus; ++i)
    {
    curScores[i] = prevScores[i] = badScore;
    }

/* Set up scoring array so that extending without an initial insert
 * looks relatively good. */
midScoreOff = 1 + 2 * maxInsert;
prevScores[midScoreOff] = 0;

/* Set up scoring array so that initial inserts of up to maxInsert
 * are allowed but penalized appropriately. */
score = 0;
for (i=1; i<=maxInsert; ++i)
    {
    score -= gapPenalty;
    curScores[midScoreOff-i] = prevScores[midScoreOff-i] 
	    = prevScores[midScoreOff+i] = score;
    }

    /* Print previous score array. */
    printf("%c ", '@');
    for (i=0; i<bandPlus; ++i)
	{
	printf("%3d ", prevScores[i]);
	}
    printf("(%d)\n", -colShift);


for (aPos=0; aPos < aSize; ++aPos)
    {
    char aBase = aStart[aPos];
    int *matRow = ss->matrix[aBase];
    int bestColScore = badScore;
    int bestColPos = -1;
    int curColOffset, prevColOffset;
    int colTop = bandCenter - maxInsert;
    int colBottom = bandCenter + maxIns1;

    if (colTop < 0) colTop = 0;
    if (colBottom > bSize) colBottom = bSize;
    curScoreOffset =  maxIns1 + colTop - (bandCenter - maxInsert);
    prevScoreOffset = curScoreOffset + colShift;
    for (bPos = colTop; bPos < colBottom; ++bPos)
	{
	/* Initialize parent and score to match (diagonal) */
	UBYTE parent = ppDiag;
	int score = prevScores[prevScoreOffset-1] + matRow[bStart[bPos]];
	int s;

	/* Try insert in one direction and record if better. */
	s = prevScores[prevScoreOffset] - gapPenalty;
	if (s > score)
	    {
	    score = s;
	    parent = ppLeft;
	    }

	/* Try insert in other direction and record if better. */
	s = curScores[curScoreOffset-1] - gapPenalty;
	if (s > score)
	    {
	    score = s;
	    parent = ppUp;
	    }

	/* Record best direction and score, noting if it is best in column. */
	curScores[curScoreOffset] = score;
	parents[curScoreOffset - maxIns1][aPos] = parent;
	if (score > bestColScore)
	    {
	    bestColScore = score;
	    bestColPos = bPos;
	    }

	/* Advance to next row in column. */
	curScoreOffset += 1;
	prevScoreOffset += 1;
	}

    /* Print current score array. */
    printf("%c ", aBase);
    for (i=0; i<bandPlus; ++i)
	{
	printf("%3d ", curScores[i]);
	}
    printf("(%d)\n", bandCenter);

    /* If this column's score is best so far make note of
     * it and shift things so that the matching bases at the
     * best score are centered in the band.  This allows the
     * band to wander where appropriate.  Having the band shift
     * to the best column position if it is not the best score
     * so far doesn't work so well though.  */
    if (bestScore < bestColScore)
	{
	bestScore = bestColScore;
	aBestPos = aPos;
	bBestPos = bestColPos;
	colShift = (bestColPos + 1) - bandCenter;
	}
    else if (bestColScore < maxDrop)
	{
	break;
	}
    else
	{
	colShift = 1;
	}

    /* Keep track of vertical offset of this column, and
     * set up offset of next column. */
    bOffsets[aPos] = bandCenter;
    bandCenter += colShift;

    /* Swap scoring arrays so current becomes next iteration's previous. */
    swapScores = curScores;
    curScores = prevScores;
    prevScores = swapScores;
    }

printf("Best score %d ends at %d %d\n", bestScore, aBestPos, bBestPos);

/* Trace back. */
if (bestScore > 0)
    {
    aPos = aBestPos;
    bPos = bBestPos;
    for (;;)
	{
	int pOffset = bPos - bOffsets[aPos] + maxInsert;
	UBYTE parent = parents[pOffset][aPos];
	switch (parent)
	    {
	    case ppDiag:
		aSym[symCount] = aStart[aPos];
		bSym[symCount] = bStart[bPos];
		aPos -= 1;
		bPos -= 1;
		break;
	    case ppUp:
		aSym[symCount] = '-';
		bSym[symCount] = bStart[bPos];
		bPos -= 1;
		break;
	    case ppLeft:
		aSym[symCount] = aStart[aPos];
		bSym[symCount] = '-';
		aPos -= 1;
		break;
	    }
	++symCount;
	if (aPos < 0 || bPos < 0)
	    {
	    while (aPos >= 0)
		{
		aSym[symCount] = aStart[aPos];
		bSym[symCount] = '-';
		++symCount;
		--aPos;
		}
	    while (bPos >= 0)
		{
		aSym[symCount] = '-';
		bSym[symCount] = bStart[bPos];
		++symCount;
		--bPos;
		}
	    break;
	    }
	}
    reverseBytes(aSym, symCount);
    aSym[symCount] = 0;
    reverseBytes(bSym, symCount);
    bSym[symCount] = 0;
    printf("aPos %d, bPos %d\n", aPos, bPos);
    printf("%s\n%s\n", aSym, bSym);
    }
else
    {
    printf("No extension.\n");
    }

/* Clean up */
freeMem(bSym);
freeMem(aSym);
freeMem(prevScores);
freeMem(curScores);
if (parents != NULL)
    {
    for (i=0; i<bandSize; ++i)
	 freeMem(parents[i]);
    freeMem(parents);
    }
freeMem(bOffsets);
}


void test(char *scoreFile, char *aFile, char *bFile)
/* test - Test something. */
{
struct axtScoreScheme *ss = axtScoreSchemeRead(scoreFile);
struct dnaSeq *aSeq = faReadDna(aFile);
struct dnaSeq *bSeq = faReadDna(bFile);
printf("%s %s %d\n", aFile, aSeq->name, aSeq->size);
printf("%s %s %d\n", bFile, bSeq->name, bSeq->size);
bandAli(ss, clMaxInsert, aSeq->dna, aSeq->size, bSeq->dna, bSeq->size);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
clMaxInsert = optionInt("maxInsert", clMaxInsert);
test(argv[1], argv[2], argv[3]);
return 0;
}
