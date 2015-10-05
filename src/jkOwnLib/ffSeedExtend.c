/* ffSeedExtend - extend alignment out from ungapped seeds. */
/* Copyright 2003 Jim Kent.  All rights reserved. */

#include "common.h"
#include "dnaseq.h"
#include "localmem.h"
#include "memalloc.h"
#include "bits.h"
#include "genoFind.h"
#include "fuzzyFind.h"
#include "supStitch.h"
#include "bandExt.h"
#include "gfInternal.h"


static void extendExactRight(int qMax, int tMax, char **pEndQ, char **pEndT)
/* Extend endQ/endT as much to the right as possible. */
{
int last = min(qMax, tMax);
int i;
char *q = *pEndQ, *t = *pEndT;

for (i=0; i<last; ++i)
    {
    if (*q != *t)
	break;
    q += 1;
    t += 1;
    }
*pEndQ = q;
*pEndT = t;
}

static void extendExactLeft(int qMax, int tMax, char **pStartQ, char **pStartT)
/* Extend startQ/startT as much to the left as possible. */
{
int last = min(qMax, tMax);
int i;
char *q = *pStartQ - 1, *t = *pStartT - 1;

for (i=0; i<last; ++i)
    {
    if (*q != *t)
	break;
    q -= 1;
    t -= 1;
    }
*pStartQ = q + 1;
*pStartT = t + 1;
}

static void extendGaplessRight(int qMax, int tMax, int maxDrop, char **pEndQ, char **pEndT)
/* Extend endQ/endT as much to the right as possible allowing mismatches
 * but not gaps. */
{
int last = min(qMax, tMax);
int i;
char *q = *pEndQ, *t = *pEndT;
int score = 0, bestScore = -1, bestPos = -1;

for (i=0; i<last; ++i)
    {
    if (q[i] == t[i])
	{
	++score;
	if (score > bestScore)
	    {
	    bestScore = score;
	    bestPos = i;
	    }
	}
    else
	{
	score -= 3;
	if (bestScore - score >= maxDrop)
	    break;
	}
    }
++bestPos;
*pEndQ = q+bestPos;
*pEndT = t+bestPos;
}

static void extendGaplessLeft(int qMax, int tMax, int maxDrop, char **pStartQ, char **pStartT)
/* Extend startQ/startT as much to the left as possible allowing mismatches
 * but not gaps. */
{
int score = 0, bestScore = -1, bestPos = 0;
int last = -min(qMax, tMax);
int i;
char *q = *pStartQ, *t = *pStartT;

for (i=-1; i>=last; --i)
    {
    if (q[i] == t[i])
	{
	++score;
	if (score > bestScore)
	    {
	    bestScore = score;
	    bestPos = i;
	    }
	}
    else
	{
	score -= 3;
	if (bestScore - score >= maxDrop)
	    break;
	}
    }
*pStartQ = q+bestPos;
*pStartT = t+bestPos;
}

static int ffHashFuncN(char *s, int seedSize)
/* Return hash function for a 4k hash on sequence. */
{
int acc = 0;
int i;
for (i=0; i<seedSize; ++i)
    {
    acc <<= 1;
    acc += ntVal[(int)s[i]];
    }
return acc&0xfff;
}

struct seqHashEl
/* An element in a sequence hash */
    {
    struct seqHashEl *next;
    char *seq;
    };

static boolean totalDegenerateN(char *s, int seedSize)
/* Return TRUE if repeat of period 1 or 2. */
{
char c1 = s[0], c2 = s[1];
int i;
if (seedSize & 1)
    {
    seedSize -= 1;
    if (c1 != s[seedSize])
        return FALSE;
    }
for (i=2; i<seedSize; i += 2)
    {
    if (c1 != s[i] || c2 != s[i+1])
        return FALSE;
    }
return TRUE;
}

static struct ffAli *ffFindExtendNmers(char *nStart, char *nEnd, char *hStart, char *hEnd,
	int seedSize)
/* Find perfectly matching n-mers and extend them. */
{
struct lm *lm = lmInit(32*1024);
struct seqHashEl **hashTable, *hashEl, **hashSlot;
struct ffAli *ffList = NULL, *ff;
char *n = nStart, *h = hStart, *ne = nEnd - seedSize, *he = hEnd - seedSize;

/* Hash the needle. */
lmAllocArray(lm, hashTable, 4*1024);
while (n <= ne)
    {
    if (!totalDegenerateN(n, seedSize))
	{
	hashSlot = ffHashFuncN(n, seedSize) + hashTable;
	lmAllocVar(lm, hashEl);
	hashEl->seq = n;
	slAddHead(hashSlot, hashEl);
	}
    ++n;
    }

/* Scan the haystack adding hits. */
while (h <= he)
    {
    for (hashEl = hashTable[ffHashFuncN(h, seedSize)]; 
    	hashEl != NULL; hashEl = hashEl->next)
	{
	if (memcmp(hashEl->seq, h, seedSize) == 0)
	    {
	    AllocVar(ff);
	    ff->hStart = h;
	    ff->hEnd = h + seedSize;
	    ff->nStart = hashEl->seq;
	    ff->nEnd = hashEl->seq + seedSize;
	    extendExactLeft(ff->nStart - nStart, ff->hStart - hStart, 
		&ff->nStart, &ff->hStart);
	    extendExactRight(nEnd - ff->nEnd, hEnd - ff->hEnd, &ff->nEnd, &ff->hEnd);
	    ff->left = ffList;
	    ffList = ff;
	    }
	}
    ++h;
    }
ffList = ffMakeRightLinks(ffList);
ffList = ffMergeClose(ffList, nStart, hStart);
lmCleanup(&lm);
return ffList;
}

