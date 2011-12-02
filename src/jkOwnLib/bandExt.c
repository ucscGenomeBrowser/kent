/* bandExt - banded Smith-Waterman extension of alignments. 
 * An aligner might first find perfectly matching hits of
 * a small size, then extend these hits as far as possible
 * while the sequences perfectly match, then call on routines
 * in this module to do further extensions allowing small
 * gaps and mismatches. 
 *
 * This version of the algorithm uses affine gap scorea and
 * has the neat feature that the band can wander around.  
 * When a score exceeds any previous score, the band will be 
 * recentered around the highest scoring position. */
/* Copyright 2003-5 Jim Kent.  All rights reserved. */

#include "common.h"
#include "dnaseq.h"
#include "axt.h"
#include "fuzzyFind.h"
#include "localmem.h"
#include "bandExt.h"


/* Definitions for traceback byte.  This is encoded as so:
 *     xxxxLUMM
 * where the x's are not used.  The L bit is set if parent of the left insert
 * is also a left insert (otherwise it is a match). Likewise the U bit is set
 * if the parent of an up insert is also an up insert.  The MM bits contain the
 * parent of the match state. */

#define mpMatch 1
#define mpUp 2
#define mpLeft 3
#define mpMask 3
#define upExt (1<<2)
#define lpExt (1<<3)

struct score
/* Info on score in our three states. */
   {
   int match;
   int up;
   int left;
   };

