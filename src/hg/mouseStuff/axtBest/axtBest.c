/* axtBest - Remove second best alignments. */
#include "common.h"
#include "linefile.h"
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
  "   -winSize=N  - Size of window, default 1000\n"
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

static int baseVal[256];
/* Table to look up integer values for bases. */

void initBaseVal()
/* Init base val array */
{
int i;
for (i=0; i<ArraySize(baseVal); ++i)
    baseVal[i] = N_BASE_VAL;
baseVal['a'] = baseVal['A'] = A_BASE_VAL;
baseVal['c'] = baseVal['C'] = C_BASE_VAL;
baseVal['g'] = baseVal['G'] = G_BASE_VAL;
baseVal['t'] = baseVal['T'] = T_BASE_VAL;
}

int axtScore(struct axt *axt, struct axtScoreScheme *ss)
/* Return calculated score of axt. */
{
int i, symCount = axt->symCount;
char *qSym = axt->qSym, *tSym = axt->tSym;
char q,t;
int score = 0;
boolean lastGap = FALSE;
int gapStart = ss->gapOpen;
int gapExt = ss->gapExtend;

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
	score += ss->matrix[baseVal[q]][baseVal[t]];
	lastGap = FALSE;
	}
    }
return score;
}

struct axt *readAllAxt(char *fileName, struct axtScoreScheme *ss, int threshold,
	char *chromName)
/* Read all axt's in a file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct axt *list = NULL, *axt;
while ((axt = axtRead(lf)) != NULL)
    {
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

struct bestKeeper
/* Structure to help find best scoring alignment for a base. */
    {
    int score;		/* Best score. */
    struct axt *axt;	/* Best scoring axt. */
    };

void markBest(struct axt *axt, struct bestKeeper *bk, 
	int chromSize, struct axtScoreScheme *ss, int winSize, boolean soft)
/* Figure out alignment score within big window and mark best ones in bk. */
{
struct bestKeeper *b = bk + axt->tStart;
int i, s, e, symCount = axt->symCount;
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
	score += ss->matrix[baseVal[q]][baseVal[t]];
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
	if (score > b->score)
	   {
	   b->score = score;
	   b->axt = axt;
	   }
	b += 1;
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
	    if (score > b->score)
	        {
		b->score = score;
		b->axt = axt;
		}
	    b += 1;
	    }
	}
    else
        {
	int lastHalfWin = tSize - winHalf;
	score = tScore[halfWin-1];
	for (i=0; i<halfWin; ++i)
	    {
	    if (score > b->score)
	        {
		b->score = score;
		b->axt = axt;
		}
	    b += 1;
	    }
	for (i=halfWin; i<lastHalfWin; ++i)
	    {
	    score = tScore[i+winHalf] - tScore[i-halfWin];
	    if (score > b->score)
	        {
		b->score = score;
		b->axt = axt;
		}
	    b += 1;
	    }
	score = tScore[tSize-1] - tScore[lastHalfWin];
	for (i=lastHalfWin; i<tSize; ++i)
	    {
	    if (score > b->score)
	        {
		b->score = score;
		b->axt = axt;
		}
	    b += 1;
	    }
	}
    }
assert(b == bk + axt->tEnd);
freez(&tScore);
}

char *skipIgnoringDash(char *a, int size, bool skipTrailingDash)
/* Count size number of characters, and any 
 * dash characters. */
{
while (size > 0)
    {
    if (*a++ != '-')
        --size;
    }
if (skipTrailingDash)
    while (*a == '-')
       ++a;
return a;
}

int countNonDash(char *a, int size)
/* Count number of non-dash characters. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (a[i] != '-') 
        ++count;
return count;
}

void writeAxtTrackStats(struct axt *axt, FILE *f)
/* Write axt and keep track of some stats. */
{
if (axt->tEnd - axt->tStart >= minOutSize)
    {
    axtWrite(axt, f);
    ++writeCount;
    baseOutCount += axt->tEnd - axt->tStart;
    }
}

void axtSubsetOnT(struct axt *axt, int newStart, int newEnd, 
	struct axtScoreScheme *ss, FILE *f)