static void clumpToExactRange(struct gfClump *clump, bioSeq *qSeq, int tileSize,
	int frame, struct trans3 *t3, struct gfRange **pRangeList)
/* Covert extend and merge hits in clump->hitList so that
 * you get a list of maximal segments with no gaps or mismatches. */
{
struct gfSeqSource *target = clump->target;
aaSeq *tSeq = target->seq;
BIOPOL *qs, *ts, *qe, *te;
struct gfHit *hit;
int qStart = 0, tStart = 0, qEnd = 0, tEnd = 0, newQ = 0, newT = 0;
boolean outOfIt = TRUE;		/* Logically outside of a clump. */
struct gfRange *range;
BIOPOL *lastQs = NULL, *lastQe = NULL, *lastTs = NULL;

if (tSeq == NULL)
    internalErr();

/* The termination condition of this loop is a little complicated.
 * We want to output something either when the next hit can't be
 * merged into the previous, or at the end of the list.  To avoid
 * duplicating the output code we're forced to complicate the loop
 * termination logic.  Hence the check for hit == NULL to break
 * the loop is not until near the end of the loop. */
for (hit = clump->hitList; ; hit = hit->next)
    {
    if (hit != NULL)
        {
	newQ = hit->qStart;
	newT = hit->tStart - target->start;
	}

    /* See if it's time to output merged (diagonally adjacent) hits. */
    if (!outOfIt)	/* Not first time through. */
        {
	/* As a micro-optimization handle strings of adjacent hits
	 * specially.  Don't do the extensions until we've merged
	 * all adjacent hits. */
	if (hit == NULL || newQ != qEnd || newT != tEnd)
	    {
	    qs = qSeq->dna + qStart;
	    ts = tSeq->dna + tStart;
	    qe = qSeq->dna + qEnd;
	    te = tSeq->dna + tEnd;
	    extendExactRight(qSeq->size - qEnd, tSeq->size - tEnd,
		&qe, &te);
	    extendExactLeft(qStart, tStart, &qs, &ts);
	    if (qs != lastQs || ts != lastTs || qe != lastQe || qs !=  lastQs)
		{
		lastQs = qs;
		lastTs = ts;
		lastQe = qe;
		// BIOPOL *lastTe = te;  unnecessary
		AllocVar(range);
		range->qStart = qs - qSeq->dna;
		range->qEnd = qe - qSeq->dna;
		range->tName = cloneString(tSeq->name);
		range->tSeq = tSeq;
		range->tStart = ts - tSeq->dna;
		range->tEnd = te - tSeq->dna;
		range->hitCount = qe - qs;
		range->frame = frame;
		range->t3 = t3;
		slAddHead(pRangeList, range);
		}
	    outOfIt = TRUE;
	    }
	}
    if (hit == NULL)
        break;

    if (outOfIt)
        {
	qStart = newQ;
	qEnd = qStart + tileSize;
	tStart = newT;
	tEnd = tStart + tileSize;
	outOfIt = FALSE;
	}
    else
        {
	qEnd = newQ + tileSize;
	tEnd = newT + tileSize;
	}
    }
}

static void addExtraHits(struct gfHit *hitList, int hitSize, 
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, struct ffAli **pExtraList)
/* Extend hits as far as possible, convert to ffAli, and add to extraList. */
{
struct gfHit *hit;
struct ffAli *ff;
char *qs = qSeq->dna, *ts = tSeq->dna;
char *qe = qs + qSeq->size,  *te = ts + tSeq->size;
for (hit = hitList; hit != NULL; hit = hit->next)
    {
    AllocVar(ff);
    ff->nStart = ff->nEnd = qs + hit->qStart;
    ff->hStart = ff->hEnd = ts + hit->tStart;
    ff->nEnd += hitSize;
    ff->hEnd += hitSize;
    ff->left = *pExtraList;
    ffExpandExactLeft(ff, qs, ts);
    ffExpandExactRight(ff, qe, te);
    *pExtraList = ff;
    }
}

static struct ffAli *foldInExtras(struct dnaSeq *qSeq, struct dnaSeq *tSeq,
	struct ffAli *ffList, struct ffAli *extraList)
/* Integrate extraList into ffList and return result. 
 * Frees bits of extraList that aren't used. */
{
if (extraList != NULL)
    {
    struct ssBundle *bun;
    struct ssFfItem *ffi;
    AllocVar(bun);
    bun->qSeq = qSeq;
    bun->genoSeq = tSeq;
    bun->avoidFuzzyFindKludge = TRUE;
    AllocVar(ffi);
    ffi->ff = ffList;
    slAddHead(&bun->ffList, ffi);
    AllocVar(ffi);
    ffi->ff = extraList;
    slAddHead(&bun->ffList, ffi);
    ssStitch(bun, ffCdna, 16, 1);
    if (bun->ffList != NULL)
	{
	ffList = bun->ffList->ff;
	bun->ffList->ff = NULL;
	}
    else
	{
        ffList = NULL;
	}
    ssBundleFree(&bun);
    }
return ffList;
}

