/* bandExt - banded Smith-Waterman extension of alignments. 
 * An aligner might first find perfectly matching hits of
 * a small size, then extend these hits as far as possible
 * while the sequences perfectly match, then call on routines
 * in this module to do further extensions allowing small
 * gaps and mismatches. */

#include "common.h"
#include "dnaSeq.h"
#include "axt.h"
#include "fuzzyFind.h"
#include "localmem.h"
#include "bandExt.h"

enum parentPos
/* Record of parant positions. */
   {
   ppNone,	/* Starting fresh from here. */
   ppUp,	/* Parent is up in graph (corresponds to insert) */
   ppDiag,	/* Parent is diagonal in graph (corresponds to match) */
   ppLeft,	/* Parent is left in graph (corresponds to insert) */
   };

boolean bandExt(struct axtScoreScheme *ss, int maxInsert,
	char *aStart, int aSize, char *bStart, int bSize, int dir,
	int symAlloc, int *retSymCount, char *retSymA, char *retSymB, 
	int *retStartA, int *retStartB)
/* Try to extend an alignment from aStart/bStart onwards.
 * Set maxInsert to the maximum gap size allowed.  3 is often
 * a good choice.  aStart/aSize bStart/bSize describe the
 * sequence to extend through (not including any of the existing
 * alignment. Set dir = 1 for forward extension, dir = -1 for backwards. 
 * retSymA and retSymB should point to arrays of characters of
 * symAlloc size.  symAlloc needs to be aSize*2 or so.  The
 * alignment is returned in the various ret values.  The function
 * overall returns TRUE if an extension occurred, FALSE if not. */
{
int i;			/* A humble index. */
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
int symCount = 0;	/* Size of alignment and size allocated for it. */
int gapPenalty = ss->gapOpen;	 /* Each gap costs us this. */
int badScore = -gapPenalty*10;	 /* A score bad enough no one will want to link with us. */
int maxDrop = -gapPenalty*maxInsert; /* Max score drop allowed before giving up. */
int score;			 /* Current score. */
int midScoreOff;		 /* Offset to middle of scoring array. */
struct lm *lm;			 /* Local memory pool. */
boolean didExt = FALSE;

/* For reverse direction just reverse bytes here and there.  It's
 * a lot easier than the alternative and doesn't cost much time in
 * the global scheme of things. */
if (dir < 0)
    {
    reverseBytes(aStart, aSize);
    reverseBytes(bStart, bSize);
    }

/* Make up a local mem structure big enough for everything. */
lm = lmInit(aSize*(bandSize+sizeof(*bOffsets)) + bandPlus*(4*sizeof(int)));

/* Allocate data structures out of local memory pool. */
lmAllocArray(lm, bOffsets, aSize);
lmAllocArray(lm, parents, bandSize);
for (i=0; i<bandSize; ++i)
    {
    lmAllocArray(lm, parents[i], aSize);
    }
lmAllocArray(lm, curScores, bandPlus);
lmAllocArray(lm, prevScores, bandPlus);

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

#ifdef DEBUG
    /* Print previous score array. */
    printf("%c ", '@');
    for (i=0; i<bandPlus; ++i)
	{
	printf("%3d ", prevScores[i]);
	}
    printf("(%d)\n", -colShift);
#endif /* DEBUG */

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

#ifdef DEBUG
    /* Print current score array. */
    printf("%c ", aBase);
    for (i=0; i<bandPlus; ++i)
	{
	printf("%3d ", curScores[i]);
	}
    printf("(%d)\n", bandCenter);
#endif /* DEBUG */

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


/* Trace back. */
if (bestScore > 0)
    {
    didExt = TRUE;
    aPos = aBestPos;
    bPos = bBestPos;
    for (;;)
	{
	int pOffset = bPos - bOffsets[aPos] + maxInsert;
	UBYTE parent = parents[pOffset][aPos];
	switch (parent)
	    {
	    case ppDiag:
		retSymA[symCount] = aStart[aPos];
		retSymB[symCount] = bStart[bPos];
		aPos -= 1;
		bPos -= 1;
		break;
	    case ppUp:
		retSymA[symCount] = '-';
		retSymB[symCount] = bStart[bPos];
		bPos -= 1;
		break;
	    case ppLeft:
		retSymA[symCount] = aStart[aPos];
		retSymB[symCount] = '-';
		aPos -= 1;
		break;
	    }
	if (++symCount >= symAlloc)
	    errAbort("unsufficient symAlloc in bandExt");
	if (aPos < 0 || bPos < 0)
	    {
	    while (aPos >= 0)
		{
		retSymA[symCount] = aStart[aPos];
		retSymB[symCount] = '-';
		if (++symCount >= symAlloc)
		    errAbort("unsufficient symAlloc in bandExt");
		--aPos;
		}
	    while (bPos >= 0)
		{
		retSymA[symCount] = '-';
		retSymB[symCount] = bStart[bPos];
		if (++symCount >= symAlloc)
		    errAbort("unsufficient symAlloc in bandExt");
		--bPos;
		}
	    break;
	    }
	}
    if (dir > 0)
	{
	reverseBytes(retSymA, symCount);
	reverseBytes(retSymB, symCount);
	}
    retSymA[symCount] = 0;
    retSymB[symCount] = 0;
    }
else
    {
    retSymA[0] = retSymB[0] = 0;
    }

if (dir < 0)
    {
    reverseBytes(aStart, aSize);
    reverseBytes(bStart, bSize);
    }

/* Clean up, set return values and go home */
lmCleanup(&lm);
*retStartA = aBestPos;
*retStartB = bBestPos;
*retSymCount = symCount;
return didExt;
}

