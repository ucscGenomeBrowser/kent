/* axtBestNear - Remove near best alignments. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "memalloc.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "axt.h"

#define NEARSIZE 2  /* keep this many best hits */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtBestNear - Remove near best alignments\n"
  "usage:\n"
  "   axtBestNear in.axt chrom out.axt\n"
  "options:\n"
  "   -winSize=N  - Size of window, default 10000\n"
  "   -minScore=N - Minimum score alignments to consider.  Default 1000\n"
  "   -minOutSize=N - Minimum score of piece to output. Default 10\n"
  "   -near=N - allow alignments that are within N of the best Default 0.8 \n"
  "   -matrix=file.mat - override default scoring matrix\n"
  "Alignments scoring over minScore (where each matching base counts\n"
  "about +100 in the default scoring scheme) are projected onto the\n"
  "target sequence.  The score within each overlapping 1000 base window\n"
  "is calculated, and the best scoring alignments in each window are\n"
  "marked.  Alignments that are never the best are thrown out.\n"
  "The best scoring alignment for each window is the output, chopping\n"
  "up alignments if necessary\n");
}

struct optionSpec options[] = {
   {"winSize", OPTION_INT},
   {"minScore", OPTION_INT},
   {"minOutSize", OPTION_INT},
   {"near", OPTION_FLOAT},
   {"matrix", OPTION_STRING},
};

/* Variable that can be set from command line. */
int winSize = 10000;
int minScore = 1000;
int minOutSize = 10;
float near = 0.8;

/* Variables to keep statistics on aligments. */
int writeCount = 0;	/* Keep track of number actually written. */
int subsetCount = 0;    /* Keep track of trimmed number written. */
int baseOutCount = 0;   /* Keep track of target bases written. */
int baseInCount = 0;    /* Keep track of target bases input. */
int totalRead = 0;      /* Keep track of number alignments read . */
int readSoft = 0;       /* Keep track of alignments after soft filter . */

/* Variables to buffer input */
struct axt *axtBuffer; /* keep last axt for subsequenct calls to readAxtChunk */
bool axtSaved; /* true if next axt is buffered and should not be read from axtRead */

struct axt *axtReadNext(struct lineFile *lf)
{
if (axtBuffer == NULL || axtSaved == FALSE)
    {
    axtBuffer = axtRead(lf);
    }
else
    axtSaved = FALSE;
return axtBuffer ;
}
struct axt *readAxtChunk(struct lineFile *lf, struct axtScoreScheme *ss, int threshold,
	char *chromName)
/* Read a chunk of overlapping axt's from a file. */
{
struct axt *list = NULL;
int maxT = 0;

while ((axtBuffer = axtReadNext(lf)) != NULL)
    {
    if (!axtCheck(axtBuffer, lf))
	{
	axtFree(&axtBuffer);
	continue;
	}
    assert(axtBuffer != NULL);
    if (sameString(chromName, axtBuffer->tName))
	{
        if (axtBuffer->tStart > maxT && maxT != 0)
            {
            axtSaved = TRUE;
            break;
            }
	if (axtBuffer->tStrand != '+')
	    errAbort("Can't handle minus target strand line %d of %s",
	        lf->lineIx, lf->fileName);
	axtBuffer->score = axtScore(axtBuffer, ss);
	baseInCount += axtBuffer->tEnd - axtBuffer->tStart;
	if (axtBuffer->score >= threshold)
	    {
	    slAddHead(&list, axtBuffer);
            maxT = max(maxT, axtBuffer->tEnd);
	    }
	else
	    axtFree(&axtBuffer);
	}
    else
        axtFree(&axtBuffer);
    }
slReverse(&list);
return list;
}

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
for (i=0; i<tSize*NEARSIZE; ++i)
    scores[i] = -1024*1024;
}

struct bestKeep *bestKeepNew(int tStart, int tEnd)
/* Allocate new bestKeep structure */
{
struct bestKeep *bk;
long size;
int *score;
AllocVar(bk);
size = tEnd - tStart;
assert(size >= 0);
bk->tStart = tStart;
bk->tEnd = tEnd;
bk->tSize = size;
size += 1;
size *= 2; /* keep best and 2nd best */
AllocArray(bk->scores, size);
AllocArray(bk->axts, size);
return bk;
}

