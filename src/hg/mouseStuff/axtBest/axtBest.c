/* axtBest - Remove second best alignments. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "memalloc.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "axt.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtBest - Remove second best alignments\n"
  "usage:\n"
  "   axtBest in.axt chrom out.axt\n"
  "options:\n"
  "   -winSize=N  - Size of window, default 10000\n"
  "   -minScore=N - Minimum score alignments to consider.  Default 1000\n"
  "   -minOutSize=N - Minimum score of piece to output. Default 10\n"
  "   -matrix=file.mat - override default scoring matrix\n"
  "Alignments scoring over minScore (where each matching base counts\n"
  "about +100 in the default scoring scheme) are projected onto the\n"
  "target sequence.  The score within each overlapping 1000 base window\n"
  "is calculated, and the best scoring alignments in each window are\n"
  "marked.  Alignments that are never the best are thrown out.\n"
  "The best scoring alignment for each window is the output, chopping\n"
  "up alignments if necessary\n");
}

/* Variable that can be set from command line. */
int winSize = 10000;
int minScore = 1000;
int minOutSize = 10;

/* Variables to keep statistics on aligments. */
int writeCount = 0;	/* Keep track of number actually written. */
int subsetCount = 0;    /* Keep track of trimmed number written. */
int baseOutCount = 0;   /* Keep track of target bases written. */
int baseInCount = 0;    /* Keep track of target bases input. */

struct axt *readAllAxt(char *fileName, struct axtScoreScheme *ss, int threshold,
	char *chromName)
/* Read all axt's in a file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct axt *list = NULL, *axt;
while ((axt = axtRead(lf)) != NULL)
    {
    if (!axtCheck(axt, lf))
	{
	axtFree(&axt);
	continue;
	}
    if (sameString(chromName, axt->tName))
	{
	if (axt->tStrand != '+')
	    errAbort("Can't handle minus target strand line %d of %s",
	        lf->lineIx, lf->fileName);
	axt->score = axtScore(axt, ss);
	baseInCount += axt->tEnd - axt->tStart;
	if (axt->score >= threshold)
	    {
	    slAddHead(&list, axt);
	    }
	else
	    axtFree(&axt);
	}
    else
        axtFree(&axt);
    }
slReverse(&list);
return list;
}

struct bestKeep
/* Structure to help find best scoring alignment for a base. */
    {
    int tStart, tEnd;	/* Target range covered */
    int tSize;		/* Size of range. */
    int *scores;	/* Array of scores. */
    struct axt **axts;   /* Array of axts. */
    };

void bestKeepInitScores(struct bestKeep *bk)
/* Initialize scores to all bad. */
{
int tSize = bk->tSize, *scores = bk->scores, i;
for (i=0; i<tSize; ++i)
    scores[i] = -1024*1024;
}

struct bestKeep *bestKeepNew(int tStart, int tEnd)
/* Allocate new bestKeep structure */
{
struct bestKeep *bk;
int size;
AllocVar(bk);
size = tEnd - tStart;
assert(size >= 0);
bk->tStart = tStart;
bk->tEnd = tEnd;
bk->tSize = size;
size += 1;
AllocArray(bk->scores, size);
AllocArray(bk->axts, size);
return bk;
}

void markBest(struct axt *axt, struct bestKeep *bk, 
	int chromStart, int rangeSize, 
	struct axtScoreScheme *ss, int winSize, boolean soft)
/* Figure out alignment score within big window and mark best ones in bk. */
{
int i, s, e, symCount = axt->symCount;
int bIx = axt->tStart - bk->tStart;
char *qSym = axt->qSym, *tSym = axt->tSym;
char q,t;
int score = 0;
boolean lastGap = FALSE;
int gapStart = ss->gapOpen;
int gapExt = ss->gapExtend;
int tSize = axt->tEnd - axt->tStart;
int *tProjScores, *tScore;
int halfWin = winSize/2;
int winHalf = winSize - halfWin;

/* Calculate base-by-base alignment scores into tScore array. */
tScore = AllocArray(tProjScores, tSize);
for (i=0; i<symCount; ++i)
    {
    q = qSym[i];
    t = tSym[i];
    if (q == '-' || t == '-')
        {
	if (lastGap)
	    score -= gapExt;
	else
	    {
	    score -= gapStart;
	    lastGap = TRUE;
	    }
	}
    else
        {
	score += ss->matrix[ntVal5[q]][ntVal5[t]];
	lastGap = FALSE;
	}
    if (score < 0)
        score = 0;
    if (t != '-')
	{
        *tScore++ = score;
	}
    }
assert(tScore == tProjScores + tSize);
tScore = tProjScores;


/* Figure out average score in window around each
 * base in target, and if it's better than the existing
 * best score in that region mark it. */
if (soft)
    {
    /* For soft scoring the edges will score less than the
     * middle. */
    for (i=0; i<tSize; ++i)
	{
	/* Figure out 'average' score per base by subtracting score
	 * a half window back from score a half window ahead. */
	s = i - halfWin;
	if (s < 0) s = 0;
	e = i + winHalf;
	if (e >= tSize) e = tSize-1;
	score = tScore[e] - tScore[s];
	if (score > bk->scores[bIx])
	    {
	    bk->scores[bIx] = score;
	    bk->axts[bIx] = axt;
	    }
	++bIx;
	}
    }
else
    {
    /* For hard scoring the edges will score as well as the 
     * middle. */
    if (tSize < winSize)
        {
	score = tScore[tSize-1];
	for (i=0; i<tSize; ++i)
	    {
	    if (score > bk->scores[bIx])
	        {
		bk->scores[bIx] = score;
		bk->axts[bIx] = axt;
		}
	    ++bIx;
	    }
	}
    else
        {
	int lastHalfWin = tSize - winHalf;
	score = tScore[halfWin-1];
	for (i=0; i<halfWin; ++i)
	    {
	    if (score > bk->scores[bIx])
	        {
		bk->scores[bIx] = score;
		bk->axts[bIx] = axt;
		}
	    ++bIx;
	    }
	for (i=halfWin; i<lastHalfWin; ++i)
	    {
	    score = tScore[i+winHalf] - tScore[i-halfWin];
	    if (score > bk->scores[bIx])
	        {
		bk->scores[bIx] = score;
		bk->axts[bIx] = axt;
		}
	    ++bIx;
	    }
	score = tScore[tSize-1] - tScore[lastHalfWin];
	for (i=lastHalfWin; i<tSize; ++i)
	    {
	    if (score > bk->scores[bIx])
	        {
		bk->scores[bIx] = score;
		bk->axts[bIx] = axt;
		}
	    ++bIx;
	    }
	}
    assert(bIx == axt->tEnd - bk->tStart);
    }
freez(&tScore);
}


