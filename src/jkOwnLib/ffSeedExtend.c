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


struct ffAli *bandExtFf(
	struct lm *lm,	/* Local memory pool, NULL to use global allocation for ff */
	struct axtScoreScheme *ss, 	/* Scoring scheme to use. */
	int maxInsert,			/* Maximum number of inserts to allow. */
	struct ffAli *origFf,		/* Alignment block to extend. */
	char *nStart, char *nEnd,	/* Bounds of region to extend through */
	char *hStart, char *hEnd,	/* Bounds of region to extend through */
	int dir,			/* +1 to extend end, -1 to extend start */
	int maxExt);			/* Maximum length of extension. */

/* ### */

static void bandExtBefore(struct axtScoreScheme *ss, struct ffAli *ff,
	int qGap, int tGap, struct dnaSeq *qSeq, struct dnaSeq *tSeq,
	struct ffAli **pExtraList)
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
	int qGap, int tGap, struct dnaSeq *qSeq, struct dnaSeq *tSeq,
	struct ffAli **pExtraList)
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
bandExtBefore(ss, ff, qGap, tGap, qSeq, tSeq, &extraList);

/* Look for middle gaps. */
for (;;)
    {
    lastFf = ff;
    ff = ff->right;
    if (ff == NULL)
	break;
    qGap = ff->nStart - lastFf->nEnd;
    tGap = ff->hStart - lastFf->hEnd;
    bandExtAfter(ss, lastFf, qGap, tGap, qSeq, tSeq, &extraList);
    bandExtBefore(ss, ff, qGap, tGap, qSeq, tSeq, &extraList);
    }

/* Look for end gaps. */
qGap = qSeq->dna + qSeq->size - lastFf->nEnd;
tGap = tSeq->dna + tSeq->size - lastFf->hEnd;
bandExtAfter(ss, lastFf, qGap, tGap, qSeq, tSeq, &extraList);

ffList = foldInExtras(qSeq, tSeq, ffList, extraList);
return ffList;
}

static void refineBundle(struct genoFind *gf, 
	struct dnaSeq *qSeq,  Bits *qMaskBits, int qMaskOffset,
	struct dnaSeq *tSeq, struct lm *lm, struct ssBundle *bun)
/* Refine bundle - extending alignments and looking for smaller exons. */
{
struct ssFfItem *ffi;
struct gfSeqSource *target = gfFindNamedSource(gf, tSeq->name);
/* Find target of given name.  Return NULL if none. */
for (ffi = bun->ffList; ffi != NULL; ffi = ffi->next)
    {
    ffi->ff = scanIndexForSmallExons(gf, target, qSeq, qMaskBits, qMaskOffset, tSeq, lm, ffi->ff);
    ffi->ff = bandedExtend(qSeq, tSeq, ffi->ff);
    }
}


struct ssBundle *gfSeedExtInMem(struct genoFind *gf, struct dnaSeq *qSeq, Bits *qMaskBits, 
	int qOffset, struct lm *lm, int minScore)
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
    refineBundle(gf, qSeq, qMaskBits, qOffset, tSeq, lm, bun);
    slAddHead(&bunList, bun);
    bun->avoidFuzzyFindKludge = FALSE;
    }
return bunList;
}