static struct ffAli *scanIndexForSmallExons(struct genoFind *gf, struct gfSeqSource *target,
    struct dnaSeq *qSeq, Bits *qMaskBits, int qMaskOffset, struct dnaSeq *tSeq,
    struct lm *lm, struct ffAli *ffList)
/* Use index to look for missing small exons. */
{
int qGap, tGap, tStart, qStart;
struct ffAli *lastFf = NULL, *ff = ffList;
struct gfHit *hitList = NULL;
struct dnaSeq qSubSeq;
struct ffAli *extraList = NULL;
int tileSize = gf->tileSize;
int biggestToFind = 200;	/* Longer should be found at an earlier stage */

/* Handle problematic empty case immediately. */
if (ffList == NULL)
    return NULL;

ZeroVar(&qSubSeq);

/* Look for initial gap. */
qGap = ff->nStart - qSeq->dna;
tGap = ff->hStart - tSeq->dna;
if (qGap >= tileSize && qGap <= biggestToFind && tGap >= tileSize)
    {
    tStart = ff->hStart - tSeq->dna;
    if (tGap > ffIntronMax) 
	{
	tGap = ffIntronMax;
	}
    qSubSeq.dna = qSeq->dna;
    qSubSeq.size = qGap;
    hitList = gfFindHitsInRegion(gf, &qSubSeq, qMaskBits, qMaskOffset,
	lm, target, tStart - tGap, tStart);
    addExtraHits(hitList, tileSize, &qSubSeq, tSeq, &extraList);
    }

/* Look for middle gaps. */
for (;;)
    {
    lastFf = ff;
    ff = ff->right;
    if (ff == NULL)
	break;
    qGap = ff->nStart - lastFf->nEnd;
    tGap = ff->hStart - lastFf->hEnd;
    if (qGap >= tileSize && qGap <= biggestToFind && tGap >= tileSize)
	 {
	 qStart = lastFf->nEnd - qSeq->dna;
	 tStart = lastFf->hEnd - tSeq->dna;
	 qSubSeq.dna = lastFf->nEnd;
	 qSubSeq.size = qGap;
	 hitList = gfFindHitsInRegion(gf, &qSubSeq, qMaskBits, qMaskOffset + qStart, 
		lm, target, tStart, tStart + tGap);
	 addExtraHits(hitList, tileSize, &qSubSeq, tSeq, &extraList);
	 }
    }

/* Look for end gaps. */
qGap = qSeq->dna + qSeq->size - lastFf->nEnd;
tGap = tSeq->dna + tSeq->size - lastFf->hEnd;
if (qGap >= tileSize && qGap < biggestToFind && tGap >= tileSize)
    {
    if (tGap > ffIntronMax) tGap = ffIntronMax;
    qStart = lastFf->nEnd - qSeq->dna;
    tStart = lastFf->hEnd - tSeq->dna;
    qSubSeq.dna = lastFf->nEnd;
    qSubSeq.size = qGap;
    hitList = gfFindHitsInRegion(gf, &qSubSeq, qMaskBits, qMaskOffset + qStart, 
		lm, target, tStart, tStart + tGap);
    addExtraHits(hitList, tileSize, &qSubSeq, tSeq, &extraList);
    }
extraList = ffMakeRightLinks(extraList);
ffList = foldInExtras(qSeq, tSeq, ffList, extraList);
return ffList;
}


static void bandExtBefore(struct axtScoreScheme *ss, struct ffAli *ff,
	int qGap, int tGap, struct ffAli **pExtraList)
/* Add in blocks from a banded extension before ff into the gap
 * and append results if any to *pExtraList. */
{
struct ffAli *ext;
int minGap = min(qGap, tGap);
int maxGap = minGap * 2;
if (minGap > 0)
    {
    if (qGap > maxGap) qGap = maxGap;
    if (tGap > maxGap) tGap = maxGap;
    ext = bandExtFf(NULL, ss, 3, ff, ff->nStart - qGap, ff->nStart, 
	    ff->hStart - tGap, ff->hStart, -1, maxGap);
    ffCat(pExtraList, &ext);
    }
}

static void bandExtAfter(struct axtScoreScheme *ss, struct ffAli *ff,
	int qGap, int tGap, struct ffAli **pExtraList)
/* Add in blocks from a banded extension after ff into the gap
 * and append results if any to *pExtraList. */
{
struct ffAli *ext;
int minGap = min(qGap, tGap);
int maxGap = minGap * 2;
if (minGap > 0)
    {
    if (qGap > maxGap) qGap = maxGap;
    if (tGap > maxGap) tGap = maxGap;
    ext = bandExtFf(NULL, ss, 3, ff, ff->nEnd, ff->nEnd + qGap,
	    ff->hEnd, ff->hEnd + tGap, 1, maxGap);
    ffCat(pExtraList, &ext);
    }
}
	
	
static struct ffAli *bandedExtend(struct dnaSeq *qSeq, struct dnaSeq *tSeq, struct ffAli *ffList)
/* Do banded extension where there is missing sequence. */
{
struct ffAli *extraList = NULL, *ff = ffList, *lastFf = NULL;
struct axtScoreScheme *ss = axtScoreSchemeRnaDefault();
int qGap, tGap;

if (ff == NULL)
    return NULL;

/* Look for initial gap. */
qGap = ff->nStart - qSeq->dna;
tGap = ff->hStart - tSeq->dna;
bandExtBefore(ss, ff, qGap, tGap, &extraList);

/* Look for middle gaps. */
for (;;)
    {
    lastFf = ff;
    ff = ff->right;
    if (ff == NULL)
	break;
    qGap = ff->nStart - lastFf->nEnd;
    tGap = ff->hStart - lastFf->hEnd;
    bandExtAfter(ss, lastFf, qGap, tGap, &extraList);
    bandExtBefore(ss, ff, qGap, tGap, &extraList);
    }

/* Look for end gaps. */
qGap = qSeq->dna + qSeq->size - lastFf->nEnd;
tGap = tSeq->dna + tSeq->size - lastFf->hEnd;
bandExtAfter(ss, lastFf, qGap, tGap, &extraList);

ffList = foldInExtras(qSeq, tSeq, ffList, extraList);
return ffList;
}