static struct ffAli *symToFfAli(int symCount, char *nSym, char *hSym,
	struct lm *lm, char *nStart, char *hStart)
/* Convert symbol representation of alignments (letters plus '-')
 * to ffAli representation.  If lm is nonNULL, ffAli result 
 * will be lmAlloced, else it will be needMemed. This routine
 * depends on nSym/hSym being zero terminated. */
{
struct ffAli *ffList = NULL, *ff = NULL;
char n, h;
int i;

for (i=0; i<=symCount; ++i)
    {
    boolean isGap;
    n = nSym[i];
    h = hSym[i];
    isGap = (n == '-' || n == 0 || h == '-' || h == 0);
    if (isGap)
	{
	if (ff != NULL)
	    {
	    ff->nEnd = nStart;
	    ff->hEnd = hStart;
	    ff->left = ffList;
	    ffList = ff;
	    ff = NULL;
	    }
	}
    else
	{
	if (ff == NULL)
	    {
	    if (lm != NULL)
		{
		lmAllocVar(lm, ff);
		}
	    else
		{
		AllocVar(ff);
		}
	    ff->nStart = nStart;
	    ff->hStart = hStart;
	    }
	}
    if (n != '-')
	{
	++nStart;
	}
    if (h != '-')
	{
	++hStart;
	}
    }
ffList = ffMakeRightLinks(ffList);
return ffList;
}

struct ffAli *bandExtFf(
	struct lm *lm,	/* Local memory pool, NULL to use global allocation for ff */
	struct axtScoreScheme *ss, 	/* Scoring scheme to use. */
	int maxInsert,			/* Maximum number of inserts to allow. */
	struct ffAli *origFf,		/* Alignment block to extend. */
	char *nStart, char *nEnd,	/* Bounds of region to extend through */
	char *hStart, char *hEnd,	/* Bounds of region to extend through */
	int dir,			/* +1 to extend end, -1 to extend start */
	int maxExt)			/* Maximum length of extension. */
