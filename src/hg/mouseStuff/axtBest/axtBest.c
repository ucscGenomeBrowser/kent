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
  "   axtBest in.axt chrom chromSize out.axt\n"
  "options:\n"
  "   -softWin=N  - Size of soft window, default 10000\n"
  "   -minWin=N  - Size of hard window.  Default 34\n"
  "   -matrix=file.mat - override default scoring matrix\n"
  "Alignments are first scored using a soft score that\n"
  "averages score of a large surrounding window.  Alignments\n"
  "that are never the best in such a window are thrown out.\n"
  "Alignments are then scored in a small hard window, and\n"
  "alignments that are the best in such a window are output.\n"
  );
}

/* Variable that can be set from command line. */
int softWin = 10000;
int minWin = 100;

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

struct axt *axtReadAll(char *fileName, char *chromName, int chromSize)
/* Read all axt's in a file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct axt *list = NULL, *el;
while ((el = axtRead(lf)) != NULL)
    {
    if (sameString(chromName, el->tName))
	{
	if (el->tEnd > chromSize)
	    errAbort("tEnd > chromSize (%d > %d) line %d of %s",
	    	el->tEnd, chromSize, lf->lineIx, lf->fileName);
	if (el->tStrand != '+')
	    errAbort("Can't handle minus target strand line %d of %s",
	        lf->lineIx, lf->fileName);
	slAddHead(&list, el);
	}
    else
        axtFree(&el);
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
uglyf("markBest(%s %d %d, %s %d %d)\n", axt->tName, axt->tStart, axt->tEnd, axt->qName, axt->qStart, axt->qEnd);
uglyf("%s\n%s\n", axt->tSym, axt->qSym);
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
	uglyf("%2d %c %c %4d %4d\n", i, tSym[i], qSym[i], tScore[i], score);
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
	    uglyf("%2d %c %c %4d %4d\n", i, tSym[i], qSym[i], tScore[i], score);
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
	    uglyf("%2d %c %c %4d %4d\n", i, tSym[i], qSym[i], tScore[i], score);
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
	    uglyf("%2d %c %c %4d %4d\n", i, tSym[i], qSym[i], tScore[i], score);
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
	    uglyf("%2d %c %c %4d %4d\n", i, tSym[i], qSym[i], tScore[i], score);
	    }
	}
    }
assert(b == bk + axt->tEnd);
freez(&tScore);
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

void axtSubsetOnT(struct axt *axt, int newStart, int newEnd, 
	struct axtScoreScheme *ss, FILE *f)
/* Write out subset of axt that goes from newStart to newEnd
 * in target coordinates. */
{
uglyf("axtSubsetOnT(%s %d %d, %s %d %d -> %s %d %d)\n",
	axt->qName, axt->qStart, axt->qEnd,
	axt->tName, axt->tStart, axt->tEnd,
	axt->tName, newStart, newEnd);
if (newStart == axt->tStart && newEnd == axt->tEnd)
    {
    axt->score = axtScore(axt, ss);
    axtWrite(axt, f);
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
    axtWrite(&a, f);
    }
}

void outputBestRanges(struct bestKeeper *bk, int chromSize, 
	struct axtScoreScheme *ss, int minSize, FILE *f)
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
	    int size = i - s;
	    if (size > minSize)
		axtSubsetOnT(lastAxt, s, i, ss, f);
	    }
	s = i;
	lastAxt = axt;
	}
    }
}

void axtBest(char *inName, char *chromName, int chromSize, char *outName)
/* axtBest - Remove second best alignments. */
{
FILE *f = mustOpen(outName, "w");
char *matrixName = optionVal("matrix", NULL);
struct axtScoreScheme *ss = NULL;
struct axt *axtList = NULL, *axt, *nextAxt, *goodList = NULL;
struct bestKeeper *bk = NULL;
int i;

if (chromSize <= 0)
    errAbort("ChromSize must be a positive number");
if (matrixName == NULL)
    ss = axtScoreSchemeDefault();
else
    ss = axtScoreSchemeRead(matrixName);
axtList = axtReadAll(inName, chromName, chromSize);
uglyf("Read %d elements from %s\n", slCount(axtList), inName);
AllocArray(bk, chromSize+1);	/* Include extra sentinal value. */
uglyf("Allocated %d elements in array\n", chromSize);

/* Find local best in big soft window and get rid of
 * alignments that aren't best anywhere, using axt->score
 * field for scratch space. */
for (axt = axtList; axt != NULL; axt = axt->next)
    markBest(axt, bk, chromSize, ss, softWin, TRUE);
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
    if (axt->score >= minWin)
        {
	slAddHead(&goodList, axt);
	}
    }
slReverse(&goodList);
axtList = NULL;
uglyf("%d elements in goodList\n", slCount(goodList));


/* Find local best alignments in big hard window and
 * output them. */
memset(bk, 0, chromSize * sizeof(*bk));
for (axt = goodList; axt != NULL; axt = axt->next)
    markBest(axt, bk, chromSize, ss, softWin, FALSE);
outputBestRanges(bk, chromSize, ss, minWin, f);

}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
softWin = optionInt("softWin", softWin);
minWin = optionInt("minWin", minWin);
initBaseVal();
axtBest(argv[1], argv[2], atoi(argv[3]), argv[4]);
return 0;
}