boolean bandExt(boolean global, struct axtScoreScheme *ss, int maxInsert,
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
int i;			/* A humble index or two. */
int *bOffsets = NULL;	/* Offset of top of band. */
UBYTE **parents = NULL;	/* Array of parent positions. */
struct score *curScores = NULL;	/* Scores for current column. */
struct score *prevScores = NULL;	/* Scores for previous column. */
struct score *swapScores;	/* Helps to swap cur & prev column. */
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
int gapOpen = ss->gapOpen;	 /* First base in gap costs this. */
int gapExtend = ss->gapExtend;	 /* Extra bases in gap cost this. */
int badScore = -gapOpen*100;	 /* A score bad enough no one will want to link with us. */
int maxDrop = gapOpen + gapExtend*maxInsert; /* Max score drop allowed before giving up. */
int midScoreOff;		 /* Offset to middle of scoring array. */
struct lm *lm;			 /* Local memory pool. */
boolean didExt = FALSE;
int initGapScore = -gapOpen;

/* For reverse direction just reverse bytes here and there.  It's
 * a lot easier than the alternative and doesn't cost much time in
 * the global scheme of things. */
if (dir < 0)
    {
    reverseBytes(aStart, aSize);
    reverseBytes(bStart, bSize);
    }
#ifdef DEBUG
uglyf("bandExt: dir %d, aStart %d, aSize %d, symAlloc %d\n", dir,
	aSize, bSize, symAlloc);
mustWrite(uglyOut, aStart, aSize);
uglyf("\n");
mustWrite(uglyOut, bStart, bSize);
uglyf("\n");
#endif /* DEBUG */

/* Make up a local mem structure big enough for everything. 
 * This is just a minor, perhaps misguided speed tweak to
 * avoid multiple mallocs. */
lm = lmInit(
    sizeof(bOffsets[0])*aSize +
    sizeof(parents[0])*bandSize +
    bandSize*(sizeof(parents[0][0])*aSize) +
    sizeof(curScores[0]) * bandPlus +
    sizeof(prevScores[0]) * bandPlus );

/* Allocate data structures out of local memory pool. */
lmAllocArray(lm, bOffsets, aSize);
lmAllocArray(lm, parents, bandSize);
for (i=0; i<bandSize; ++i)
    lmAllocArray(lm, parents[i], aSize);
lmAllocArray(lm, curScores, bandPlus);
lmAllocArray(lm, prevScores, bandPlus);

/* Set up scoring arrays so that stuff outside of the band boundary 
 * looks bad.  There will be maxIns+1 of these sentinel values at
 * the start and at the beginning. */
for (i=0; i<bandPlus; ++i)
    {
    struct score *cs = &curScores[i];
    cs->match = cs->up = cs->left = badScore;
    cs = &prevScores[i];
    cs->match = cs->up = cs->left = badScore;
    }

/* Set up scoring array so that extending without an initial insert
 * looks relatively good. */
midScoreOff = 1 + 2 * maxInsert;
prevScores[midScoreOff].match = 0;

/* Set up scoring array so that initial inserts of up to maxInsert
 * are allowed but penalized appropriately. */
    {
    int score = -gapOpen;
    for (i=0; i<maxInsert; ++i)
	{
	prevScores[midScoreOff+i].up = score;
	score -= gapExtend;
	}
    }

for (aPos=0; aPos < aSize; ++aPos)
    {
    char aBase = aStart[aPos];
    int *matRow = ss->matrix[(int)aBase];
    int bestColScore = badScore;
    int bestColPos = -1;
    int colTop = bandCenter - maxInsert;
    int colBottom = bandCenter + maxIns1;

    if (colTop < 0) colTop = 0;
    if (colBottom > bSize) colBottom = bSize;
    curScoreOffset =  maxIns1 + colTop - (bandCenter - maxInsert);
    prevScoreOffset = curScoreOffset + colShift;
#ifdef DEBUG
    uglyf("curScoreOffset %d, maxIns %d, prevScoreOffset %d\n", curScoreOffset, maxInsert, prevScoreOffset);
#endif /* DEBUG */

    /* At top we deal with possibility of initial insert that
     * comes in at this point, unless the band has wandered off. */
    if (aPos < maxInsert)
	{
	curScores[curScoreOffset-1].up = initGapScore;
	initGapScore -= gapExtend;
	}
    else
	curScores[curScoreOffset-1].up = badScore;

    for (bPos = colTop; bPos < colBottom; ++bPos)
	{
	UBYTE parent;
	struct score *curScore = &curScores[curScoreOffset];

#ifdef DEBUG
	uglyf("aPos %d, bPos %d, %c vs %c\n", aPos, bPos, aBase, bStart[bPos]);
	uglyf("  diag[%d %d %d], left[%d %d %d], up[%d %d %d]\n", 
		prevScores[prevScoreOffset-1].match, 
		prevScores[prevScoreOffset-1].left, 
		prevScores[prevScoreOffset-1].up, 
		prevScores[prevScoreOffset].match, 
		prevScores[prevScoreOffset].left, 
		prevScores[prevScoreOffset].up, 
		curScores[curScoreOffset-1].match, 
		curScores[curScoreOffset-1].left, 
		curScores[curScoreOffset-1].up);
#endif /* DEBUG */

	/* Handle ways into the matching state and record if it's
	 * best score so far. */
	    {
	    int match = matRow[(int)bStart[bPos]];
	    struct score *a = &prevScores[prevScoreOffset-1];
	    int diagScore = a->match;
	    int leftScore = a->left;
	    int upScore = a->up;
	    int score;
	    if (diagScore >= leftScore && diagScore >= upScore)
	        {
		score = diagScore + match;
		parent = mpMatch;
		}
	    else if (leftScore > upScore)
	        {
		score = leftScore + match;
	        parent = mpLeft;
		}
	    else
	        {
	        score = upScore + match;
	        parent = mpUp;
	        }
	    curScore->match = score;
	    if (score > bestColScore)
	        {
		bestColScore = score;
		bestColPos = bPos;
		}
	    }

	/* Handle ways into left gap state. */
	    {
	    struct score *a = &prevScores[prevScoreOffset];
	    int extScore = a->left - gapExtend;
	    int openScore = a->match - gapOpen;
	    if (extScore >= openScore)
	        {
		parent |= lpExt;
		curScore->left = extScore;
		}
	    else
	        {
		curScore->left = openScore;
		}
	    }

	/* Handle ways into up gap state. */
	    {
	    struct score *a = &curScore[-1];
	    int extScore = a->up - gapExtend;
	    int openScore = a->match - gapOpen;
	    if (extScore >= openScore)
	        {
		parent |= upExt;
		curScore->up = extScore;
		}
	    else
	        {
		curScore->up = openScore;
		}
	    }

	parents[curScoreOffset - maxIns1][aPos] = parent;

#ifdef DEBUG
	uglyf(" cur [%d %d %d]\n", 
		curScore->match, 
		curScore->left, 
		curScore->up);
#endif /* DEBUG */
	/* Advance to next row in column. */
	curScoreOffset += 1;
	prevScoreOffset += 1;
	}


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
    else if (bestColScore < bestScore - maxDrop)
	{
	if (!global)
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
#ifdef DEBUG
uglyf("traceback: bestScore %d, aBestPos %d, bBestPos %d\n", bestScore, aBestPos, bBestPos);
#endif /* DEBUG */
if (global || bestScore > 0)
    {
    boolean upState = FALSE;
    boolean leftState = FALSE;
    didExt = TRUE;
    if (global)
        {
	aPos = aSize-1;
	bPos = bSize-1;
	}
    else
	{
	aPos = aBestPos;
	bPos = bBestPos;
	}
    for (;;)
	{
	int pOffset;
	UBYTE parent;
	pOffset = bPos - bOffsets[aPos] + maxInsert;
	if (pOffset < 0) pOffset = 0;
	// FIXME: there may be some cases near beginning where
	// pOffset is not right.  Clamping it at 0 prevents a crash
	// and produces correct behavior in many cases.  However
	// I'm thinking a better fix would be to have bOffsets follow
	// colTop rather than bandCenter somehow. Exactly how is
	// beyond me at the moment.
	if (pOffset >= bandSize)
	    {
#ifdef DEBUG
	    uglyf("bPos %d, aPos %d, bOffsets[aPos] %d, maxInsert %d\n", bPos, aPos, bOffsets[aPos], maxInsert);
	    uglyf("pOffset = %d, bandSize = %d\n", pOffset, bandSize);
	    uglyf("aSize %d, bSize %d\n", aSize, bSize);
	    faWriteNext(uglyOut, "uglyA", aStart, aSize);
	    faWriteNext(uglyOut, "uglyB", bStart, bSize);
#endif /* DEBUG */
	    assert(global);
	    return FALSE;
	    }
	parent = parents[pOffset][aPos];
#ifdef DEBUG
	uglyf("aPos %d, bPos %d, parent %d, pMask %d upState %d, leftState %d\n", aPos, bPos, parent, parent & mpMask, upState, leftState);
#endif /* DEBUG */
	if (upState)
	    {
	    retSymA[symCount] = '-';
	    retSymB[symCount] = bStart[bPos];
	    bPos -= 1;
	    upState = (parent&upExt);
	    }
	else if (leftState)
	    {
	    retSymA[symCount] = aStart[aPos];
	    retSymB[symCount] = '-';
	    aPos -= 1;
	    leftState = (parent&lpExt);
	    }
	else
	    {
	    retSymA[symCount] = aStart[aPos];
	    retSymB[symCount] = bStart[bPos];
	    aPos -= 1;
	    bPos -= 1;
	    parent &= mpMask;
	    if (parent == mpUp)
	        upState = TRUE;
	    else if (parent == mpLeft)
	        leftState = TRUE;
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
if (retStartA != NULL) *retStartA = aBestPos;
if (retStartB != NULL) *retStartB = bBestPos;
*retSymCount = symCount;
return didExt;
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

gotExt = bandExt(FALSE, ss, maxInsert, ns, nSize, hs, hSize, dir,
	symAlloc, &symCount, nBuf, hBuf, &nExt, &hExt);
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
    ffList = ffAliFromSym(symCount, nBuf, hBuf, lm,  nExtStart, hExtStart);
    }
freeMem(symBuf);
return ffList;
}