void outputBestRanges(struct bestKeep *bk, int chromStart, 
	int rangeSize, struct axtScoreScheme *ss, FILE *f)
/* Find runs inside of bk that refer to same alignment and
 * then output. */
{
int s = 0, i;
struct axt *axt, *lastAxt = NULL;
struct axt **bkAxt = bk->axts;

/* This loop depends on having a sentinal NULL bk->axt at 
 * end of array. */
for (i=0; i<=rangeSize; ++i)
    {
    axt = bkAxt[i];
    if (axt != lastAxt)
        {
	if (lastAxt != NULL)
	    {
	    int tBaseCount = i - s;
	    if (tBaseCount >= minOutSize)
		{
		axtSubsetOnT(lastAxt, s+chromStart, i+chromStart, ss, f);
		++writeCount;
		baseOutCount += tBaseCount;
		}
	    }
	s = i;
	lastAxt = axt;
	}
    }
}


void chromRange(struct axt *axtList, int *retStart, int *retEnd)
/* Return range in chromosome covered. */
{
int maxSize = 0, minSize = BIGNUM;
struct axt *axt;
for (axt = axtList; axt != NULL; axt = axt->next)
    {
    if (axt->tEnd > maxSize)
        maxSize = axt->tEnd;
    if (axt->tStart < minSize)
        minSize = axt->tStart;
    }
*retStart = minSize;
*retEnd = maxSize;
}

void axtBest(char *inName, char *chromName, char *outName)
/* axtBest - Remove second best alignments. */
{
FILE *f = mustOpen(outName, "w");
char *matrixName = optionVal("matrix", NULL);
struct axtScoreScheme *ss = NULL;
struct axt *axtList = NULL, *axt, *nextAxt, *goodList = NULL;
struct bestKeep *bk = NULL;
int i;
int chromStart, chromEnd;
int rangeSize;

if (matrixName == NULL)
    ss = axtScoreSchemeDefault();
else
    ss = axtScoreSchemeRead(matrixName);
axtList = readAllAxt(inName, ss, minScore, chromName);
if (axtList == NULL)
    {
    printf("Empty %s\n", inName);
    return;
    }
printf("Read %d elements from %s\n", slCount(axtList), inName);
chromRange(axtList, &chromStart, &chromEnd);
rangeSize = chromEnd - chromStart;
bk = bestKeepNew(chromStart, chromEnd);
printf("Allocated %d elements in array\n", rangeSize);


/* Find local best in big soft window and get rid of
 * alignments that aren't best anywhere, using axt->score
 * field for scratch space. */
for (axt = axtList; axt != NULL; axt = axt->next)
    {
    markBest(axt, bk, chromStart, rangeSize, ss, winSize, TRUE);
    }
for (axt = axtList; axt != NULL; axt = axt->next)
    axt->score = 0;
for (i=0; i<bk->tSize; ++i)
    {
    if ((axt = bk->axts[i]) != NULL)
        axt->score += 1;
    }
for (axt = axtList; axt != NULL; axt = nextAxt)
    {
    nextAxt = axt->next;
    if (axt->score)
        {
	slAddHead(&goodList, axt);
	}
    }
slReverse(&goodList);
axtList = NULL;
printf("%d elements in after soft filter.\n", slCount(goodList));


/* Find local best alignments in big hard window and
 * output them. */
memset(bk->axts, 0, rangeSize * sizeof(bk->axts[0]));
bestKeepInitScores(bk);
for (axt = goodList; axt != NULL; axt = axt->next)
    markBest(axt, bk, chromStart, rangeSize, ss, winSize, FALSE);
outputBestRanges(bk, chromStart, rangeSize, ss, f);
printf("Output %d alignments including %d trimmed from overlaps\n", writeCount, subsetCount);
printf("Bases in %d, bases out %d (%4.2f%%)\n", baseInCount, baseOutCount,
	100.0 * baseOutCount / baseInCount);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
winSize = optionInt("winSize", winSize);
minScore = optionInt("minScore", minScore);
minOutSize = optionInt("minOutSize", minOutSize);
dnaUtilOpen();
setMaxAlloc(2000000000U);
axtBest(argv[1], argv[2], argv[3]);
return 0;
}