/* Write out subset of axt that goes from newStart to newEnd
 * in target coordinates. */
{
if (newStart == axt->tStart && newEnd == axt->tEnd)
    {
    axt->score = axtScore(axt, ss);
    writeAxtTrackStats(axt, f);
    }
else
    {
    struct axt a = *axt;
    char *tSymStart = skipIgnoringDash(a.tSym, newStart - a.tStart, TRUE);
    char *tSymEnd = skipIgnoringDash(tSymStart, newEnd - newStart, FALSE);
    int symCount = tSymEnd - tSymStart;
    char *qSymStart = a.qSym + (tSymStart - a.tSym);
    a.qStart += countNonDash(a.qSym, qSymStart - a.qSym);
    a.qEnd = a.qStart + countNonDash(qSymStart, symCount);
    a.tStart = newStart;
    a.tEnd = newEnd;
    a.symCount = symCount;
    a.qSym = qSymStart;
    a.tSym = tSymStart;
    a.score = axtScore(&a, ss);
    // uglyf("subsetting %s %d %d %s %d %d to %s %d %d, score %d\n", axt->qName, axt->qStart, axt->qEnd, axt->tName, axt->tStart, axt->tEnd, axt->tName, newStart, newEnd, a.score);
    writeAxtTrackStats(&a, f);
    ++subsetCount;
    }
}

void outputBestRanges(struct bestKeeper *bk, int chromSize, 
	struct axtScoreScheme *ss, FILE *f)
/* Find runs inside of bk that refer to same alignment and
 * then output. */
{
int s = 0, i;
struct axt *axt, *lastAxt = NULL;

/* This loop depends on having a sentinal NULL bk->axt at 
 * end of array. */
for (i=0; i<=chromSize; ++i)
    {
    axt = bk[i].axt;
    if (axt != lastAxt)
        {
	if (lastAxt != NULL)
	    {
	    axtSubsetOnT(lastAxt, s, i, ss, f);
	    }
	s = i;
	lastAxt = axt;
	}
    }
}

int maxChromSize(struct axt *axtList)
/* Return largest tEnd. */
{
int maxSize = 0;
struct axt *axt;
for (axt = axtList; axt != NULL; axt = axt->next)
    {
    if (axt->tEnd > maxSize)
        maxSize = axt->tEnd;
    }
return maxSize;
}

void clearBkScores(struct bestKeeper *bk, int size)
/* Set scores in bk to large negative number. */
{
int i;
for (i=0; i<size; ++i)
    bk->score = -1024*1024;
}

void axtBest(char *inName, char *chromName, char *outName)
/* axtBest - Remove second best alignments. */
{
FILE *f = mustOpen(outName, "w");
char *matrixName = optionVal("matrix", NULL);
struct axtScoreScheme *ss = NULL;
struct axt *axtList = NULL, *axt, *nextAxt, *goodList = NULL;
struct bestKeeper *bk = NULL;
int i;
int chromSize;

if (matrixName == NULL)
    ss = axtScoreSchemeDefault();
else
    ss = axtScoreSchemeRead(matrixName);
axtList = readAllAxt(inName, ss, minScore, chromName);
chromSize = maxChromSize(axtList);
printf("Read %d elements from %s\n", slCount(axtList), inName);
AllocArray(bk, chromSize+1);	/* Include extra sentinal value. */
printf("Allocated %d elements in array\n", chromSize);

/* Find local best in big soft window and get rid of
 * alignments that aren't best anywhere, using axt->score
 * field for scratch space. */
clearBkScores(bk, chromSize);
for (axt = axtList; axt != NULL; axt = axt->next)
    {
    markBest(axt, bk, chromSize, ss, winSize, TRUE);
    }
for (axt = axtList; axt != NULL; axt = axt->next)
    axt->score = 0;
for (i=0; i<chromSize; ++i)
    {
    if ((axt = bk[i].axt) != NULL)
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
memset(bk, 0, chromSize * sizeof(*bk));
clearBkScores(bk, chromSize);
for (axt = goodList; axt != NULL; axt = axt->next)
    markBest(axt, bk, chromSize, ss, winSize, FALSE);
outputBestRanges(bk, chromSize, ss, f);
uglyf("Output %d alignments including %d trimmed from overlaps\n", writeCount, subsetCount);
uglyf("Bases in %d, bases out %d (%4.2f%%)\n", baseInCount, baseOutCount,
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
initBaseVal();
axtBest(argv[1], argv[2], argv[3]);
return 0;
}