static struct ffAli *expandGapless(struct dnaSeq *qSeq, struct dnaSeq *tSeq, struct ffAli *ffList)
/* Do non-banded extension sequence.  Since this is quick
 * we'll let it overlap with existing sequence. */
{
struct ffAli *ff = ffList, *lastFf = NULL;
char *nStart = qSeq->dna;
char *nEnd = qSeq->dna + qSeq->size;
char *hStart = tSeq->dna;
char *hEnd = tSeq->dna + tSeq->size;

/* Look for initial gap. */
extendGaplessLeft(ff->nStart - nStart, ff->hStart - hStart, 
	9, &ff->nStart, &ff->hStart);

/* Look for middle gaps. */
for (;;)
    {
    lastFf = ff;
    ff = ff->right;
    if (ff == NULL)
	break;
    extendGaplessRight(nEnd - lastFf->nEnd, hEnd - lastFf->hEnd, 9, 
	&lastFf->nEnd, &lastFf->hEnd);
    extendGaplessLeft(ff->nStart - nStart, ff->hStart - hStart, 9, 
	&ff->nStart, &ff->hStart);
    }
extendGaplessRight(nEnd - lastFf->nEnd,
	hEnd - lastFf->hEnd, 9,
	&lastFf->nEnd, &lastFf->hEnd);
return ffList;
}


static int seedResolvePower(int seedSize, int resolveLimit)
/* Return how many bases to search for seed of given
 * size. */
{
int res;
if (seedSize >= 14)	/* Avoid int overflow */
    return ffIntronMax;
res  = (1 << (seedSize+seedSize-resolveLimit));
if (res > ffIntronMax)
    res = ffIntronMax;
return res;
}

static char *scanExactLeft(char *n, int nSize, int hSize, char *hEnd, int resolveLimit)
/* Look for first exact match to the left. */
{
/* Optimize a little by comparing the first character inline. */
char n1 = *n++;
char *hStart;
int maxSize = seedResolvePower(nSize, resolveLimit);

if (hSize > maxSize) hSize = maxSize;
nSize -= 1;
hStart = hEnd - hSize;

hEnd -= nSize;
while (hEnd >= hStart)
    {
    if (n1 == *hEnd && memcmp(n, hEnd+1, nSize) == 0)
	return hEnd;
    hEnd -= 1;
    }
return NULL;
}

static char *scanExactRight(char *n, int nSize, int hSize, char *hStart, int resolveLimit)
/* Look for first exact match to the right. */
{
/* Optimize a little by comparing the first character inline. */
char n1 = *n++;
char *hEnd;
int maxSize = seedResolvePower(nSize, resolveLimit);

if (hSize > maxSize) hSize = maxSize;
hEnd = hStart + hSize;
nSize -= 1;

hEnd -= nSize;
while (hStart <= hEnd)
    {
    if (n1 == *hStart && memcmp(n, hStart+1, nSize) == 0)
	return hStart;
    hStart += 1;
    }
return NULL;
}

static struct ffAli *fillInExact(char *nStart, char *nEnd, char *hStart, char *hEnd, 
	boolean isRc, boolean scanLeft, boolean scanRight, int resolveLimit)
/* Try and add exact match to the region, adding splice sites to
 * the area to search for small query sequences. scanRight and scanLeft
 * specify which way to scan and which side of the splice site to
 * include.  One or the other or neither should be set. */
{
struct ffAli *ff = NULL;
char *hPos = NULL;
int nGap = nEnd - nStart;
int hGap = hEnd - hStart;
int minGap = min(nGap, hGap);

if (minGap <= 2)
    return NULL;
if (scanLeft)
    {
    if ((hPos = scanExactLeft(nStart, nGap, hGap, hEnd, resolveLimit)) != NULL)
	{
	AllocVar(ff);
	ff->nStart = nStart;
	ff->nEnd = nEnd;
	ff->hStart = hPos;
	ff->hEnd = hPos + nGap;
	return ff;
	}
    }
else
    {
    if ((hPos = scanExactRight(nStart, nGap, hGap, hStart, resolveLimit)) != NULL)
	{
	AllocVar(ff);
	ff->nStart = nStart;
	ff->nEnd = nEnd;
	ff->hStart = hPos;
	ff->hEnd = hPos + nGap;
	return ff;
	}
    }
return NULL;
}

static struct ffAli *findFromSmallerSeeds(char *nStart, char *nEnd, 
	char *hStart, char *hEnd, boolean isRc, 
	boolean scanLeft, boolean scanRight, int seedSize, int resolveLimit)
