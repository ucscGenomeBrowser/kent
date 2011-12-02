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
  "   -verbose=N - Alter verbosity (default 1)\n"
  "   -reads - allow reads to be split across introns but not chromosomes\n"
  "   chrom=all - read all records and group by tName (in.axt must be axtSorted)\n"
  "Alignments scoring over minScore (where each matching base counts\n"
  "about +100 in the default scoring scheme) are projected onto the\n"
  "target sequence.  The score within each overlapping 1000 base window\n"
  "is calculated, and the best scoring alignments in each window are\n"
  "marked.  Alignments that are never the best are thrown out.\n"
  "The best scoring alignment for each window is the output, chopping\n"
  "up alignments if necessary\n"
  "Note: -reads option requires reads to be in tName (preprocess with axtSwap)\n"
  "set the chrom parameter for all for the -reads option.\n"
  );
}

struct optionSpec options[] = {
   {"winSize", OPTION_INT},
   {"minScore", OPTION_INT},
   {"minOutSize", OPTION_INT},
   {"matrix", OPTION_STRING},
   {"reads", OPTION_BOOLEAN},
   {NULL,0}
};

/* Variable that can be set from command line. */
int winSize = 10000;
int minScore = 1000;
int minOutSize = 10;
bool reads = FALSE;

/* Variables to keep statistics on aligments. */
int writeCount = 0;	/* Keep track of number actually written. */
int subsetCount = 0;    /* Keep track of trimmed number written. */
int baseOutCount = 0;   /* Keep track of target bases written. */
int baseInCount = 0;    /* Keep track of target bases input. */
size_t maxAlloc = 2000;    /* Keep track of target bases input. */
char *prevTname = NULL;
struct axt *prevAxt = NULL;

struct axt *readNextTnameList(struct lineFile *lf,  struct axtScoreScheme *ss, int threshold)
/* Read set of axt's with same tName from sorted list */
{
struct axt *list = prevAxt, *axt;
while ((axt = axtRead(lf)) != NULL)
    {
    if (!axtCheck(axt, lf))
	{
	axtFree(&axt);
	continue;
	}
    if (sameString(axt->tName, prevTname) || sameString(prevTname, "first"))
	{
        if (axt->tStrand != '+')
	    errAbort("Can't handle minus target strand line %d of %s",
	        lf->lineIx, lf->fileName);
	axt->score = axtScore(axt, ss);
	baseInCount += axt->tEnd - axt->tStart;
	if (axt->score >= threshold)
	    {
	    slAddHead(&list, axt);
            prevTname = cloneString(axt->tName);
	    }
	else
            {
	    axtFree(&axt);
            }
	}
    else
        {
        prevAxt = axt;
        prevTname = cloneString(axt->tName);
        break;
        }
    }
if (axt == NULL)
    {
    prevAxt = NULL;
    }
slReverse(&list);
return list;
}
struct axt *readAllAxt(char *fileName, struct axtScoreScheme *ss, int threshold,
	char *chromName, FILE *f)
/* Read all axt's in a file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct axt *list = NULL, *axt;
lineFileSetMetaDataOutput(lf, f);
while ((axt = axtRead(lf)) != NULL)
    {
    if (!axtCheck(axt, lf))
	{
	axtFree(&axt);
	continue;
	}
    if (sameString(chromName, axt->tName) || sameString(chromName,"all"))
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

void bestKeepFree(struct bestKeep **pEl)
{
/* #*** NOTE FIXME -- seems like this should free el->axts too.
struct bestKeep *el = *pEl;
*/
freez(pEl);
}

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
	score += ss->matrix[(int)q][(int)t];
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

void axtBestList(struct axt *axtList, char *inName, struct axtScoreScheme *ss, FILE *f )
{
int chromStart, chromEnd;
int rangeSize;
struct bestKeep *bk = NULL;
struct axt *axt, *nextAxt, *goodList = NULL, *readList = NULL;
int i;
if (axtList == NULL)
    {
    verbose(2,"Empty %s\n", inName);
    return;
    }
chromRange(axtList, &chromStart, &chromEnd);
rangeSize = chromEnd - chromStart;
bk = bestKeepNew(chromStart, chromEnd);
verbose(2,"Allocated %d elements in array\n", rangeSize);


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
verbose(2,"%d elements in after soft filter.\n", slCount(goodList));

/* Find local best alignments in big hard window and
 * output them. */
memset(bk->axts, 0, rangeSize * sizeof(bk->axts[0]));
bestKeepInitScores(bk);
/* find top scoring chromosome for reads and only keep
 * reads from that chromosome within max intron size */
if (reads)
    {
    int maxScore = 0;
    char *bestChrom = NULL;
    for (axt = goodList; axt != NULL; axt = axt->next)
        {
        maxScore = max(maxScore,axt->score);
        if (maxScore == axt->score)
            bestChrom = axt->qName;
        }
    for (axt = goodList; axt != NULL; axt = axt->next)
        if (sameString(bestChrom,axt->qName))
            slAddHead(&readList, axt);
    slReverse(&readList);
    goodList = readList;
    }
for (axt = goodList; axt != NULL; axt = axt->next)
    markBest(axt, bk, chromStart, rangeSize, ss, winSize, FALSE);
outputBestRanges(bk, chromStart, rangeSize, ss, f);
bestKeepFree(&bk);
}

void axtBest(char *inName, char *chromName, char *outName)
/* axtBest - Remove second best alignments. */
{
FILE *f = mustOpen(outName, "w");
char *matrixName = optionVal("matrix", NULL);
struct axtScoreScheme *ss = NULL;
struct axt *axtList = NULL;
prevTname = cloneString("first");

if (matrixName == NULL)
    ss = axtScoreSchemeDefault();
else
    ss = axtScoreSchemeRead(matrixName);
axtScoreSchemeDnaWrite(ss, f, "axtBest");
if (sameString(chromName,"all"))
    {
    struct lineFile *lf = lineFileOpen(inName, TRUE);
    lineFileSetMetaDataOutput(lf, f);
    while ((axtList = readNextTnameList(lf, ss, minScore)) != NULL)
        {
        verbose(2,"Read %d elements from %s %s\n", slCount(axtList), inName, axtList->tName);
        axtBestList(axtList, inName, ss, f);
        axtFreeList(&axtList);
        }
    }
else
    {
    axtList = readAllAxt(inName, ss, minScore, chromName, f);
    verbose(2,"Read %d elements from %s\n", slCount(axtList), inName);
    axtBestList(axtList, inName, ss, f);
    }
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
maxAlloc = optionInt("maxAlloc", maxAlloc)*1024*1024;
reads = optionExists("reads");
dnaUtilOpen();
setMaxAlloc(maxAlloc);
axtBest(argv[1], argv[2], argv[3]);
return 0;
}
