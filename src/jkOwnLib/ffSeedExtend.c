#include "common.h"
#include "dnaseq.h"
#include "localmem.h"
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


int ffHashFuncN(char *s, int seedSize)
/* Return hash function for a 4k hash on sequence. */
{
int acc = 0;
int i;
for (i=0; i<seedSize; ++i)
    {
    acc <<= 1;
    acc += ntVal[s[i]];
    }
return acc&0xfff;
}

struct seqHashEl
/* An element in a sequence hash */
    {
    struct seqHashEl *next;
    char *seq;
    };

boolean totalDegenerateN(char *s, int seedSize)
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

struct ffAli *ffFindExtendNmers(char *nStart, char *nEnd, char *hStart, char *hEnd,
	int seedSize)
/* Find perfectly matching n-mers and extend them. */
{
struct lm *lm = lmInit(32*1024);
struct seqHashEl **hashTable, *hashChain, *hashEl, **hashSlot;
struct ffAli *ffList = NULL, *ff;
char *n = nStart, *h = hStart, *ne = nEnd - seedSize, *he = hEnd - seedSize;

/* Hash the needle. */
lmAllocArray(lm, hashTable, 4*1024);
while (n <= ne)
    {
    if (!totalDegenerateN(nStart, seedSize))
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
ffList = ffMergeClose(ffList);
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
int maxScore = 0, maxPos = 0, score, pos;
struct gfHit *hit;
int qStart = 0, tStart = 0, qEnd = 0, tEnd = 0, newQ = 0, newT = 0;
boolean outOfIt = TRUE;		/* Logically outside of a clump. */
struct gfRange *range;
BIOPOL *lastQs = NULL, *lastQe = NULL, *lastTs = NULL, *lastTe = NULL;

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
		lastTe = te;
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

void addExtraHits(struct gfHit *hitList, int hitSize, struct dnaSeq *qSeq, struct dnaSeq *tSeq,
	struct ffAli **pExtraList)
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
    ssStitch(bun, ffCdna, 16);
    ffList = bun->ffList->ff;
    bun->ffList->ff = NULL;
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
struct gfHit *hitList = NULL, *hit;
struct dnaSeq qSubSeq;
struct ffAli *extraList = NULL;
int tileSize = gf->tileSize;

/* Handle problematic empty case immediately. */
if (ffList == NULL)
    return NULL;

ZeroVar(&qSubSeq);

/* Look for initial gap. */
qGap = ff->nStart - qSeq->dna;
tGap = ff->hStart - tSeq->dna;
if (qGap >= tileSize && tGap >= tileSize)
    {
    tStart = 0;
    if (tGap > ffIntronMax) 
	{
	tStart = tGap - ffIntronMax;
	tGap = ffIntronMax;
	}
    qSubSeq.dna = qSeq->dna;
    qSubSeq.size = qGap;
    hitList = gfFindHitsInRegion(gf, &qSubSeq, qMaskBits, qMaskOffset,
	lm, target, tStart, tStart + tGap);
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
    if (qGap >= tileSize && tGap >= tileSize)
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
if (qGap >= tileSize && tGap >= tileSize)
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


int seedResolvePower(int seedSize)
/* Return how many bases to search for seed of given
 * size. */
{
if (seedSize >= 15)
    return BIGNUM;
return 1 << (seedSize+seedSize-3);
}

static char *scanExactLeft(char *n, int nSize, int hSize, char *hEnd)
/* Look for first exact match to the left. */
{
/* Optimize a little by comparing the first character inline. */
char n1 = *n++;
char *hStart;
int maxSize = seedResolvePower(nSize);

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

static char *scanExactRight(char *n, int nSize, int hSize, char *hStart)
/* Look for first exact match to the right. */
{
/* Optimize a little by comparing the first character inline. */
char n1 = *n++;
char *hEnd;
int maxSize = seedResolvePower(nSize);

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

#define gfIndexShouldFindSize 75	/* Size that index should find. */

static struct ffAli *exactPlusSplice(char *nStart, char *nEnd, char *hStart, char *hEnd, 
	boolean isRc, boolean scanLeft, boolean scanRight)
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
if (nGap <= 12 )
    {
    /* For small blocks look for perfect match plus splice sites.
     * Splice sites are:
	gt/ag - forward strand
	ct/ac - reverse strand
     */
    char buf[16];
    int i;
    if (scanLeft)
	{
	memcpy(buf, nStart, nGap);
	if (isRc)
	    {
	    buf[nGap] = 'c';
	    buf[nGap+1] = 't';
	    }
	else
	    {
	    buf[nGap] = 'g';
	    buf[nGap+1] = 't';
	    }
	for (i=2; i>=0; --i)
	    {
	    if ((hPos = scanExactLeft(buf, nGap+i, hGap, hEnd)) != NULL)
		{
		AllocVar(ff);
		ff->nStart = nStart;
		ff->nEnd = nEnd;
		ff->hStart = hPos;
		ff->hEnd = hPos + nGap + i;
		return ff;
		}
	    }
	}
    else if (scanRight)
	{
	memcpy(buf+2, nStart, nGap);
	if (isRc)
	    {
	    buf[0] = 'a';
	    buf[1] = 'c';
	    }
	else
	    {
	    buf[0] = 'a';
	    buf[1] = 'g';
	    }
	for (i=0; i<=2; ++i)
	    {
	    if ((hPos = scanExactRight(buf+i, nGap+2-i, hGap, hStart)) != NULL)
		{
		AllocVar(ff);
		ff->nStart = nStart;
		ff->nEnd = nEnd;
		ff->hStart = hPos;
		ff->hEnd = hPos + nGap + i;
		return ff;
		}
	    }
	}
    else
	{
	memcpy(buf+2, nStart, nGap);
	if (isRc)
	    {
	    buf[0] = 'a';
	    buf[1] = 'c';
	    buf[nGap+2] = 'c';
	    buf[nGap+3] = 't';
	    }
	else
	    {
	    buf[0] = 'a';
	    buf[1] = 'g';
	    buf[nGap+2] = 'g';
	    buf[nGap+3] = 't';
	    }
	for (i=0; i<=2; ++i)
	    {
	    if ((hPos = scanExactRight(buf+i, nGap+4-2*i, hGap, hStart)) != NULL)
		{
		AllocVar(ff);
		ff->nStart = nStart;
		ff->nEnd = nEnd;
		ff->hStart = hPos;
		ff->hEnd = hPos + nGap + i;
		return ff;
		}
	    }
	}
    }
else
    {
    if (scanLeft)
        {
	if ((hPos = scanExactLeft(nStart, nGap, hGap, hEnd)) != NULL)
	    {
	    ff->nStart = nStart;
	    ff->nEnd = nEnd;
	    ff->hStart = hPos;
	    ff->hEnd = hPos + nGap;
	    return ff;
	    }
	}
    else
        {
	if ((hPos = scanExactRight(nStart, nGap, hGap, hStart)) != NULL)
	    {
	    AllocVar(ff);
	    ff->nStart = nStart;
	    ff->nEnd = nEnd;
	    ff->hStart = hPos;
	    ff->hEnd = hPos + nGap;
	    return ff;
	    }
	}
    }
return NULL;
}

struct ffAli *frizzyFindFromSmallerSeeds(char *nStart, char *nEnd, 
	char *hStart, char *hEnd, boolean isRc, 
	boolean scanLeft, boolean scanRight, int seedSize)
/* Look for matches with smaller seeds. */
{
int nGap = nEnd - nStart;
if (nGap >= seedSize && nGap < 80)
    {
    struct ffAli *ffList;
    if (scanLeft || scanRight)
        {
	int hGap = hEnd - hStart;
	int maxSize = seedResolvePower(seedSize);
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

struct ffAli *frizzyFind(char *nStart, char *nEnd, char *hStart, char *hEnd, 
	boolean isRc, boolean scanLeft, boolean scanRight)
/* Try and add some exon candidates in the region. */
{
struct ffAli *ff;
int nGap = nEnd - nStart;
if (nGap < gfIndexShouldFindSize)
    return NULL;
ff = exactPlusSplice(nStart, nEnd, hStart, hEnd, isRc, scanLeft, scanRight);
if (ff != NULL)
    return ff;
return frizzyFindFromSmallerSeeds(nStart, nEnd, hStart, hEnd, isRc,
	scanLeft, scanRight, 11);
}


static struct ffAli *scanForTinyExons(struct dnaSeq *qSeq, struct dnaSeq *tSeq,
	boolean isRc, struct ffAli *ffList)
/* Look for exons too small to be caught by index. */
{
struct ffAli *extraList = NULL, *ff = ffList, *lastFf = NULL, *newFf;

/* Look for initial gap. */
newFf = frizzyFind(qSeq->dna, ff->nStart, tSeq->dna, ff->hStart, 
	isRc, TRUE, FALSE); 
ffCat(&extraList, &newFf);

/* Look for middle gaps. */
for (;;)
    {
    lastFf = ff;
    ff = ff->right;
    if (ff == NULL)
	break;
    newFf = frizzyFind(lastFf->nEnd, ff->nStart, lastFf->hEnd, ff->hStart, 
	isRc, FALSE, FALSE);
    ffCat(&extraList, &newFf);
    }

/* Look for end gaps. */
newFf = frizzyFind(lastFf->nEnd, qSeq->dna + qSeq->size, 
	lastFf->hEnd, tSeq->dna + tSeq->size, isRc, FALSE, TRUE);
ffCat(&extraList, &newFf);

ffList = foldInExtras(qSeq, tSeq, ffList, extraList);
return ffList;
}

static boolean tradeMismatchToCloseSpliceGap( struct ffAli *left, struct ffAli *right, 
	int orientation)
/* Try extending one side or the other to close gap caused by
 * mismatch near splice site */
{
if (intronOrientation(left->hEnd+1, right->hStart) == orientation)
    {
    left->hEnd += 1;
    left->nEnd += 1;
    return;
    }
if (intronOrientation(left->hEnd, right->hStart-1) == orientation)
    {
    right->hStart -= 1;
    right->nStart -= 1;
    }
}

static struct ffAli *hardRefineSplice(struct ffAli *left, struct ffAli *right,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, struct ffAli *ffList)
/* Do difficult refinement of splice site.  See if
 * can get nice splice sites without breaking too much. */
{
/* Strategy - peel back 6 bases on either side of putative intron.
 * try to realign with good intron ends. */
return ffList;
}

static struct ffAli *refineSpliceSites(struct dnaSeq *qSeq, struct dnaSeq *tSeq,
	struct ffAli *ffList)
/* Try and get a little closer to splice site consensus
 * by jiggle things a little. */
{
int orientation = ffIntronOrientation(ffList);
struct ffAli *ff, *nextFf;
uglyf("refineSpliceSites:\n");
dumpFf(ffList, qSeq->dna, tSeq->dna);
if (orientation == 0)
    return ffList;
uglyf("orientation = %d\n", orientation);
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
	uglyf("Refining....\n");
	uglyf("nGap %d, hGap %d\n", nGap, hGap);
	if (nGap == 1)
	    {
	    if (tradeMismatchToCloseSpliceGap(ff, nextFf, orientation))
	        continue;
	    }
	ffList = hardRefineSplice(ff, nextFf, qSeq, tSeq, ffList);
	}
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
/* Find target of given name.  Return NULL if none. */
for (ffi = bun->ffList; ffi != NULL; ffi = ffi->next)
    {
    ffi->ff = scanIndexForSmallExons(gf, target, qSeq, qMaskBits, qMaskOffset, 
	tSeq, lm, ffi->ff);
    ffi->ff = bandedExtend(qSeq, tSeq, ffi->ff);
    ffi->ff = scanForTinyExons(qSeq, tSeq, isRc, ffi->ff);
    ffi->ff = refineSpliceSites(qSeq, tSeq, ffi->ff);
    }
}


struct ssBundle *gfSeedExtInMem(struct genoFind *gf, struct dnaSeq *qSeq, Bits *qMaskBits, 
	int qOffset, struct lm *lm, int minScore, boolean isRc)
/* Do seed and extend type alignment */
{
struct ssBundle *bunList = NULL, *bun;
int hitCount, aliCount;
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
    bun->data = range;
    bun->ffList = gfRangesToFfItem(range->components, qSeq);
    bun->isProt = FALSE;
    bun->avoidFuzzyFindKludge = TRUE;
    aliCount = ssStitch(bun, ffCdna, 16);
    refineBundle(gf, qSeq, qMaskBits, qOffset, tSeq, lm, bun, isRc);
    slAddHead(&bunList, bun);
    }
gfRangeFreeList(&rangeList);
gfClumpFreeList(&clumpList);
return bunList;
}