/* Look for matches with smaller seeds. */
{
int nGap = nEnd - nStart;
if (nGap >= seedSize)
    {
    struct ffAli *ffList;
    if (scanLeft || scanRight)
        {
	int hGap = hEnd - hStart;
	int maxSize = seedResolvePower(seedSize, resolveLimit);
	if (hGap > maxSize) hGap = maxSize;
	if (scanLeft)
	    hStart = hEnd - hGap;
	if (scanRight)
	    hEnd = hStart + hGap;
	}
    ffList = ffFindExtendNmers(nStart, nEnd, hStart, hEnd, seedSize);
    if (ffList != NULL)
	{
	struct ffAli *extensions = NULL, *ff;
	struct axtScoreScheme *ss = axtScoreSchemeRnaDefault();
	for (ff = ffList; ff != NULL; ff = ff->right)
	    {
	    bandExtBefore(ss, ff, ff->nStart - nStart, ff->hStart - hStart, &extensions);
	    bandExtAfter(ss, ff, nEnd - ff->nEnd, hEnd - ff->hEnd, &extensions);
	    }
	ffCat(&ffList, &extensions);
	}

    return ffList;
    }
return NULL;
}

static int countT(char *s, int size)
/* Count number of initial T's. */
{
int i;
for (i=0; i<size; ++i)
    {
    if (s[i] != 't')
	break;
    }
return i;
}

static int countA(char *s, int size)
/* Count number of terminal A's. */
{
int count = 0;
int i;
for (i=size-1; i >= 0; --i)
    {
    if (s[i] == 'a')
	++count;
    else
	break;
    }
return count;
}

struct ffAli *scanTinyOne(char *nStart, char *nEnd, char *hStart, char *hEnd, 
	boolean isRc, boolean scanLeft, boolean scanRight, int seedSize)
/* Try and add some exon candidates in the region. */
{
struct ffAli *ff;
int nGap = nEnd - nStart;
if (nGap > 80)		/* The index should have found things this big already. */
    return NULL;
if (scanLeft && isRc)
    nStart += countT(nStart, nGap);
if (scanRight && !isRc)
    nEnd -= countA(nStart, nGap);
ff = fillInExact(nStart, nEnd, hStart, hEnd, isRc, scanLeft, scanRight, 3);
if (ff != NULL)
    {
    return ff;
    }
return findFromSmallerSeeds(nStart, nEnd, hStart, hEnd, isRc,
	scanLeft, scanRight, seedSize, 3);
}

static struct ffAli *scanForSmallerExons( int seedSize, 
	struct dnaSeq *qSeq, struct dnaSeq *tSeq,
	boolean isRc, struct ffAli *ffList)
/* Look for exons too small to be caught by index. */
{
struct ffAli *extraList = NULL, *ff = ffList, *lastFf = NULL, *newFf;

if (ff == NULL)
    return NULL;

/* Look for initial gap. */
newFf = scanTinyOne(qSeq->dna, ff->nStart, tSeq->dna, ff->hStart, 
	isRc, TRUE, FALSE, seedSize); 
ffCat(&extraList, &newFf);

/* Look for middle gaps. */
for (;;)
    {
    lastFf = ff;
    ff = ff->right;
    if (ff == NULL)
	break;
    newFf = scanTinyOne(lastFf->nEnd, ff->nStart, lastFf->hEnd, ff->hStart, 
	isRc, FALSE, FALSE, seedSize);
    ffCat(&extraList, &newFf);
    }

/* Look for end gaps. */
newFf = scanTinyOne(lastFf->nEnd, qSeq->dna + qSeq->size, 
	lastFf->hEnd, tSeq->dna + tSeq->size, isRc, FALSE, TRUE, seedSize);
ffCat(&extraList, &newFf);

ffList = foldInExtras(qSeq, tSeq, ffList, extraList);
return ffList;
}

static struct ffAli *scanForTinyInternal(struct dnaSeq *qSeq, struct dnaSeq *tSeq,
	boolean isRc, struct ffAli *ffList)
/* Look for exons too small to be caught by index. */
{
struct ffAli *extraList = NULL, *ff = ffList, *lastFf = NULL, *newFf;

if (ff == NULL)
    return NULL;

/* Look for middle gaps. */
for (;;)
    {
    lastFf = ff;
    ff = ff->right;
    if (ff == NULL)
	break;
    newFf = fillInExact(lastFf->nEnd, ff->nStart, lastFf->hEnd, ff->hStart, isRc, 
	FALSE, FALSE, 0);
    ffCat(&extraList, &newFf);
    }

ffList = foldInExtras(qSeq, tSeq, ffList, extraList);
return ffList;
}

static boolean tradeMismatchToCloseSpliceGap( struct ffAli *left, 
	struct ffAli *right, int orientation)
/* Try extending one side or the other to close gap caused by
 * mismatch near splice site */
{
if (intronOrientation(left->hEnd+1, right->hStart) == orientation)
    {
    left->hEnd += 1;
    left->nEnd += 1;
    return TRUE;
    }
if (intronOrientation(left->hEnd, right->hStart-1) == orientation)
    {
    right->hStart -= 1;
    right->nStart -= 1;
    return TRUE;
    }
return FALSE;
}

static int calcSpliceScore(struct axtScoreScheme *ss, 
	char a1, char a2, char b1, char b2, int orientation)