/* Extend a gapless alignment in one direction.  Returns extending
 * ffAlis, not linked into origFf, or NULL if no extension possible. */
{
int symAlloc = 2*maxExt;
char *symBuf = needMem(4*maxExt);
char *nBuf = symBuf;
char *hBuf = symBuf + symAlloc;
char *ns, *hs;
int symCount, nExt, hExt;
int nSize, hSize;
struct ffAli *ffList = NULL;
boolean gotExt;

if (dir > 0)
    {
    nSize = nEnd - origFf->nEnd;
    hSize = hEnd - origFf->hEnd;
    if (nSize > maxExt) nSize = maxExt;
    if (hSize > maxExt) hSize = maxExt;
    ns = origFf->nEnd;
    hs = origFf->hEnd;
    }
else
    {
    nSize = origFf->nStart - nStart;
    hSize = origFf->hStart - hStart;
    if (nSize > maxExt) nSize = maxExt;
    if (hSize > maxExt) hSize = maxExt;
    ns = origFf->nStart - nSize;
    hs = origFf->hStart - hSize;
    }

gotExt = bandExt(ss, maxInsert, ns, nSize, hs, hSize, dir,
	symAlloc, &symCount, nBuf, hBuf, &nExt, &hExt);
uglyf("nExt %d, hExt %d\n", nExt, hExt);
if (gotExt)
    {
    char *nExtStart, *hExtStart;
    if (dir > 0)
	{
	nExtStart = ns;
	hExtStart = hs;
	}
    else
	{
	nExtStart = origFf->nStart - nExt - 1;
	hExtStart = origFf->hStart - hExt - 1;
	}
    ffList = symToFfAli(symCount, nBuf, hBuf, lm,  nExtStart, hExtStart);
    }
freeMem(symBuf);
return ffList;
}

#ifdef TEST_ONLY

void testBandExt(char *scoreFile, char *aFile, char *bFile)
/* test - Test something. */
{
struct axtScoreScheme *ss = axtScoreSchemeRead(scoreFile);
struct dnaSeq *aSeq = faReadDna(aFile);
struct dnaSeq *bSeq = faReadDna(bFile);
int symAlloc = aSeq->size*2;
char *aSym = needMem(symAlloc);
char *bSym = needMem(symAlloc);
int aExt, bExt, symCount;
boolean gotExt;

printf("%s %s %d\n", aFile, aSeq->name, aSeq->size);
printf("%s %s %d\n", bFile, bSeq->name, bSeq->size);
gotExt = bandExt(ss, clMaxInsert, aSeq->dna, aSeq->size, bSeq->dna, bSeq->size, clDir,
	symAlloc, &symCount, aSym, bSym, &aExt, &bExt);
printf("%sextended to %d %d\n", (gotExt ? "" : "not "), aExt, bExt);
if (gotExt)
    printf("%s\n%s\n", aSym, bSym);
}

void test(char *scoreFile, char *aFile, char *bFile)
/* test - Test something. */
{
struct axtScoreScheme *ss = axtScoreSchemeRead(scoreFile);
struct dnaSeq *aSeq = faReadDna(aFile);
struct dnaSeq *bSeq = faReadDna(bFile);
struct ffAli *ffList, *ff, *origFf;

printf("%s %s %d\n", aFile, aSeq->name, aSeq->size);
printf("%s %s %d\n", bFile, bSeq->name, bSeq->size);
AllocVar(origFf);
if (clDir > 0)
    {
    origFf->nStart = origFf->nEnd = aSeq->dna;
    origFf->hStart = origFf->hEnd = bSeq->dna;
    }
else
    {
    origFf->nStart = origFf->nEnd = aSeq->dna + aSeq->size;
    origFf->hStart = origFf->hEnd = bSeq->dna + bSeq->size;
    }
ffList = bandExtFf(NULL, ss, clMaxInsert, origFf,
	aSeq->dna, aSeq->dna + aSeq->size,
	bSeq->dna, bSeq->dna + bSeq->size,
	clDir, 50);
for (ff = ffList; ff != NULL; ff = ff->right)
    {
    mustWrite(stdout, ff->nStart, ff->nEnd - ff->nStart);
    fprintf(stdout, " %d\n", ff->nEnd - aSeq->dna);
    mustWrite(stdout, ff->hStart, ff->hEnd - ff->hStart);
    fprintf(stdout, " %d\n\n", ff->hEnd - bSeq->dna);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
clMaxInsert = optionInt("maxInsert", clMaxInsert);
clDir = optionInt("dir", clDir);
test(argv[1], argv[2], argv[3]);
return 0;
}

#endif /* TEST_ONLY */