void addHit(struct axt *axt, struct bestKeep *bk, int bIx, int score  )
/* add high scoring axt to keeper list, keeps best and near best hits */
{
    int offset = bk->tSize;
if (score > bk->scores[bIx])
    {
    /* copy best score to 2nd best */
    bk->scores[bIx+offset] = bk->scores[bIx];
    bk->axts[bIx+offset] = bk->axts[bIx];
    /* new best score */
    bk->scores[bIx] = score;
    bk->axts[bIx] = axt;
    //printf("yes best %d , near %f bIx %d %s tStart %d 2nd score %d axt %x\n",
    //        score, near , bIx, axt->qName, axt->tStart, bk->scores[bIx+offset], (int)axt);
    }
/* second best */
else if (score > bk->scores[bIx+offset] && axt != bk->axts[bIx])
    {
    bk->scores[bIx+offset] = score;
    bk->axts[bIx+offset] = axt;
    //printf("yes 2nd best %d near %f bIx %d %s tStart %d axt %x\n",
    //        score, near , bIx, axt->qName, axt->tStart, (int)axt);
    }
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
	score += ss->matrix[q][t];
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
	if (score > (bk->scores[bIx] * near))
	    {
            addHit( axt, bk, bIx,score);
	    }
	++bIx;
	}
            //printf("score %s:%d-%d %s:%d-%d score %d best %d bIx %d\n",
            //        axt->tName, axt->tStart, axt->tEnd, axt->qName, axt->qStart, axt->qEnd, score, bk->scores[bIx], bIx);
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
	    if (score > bk->scores[bIx] * near)
	        {
                addHit( axt, bk, bIx,score);
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
	    if (score > bk->scores[bIx] * near)
	        {
                addHit( axt, bk, bIx,score);
		}
	    ++bIx;
	    }
	for (i=halfWin; i<lastHalfWin; ++i)
	    {
	    score = tScore[i+winHalf] - tScore[i-halfWin];
	    if (score > bk->scores[bIx] * near)
	        {
                addHit( axt, bk, bIx,score);
		}
	    ++bIx;
	    }
	score = tScore[tSize-1] - tScore[lastHalfWin];
	for (i=lastHalfWin; i<tSize; ++i)
	    {
	    if (score > bk->scores[bIx] * near)
	        {
                addHit( axt, bk, bIx,score);
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
int s = 0, i, offset;
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
/* output second best hits */
lastAxt = NULL;
offset = rangeSize;
for (i=rangeSize; i<=rangeSize*NEARSIZE; ++i)
    {
    axt = bkAxt[i];
    if (axt != lastAxt)
        {
	if (lastAxt != NULL)
	    {
	    int tBaseCount = i - s;
	    if (tBaseCount >= minOutSize)
		{
                //printf("2nd best out ts %d q %s %d s %d i %d \n ",
                 //       lastAxt->tStart, lastAxt->qName, lastAxt->qStart, s-offset+chromStart, i-offset+chromStart);
		axtSubsetOnT(lastAxt, s-offset+chromStart, i-offset+chromStart, ss, f);
		++writeCount;
		baseOutCount += tBaseCount;
                //printf ("2nd best i %d s %d size %d\n",i-offset,s-offset, i-s);
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

void axtBestNearChunk(struct axt *axtList, char *inName, struct axtScoreScheme *ss, FILE *f)
/* axtBestNear - Remove near best alignments from a list. */
{
struct axt *axt, *nextAxt, *goodList = NULL;
struct bestKeep *bk = NULL;
int i;
int chromStart, chromEnd;
int rangeSize;

if (axtList == NULL)
    {
    printf("Empty %s\n", inName);
    return;
    }
totalRead += slCount(axtList);
chromRange(axtList, &chromStart, &chromEnd);
rangeSize = chromEnd - chromStart;
bk = bestKeepNew(chromStart, chromEnd);
//printf("Allocated %d elements in array\n", rangeSize);


/* Find local best in big soft window and get rid of
 * alignments that aren't best anywhere, using axt->score
 * field for scratch space. */
for (axt = axtList; axt != NULL; axt = axt->next)
    {
    markBest(axt, bk, chromStart, rangeSize, ss, winSize, TRUE);
    }
for (axt = axtList; axt != NULL; axt = axt->next)
    axt->score = 0;
for (i=0; i<bk->tSize*NEARSIZE; ++i)
    {
    if ((axt = bk->axts[i]) != NULL)
        {
        axt->score += 1;
        }
    }
for (axt = axtList; axt != NULL; axt = nextAxt)
    {
    nextAxt = axt->next;
    if (axt->score)
        {
        //    printf("MARKed BEST %s:%d-%d %s:%d-%d\n",axt->tName, axt->tStart, axt->tEnd, axt->qName, axt->qStart, axt->qEnd);

	slAddHead(&goodList, axt);
	}
    }
slReverse(&goodList);
axtList = NULL;
readSoft += slCount(goodList);

//outputBestRanges(bk, chromStart, rangeSize, ss, f);
//fprintf(f,"#=====\n");
/* Find local best alignments in big hard window and
 * output them. */
memset(bk->axts, 0, rangeSize*NEARSIZE* sizeof(bk->axts[0]));
bestKeepInitScores(bk);
for (axt = goodList; axt != NULL; axt = axt->next)
    markBest(axt, bk, chromStart, rangeSize, ss, winSize, FALSE);
outputBestRanges(bk, chromStart, rangeSize, ss, f);
}

void axtBestNear(char *inName, char *chromName, char *outName)
/* axtBestNear - Remove near best alignments. */
{
FILE *f = mustOpen(outName, "w");
char *matrixName = optionVal("matrix", NULL);
struct axtScoreScheme *ss = NULL;
struct axt *axtList = NULL; 
struct lineFile *lf = lineFileOpen(inName, TRUE);

if (matrixName == NULL)
    ss = axtScoreSchemeDefault();
else
    ss = axtScoreSchemeRead(matrixName);
axtBuffer = NULL;
axtSaved = FALSE;
while ((axtList = readAxtChunk(lf, ss, minScore, chromName)) != NULL)
    axtBestNearChunk(axtList, inName, ss, f);
verbose(2,"Read %d elements from %s\n", totalRead, inName);
verbose(2,"%d elements in after soft filter.\n", readSoft);
verbose(2,"Output %d alignments including %d trimmed from overlaps\n", writeCount, subsetCount);
verbose(2,"Bases in %d, bases out %d (%4.2f%%)\n", baseInCount, baseOutCount,
	100.0 * baseOutCount / baseInCount);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
winSize = optionInt("winSize", winSize);
minScore = optionInt("minScore", minScore);
minOutSize = optionInt("minOutSize", minOutSize);
near = optionFloat("near", near);
dnaUtilOpen();
setMaxAlloc(2000000000U);
axtBestNear(argv[1], argv[2], argv[3]);
return 0;
}