/* Return adjustment for match/mismatch of consensus. */
{
int score = 0;
int matchScore = ss->matrix['c']['c'];
if (orientation >= 0)  /* gt/ag or gc/ag */
    {
    score += ss->matrix[(int)a1]['g'];
    if (a2 != 'c')
        score += ss->matrix[(int)a2]['t'];
    score += ss->matrix[(int)b1]['a'];
    score += ss->matrix[(int)b2]['g'];
    }
else		       /* ct/ac or ct/gc */
    {
    score += ss->matrix[(int)a1]['c'];
    score += ss->matrix[(int)a2]['t'];
    if (b1 != 'g')
	score += ss->matrix[(int)b1]['a'];
    score += ss->matrix[(int)b2]['c'];
    }
if (score >= 3* matchScore)
    score += matchScore;
return score;
}

static void grabAroundIntron(char *hpStart, int iPos, int iSize,
	int modPeelSize, char *hSeq)
/* Grap sequence on either side of intron. */
{
memcpy(hSeq, hpStart, iPos);
memcpy(hSeq+iPos, hpStart+iPos+iSize, modPeelSize - iPos);
hSeq[modPeelSize] = 0;
}

static struct ffAli *hardRefineSplice(struct ffAli *left, struct ffAli *right,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, struct ffAli *ffList, 
	int orientation)
/* Do difficult refinement of splice site.  See if
 * can get nice splice sites without breaking too much.  */
{
/* Strategy: peel back about 6 bases on either side of intron.
 * Then try positioning the intron at each position in the
 * peeled area and assessing score. */
int peelSize = 12;
char nSeq[12+1], hSeq[12+1+1];
char nSym[25], hSym[25];
int symCount;
int seqScore, spliceScore, score, maxScore = 0;
int nGap = right->nStart - left->nEnd;
int hGap = right->hStart - left->hEnd;
int peelLeft = (peelSize - nGap)/2;
int intronSize = hGap - nGap;
char *npStart = left->nEnd - peelLeft;
char *npEnd = npStart + peelSize;
char *hpStart = left->hEnd - peelLeft;
char *hpEnd = npEnd + (right->hStart - right->nStart);
struct axtScoreScheme *ss = axtScoreSchemeRnaDefault();
static int modSize[3] = {0, 1, -1};
int modIx;
int bestPos = -1, bestMod = 0;
int iPos;

memcpy(nSeq, npStart, peelSize);
nSeq[peelSize] = 0;
for (modIx=0; modIx < ArraySize(modSize); ++modIx)
    {
    int modOne = modSize[modIx];
    int modPeelSize = peelSize - modOne;
    int iSize = intronSize + modOne;
    for (iPos=0; iPos <= modPeelSize; iPos++)
        {
	grabAroundIntron(hpStart, iPos, iSize, modPeelSize, hSeq);
	if (bandExt(TRUE, ss, 2, nSeq, peelSize, hSeq, modPeelSize, 1,
		sizeof(hSym), &symCount, nSym, hSym, NULL, NULL))
	    {
	    seqScore = axtScoreSym(ss, symCount, nSym, hSym);
	    spliceScore = calcSpliceScore(ss, hpStart[iPos], hpStart[iPos+1],
		    hpStart[iPos+iSize-2], hpStart[iPos+iSize-1], orientation);
	    score = seqScore + spliceScore;
	    if (score > maxScore)
		{
		maxScore = score;
		bestPos = iPos;
		bestMod = modOne;
		}
	    }
	}
    }
if (maxScore > 0)
    {
    int modPeelSize = peelSize - bestMod;
    int i,diff, cutSymIx = 0;
    int nIx, hIx;
    struct ffAli *ff;

    /* Regenerate the best alignment. */
    grabAroundIntron(hpStart, bestPos, intronSize + bestMod, modPeelSize, hSeq); 
    bandExt(TRUE, ss, 2, nSeq, peelSize, hSeq, modPeelSize, 1,
	    sizeof(hSym), &symCount, nSym, hSym, NULL, NULL);

    /* Peel back surrounding ffAli's */
    if (left->nStart > npStart || right->nEnd < npEnd)
	{
	/* It would take a lot of code to handle this case. 
	 * I believe it is rare enough that it's not worth
	 * it.  This verbosity will help keep track of how
	 * often it comes up. */
	verbose(2, "Unable to peel in hardRefineSplice\n");
	return ffList;
	}
    diff = left->nEnd - npStart;
    left->nEnd -= diff;
    left->hEnd -= diff;
    diff = right->nStart - npEnd;
    right->nStart -= diff;
    right->hStart -= diff;

    /* Step through base by base alignment from the left until
     * hit intron, converting it into ffAli format. */
    nIx = hIx = 0;
    ff = left;
    for (i=0; i<symCount; ++i)
	{
	if (hIx == bestPos)
	    {
	    cutSymIx = i;
	    break;
	    }
	if (hSym[i] == '-')
	    {
	    ff = NULL;
	    nIx += 1;
	    }
	else if (nSym[i] == '-')
	    {
	    ff = NULL;
	    hIx += 1;
	    }
	else
	    {
	    if (ff == NULL)
		{
		AllocVar(ff);
		ff->left = left;
		ff->right = right;
		left->right = ff;
		right->left = ff;
		left = ff;
		ff->nStart = ff->nEnd = npStart + nIx;
		ff->hStart = ff->hEnd = hpStart + hIx;
		}
	    ++nIx;
	    ++hIx;
	    ff->nEnd += 1;
	    ff->hEnd += 1;
	    }
	}

    /* Step through base by base alignment from the right until
     * hit intron, converting it into ffAli format. */
    ff = right;
    hIx = nIx = 0;	/* Index from right side. */
    for (i = symCount-1; i >= cutSymIx; --i)
	{
	if (hSym[i] == '-')
	    {
	    ff = NULL;
	    nIx += 1;
	    }
	else if (nSym[i] == '-')
	    {
	    ff = NULL;
	    hIx += 1;
	    }
	else
	    {
	    if (ff == NULL)
		{
		AllocVar(ff);
		ff->left = left;
		ff->right = right;
		left->right = ff;
		right->left = ff;
		left = ff;
		ff->nStart = ff->nEnd = npEnd - nIx;
		ff->hStart = ff->hEnd = hpEnd - hIx;
		}
	    ++nIx;
	    ++hIx;
	    ff->nStart -= 1;
	    ff->hStart -= 1;
	    }
	}
    }
return ffList;
}


static struct ffAli *refineSpliceSites(struct dnaSeq *qSeq, struct dnaSeq *tSeq,
	struct ffAli *ffList)
/* Try and get a little closer to splice site consensus
 * by jiggle things a little. */
{
int orientation = ffIntronOrientation(ffList);
struct ffAli *ff, *nextFf;
if (orientation == 0)
    return ffList;
if (ffSlideOrientedIntrons(ffList, orientation))
    ffList = ffRemoveEmptyAlis(ffList, TRUE);
for (ff = ffList; ff != NULL; ff = nextFf)
    {
    int nGap, hGap;
    if ((nextFf = ff->right) == NULL)
	break;
    nGap = nextFf->nStart - ff->nEnd;
    hGap = nextFf->hStart - ff->hEnd;
    if (nGap > 0 && nGap <= 6 && hGap >= 30)
	{
	if (nGap == 1)
	    {
	    if (tradeMismatchToCloseSpliceGap(ff, nextFf, orientation))
	        continue;
	    }
	ffList = hardRefineSplice(ff, nextFf, qSeq, tSeq, ffList, orientation);
	}
    }
return ffList;
}


static boolean smoothOneGap(struct ffAli *left, struct ffAli *right, struct ffAli *ffList)
/* If and necessary connect left and right - either directly or
 * with a small intermediate ffAli inbetween.  Do not bother to
 * merge directly abutting regions,  this happens later.  Returns
 * TRUE if any smoothing done. */ 
{
int nGap = right->nStart - left->nEnd;
int hGap = right->hStart - left->hEnd;
if (nGap > 0 && hGap > 0 && nGap < 10 && hGap < 10)
    {
    int sizeDiff = nGap - hGap;
    if (sizeDiff < 0) sizeDiff = -sizeDiff;
    if (sizeDiff <= 3)
	{
	struct axtScoreScheme *ss = axtScoreSchemeRnaDefault();
	char hSym[20], nSym[20];
	int symCount;
	if (bandExt(TRUE, ss, 3, left->nEnd, nGap, left->hEnd, hGap, 1,
		sizeof(hSym), &symCount, nSym, hSym, NULL, NULL))
	    {
	    int gapPenalty = -ffCalcCdnaGapPenalty(hGap, nGap) * ss->matrix['a']['a'];
	    int score = axtScoreSym(ss, symCount, nSym, hSym);
	    if (score >= gapPenalty)
		{
		struct ffAli *l, *r;
		l = ffAliFromSym(symCount, nSym, hSym, NULL, left->nEnd, left->hEnd);
		r = ffRightmost(l);
		left->right = l;
		l->left = left;
		r->right = right;
		right->left = r;
		return TRUE;
		}
	    }
	}
    }
return FALSE;
}


static struct ffAli *smoothSmallGaps(struct dnaSeq *qSeq, struct dnaSeq *tSeq,
	struct ffAli *ffList)
/* Fill in small double sided gaps where possible. */
{
struct ffAli *left = ffList, *right;
boolean smoothed = FALSE;

if (ffList == NULL) return NULL;
for (;;)
    {
    if ((right = left->right) == NULL)
	break;
    if (smoothOneGap(left, right, ffList))
	smoothed = TRUE;
    left = right;
    }
if (smoothed)
    {
    ffList = ffMergeNeedleAlis(ffList, TRUE);
    }
return ffList;
}

int aPenalty(char *s, int size)
/* Penalty for polyA/polyT */
{
int aCount = 0, tCount = 0;
int i;
char c;
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (c == 'a') ++aCount;
    if (c == 't') ++tCount;
    }
if (tCount > aCount) aCount = tCount;
if (aCount >= size)
    return aCount-1;
else if (aCount >= size*0.75)
    return aCount * 0.90;
else
    return 0;
}

static int trimGapPenalty(int hGap, int nGap, char *iStart, char *iEnd, int orientation)
/* Calculate gap penalty for routine below. */
{
int penalty =  ffCalcGapPenalty(hGap, nGap, ffCdna);
if (hGap > 2 || nGap > 2)	/* Not just a local extension. */
				/* Score gap to favor introns. */
    {
    penalty <<= 1;
    if (nGap > 0)	/* Intron gaps are not in n side at all. */
	 penalty += 3;
    			/* Good splice sites give you bonus 2,
			 * bad give you penalty of six. */
    penalty += 6 - 2*ffScoreIntron(iStart[0], iStart[1], 
    	iEnd[-2], iEnd[-1], orientation);
    }
return penalty;
}


static struct ffAli *trimFlakyEnds(struct dnaSeq *qSeq, struct dnaSeq *tSeq,
	struct ffAli *ffList)
/* Get rid of small initial and terminal exons that seem to just
 * be chance alignments.  Looks for splice sites and non-degenerate
 * sequence to keep things. */
{
int orientation = ffIntronOrientation(ffList);
struct ffAli *left, *right;
char *iStart, *iEnd;
int blockScore, gapPenalty;

/* If one or less block then don't bother. */
if (ffAliCount(ffList) < 2)
    return ffList;

/* Trim beginnings. */
left = ffList;
right = ffList->right;
while (right != NULL)
    {
    blockScore = ffScoreMatch(left->nStart, left->hStart, 
    	left->nEnd-left->nStart);
    blockScore -= aPenalty(left->nStart, left->nEnd - left->nStart);
    iStart = left->hEnd;
    iEnd = right->hStart;
    gapPenalty = trimGapPenalty(iEnd-iStart, 
    	right->nStart - left->nEnd, iStart, iEnd, orientation);
    if (gapPenalty >= blockScore)
        {
	freeMem(left);
	ffList = right;
	right->left = NULL;
	}
    else
        break;
    left = right;
    right = right->right;
    }

right = ffRightmost(ffList);
if (right == ffList)
    return ffList;
left = right->left;
while (left != NULL)
    {
    blockScore = ffScoreMatch(right->nStart, right->hStart, 
    	right->nEnd-right->nStart);
    blockScore -= aPenalty(right->nStart, right->nEnd - right->nStart);
    iStart = left->hEnd;
    iEnd = right->hStart;
    gapPenalty = trimGapPenalty(iEnd-iStart, 
    	right->nStart - left->nEnd, iStart, iEnd, orientation);
    if (gapPenalty >= blockScore)
        {
	freeMem(right);
	left->right = NULL;
	}
    else
        break;
    right = left;
    left = left->left;
    }
return ffList;
}


static void refineBundle(struct genoFind *gf, 
	struct dnaSeq *qSeq,  Bits *qMaskBits, int qMaskOffset,
	struct dnaSeq *tSeq, struct lm *lm, struct ssBundle *bun, boolean isRc)
/* Refine bundle - extending alignments and looking for smaller exons. */
{
struct ssFfItem *ffi;
struct gfSeqSource *target = gfFindNamedSource(gf, tSeq->name);

/* First do gapless expansions and restitch. */
for (ffi = bun->ffList; ffi != NULL; ffi = ffi->next)
    {
    ffi->ff = expandGapless(qSeq, tSeq, ffi->ff);
    }
ssStitch(bun, ffCdna, 16, 16);

for (ffi = bun->ffList; ffi != NULL; ffi = ffi->next)
    {
    ffi->ff = scanIndexForSmallExons(gf, target, qSeq, qMaskBits, qMaskOffset, 
	tSeq, lm, ffi->ff);
    ffi->ff = bandedExtend(qSeq, tSeq, ffi->ff);
    ffi->ff = scanForSmallerExons(gf->tileSize, qSeq, tSeq, isRc, ffi->ff);
    ffi->ff = refineSpliceSites(qSeq, tSeq, ffi->ff);
    ffi->ff = scanForTinyInternal(qSeq, tSeq, isRc, ffi->ff);
    ffi->ff = smoothSmallGaps(qSeq, tSeq, ffi->ff);
    ffi->ff = trimFlakyEnds(qSeq, tSeq, ffi->ff);
    }
}


struct ssBundle *ffSeedExtInMem(struct genoFind *gf, struct dnaSeq *qSeq, Bits *qMaskBits, 
	int qOffset, struct lm *lm, int minScore, boolean isRc)
/* Do seed and extend type alignment */
{
struct ssBundle *bunList = NULL, *bun;
int hitCount;
struct gfClump *clumpList, *clump;
struct gfRange *rangeList = NULL, *range;
struct dnaSeq *tSeq;

clumpList = gfFindClumpsWithQmask(gf, qSeq, qMaskBits, qOffset, lm, &hitCount);
for (clump = clumpList; clump != NULL; clump = clump->next)
    clumpToExactRange(clump, qSeq, gf->tileSize, 0, NULL, &rangeList);
slSort(&rangeList, gfRangeCmpTarget);
rangeList = gfRangesBundle(rangeList, ffIntronMax);
for (range = rangeList; range != NULL; range = range->next)
    {
    range->qStart += qOffset;
    range->qEnd += qOffset;
    tSeq = range->tSeq;
    AllocVar(bun);
    bun->qSeq = qSeq;
    bun->genoSeq = tSeq;
    bun->ffList = gfRangesToFfItem(range->components, qSeq);
    bun->isProt = FALSE;
    bun->avoidFuzzyFindKludge = TRUE;
    ssStitch(bun, ffCdna, 16, 10);
    refineBundle(gf, qSeq, qMaskBits, qOffset, tSeq, lm, bun, isRc);
    slAddHead(&bunList, bun);
    }
gfRangeFreeList(&rangeList);
gfClumpFreeList(&clumpList);
return bunList;
}

