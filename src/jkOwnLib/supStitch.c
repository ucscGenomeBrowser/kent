/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* supStitch stitches together a bundle of ffAli alignments that share
 * a common query and target sequence into larger alignments if possible.
 * This is commonly used when the query sequence was broken up into
 * overlapping blocks in the initial alignment, and also to look for
 * introns larger than fuzzyFinder can handle. */

#include "common.h"
#include "dnautil.h"
#include "fuzzyFind.h"
#include "localmem.h"
#include "patSpace.h"
#include "trans3.h"
#include "supStitch.h"
#include "chainBlock.h"

struct ssGraph
/* The whole tree.  */
    {
    struct kdBranch *root;	/* Pointer to root of kd-tree. */
    };


void ssFfItemFree(struct ssFfItem **pEl)
/* Free a single ssFfItem. */
{
struct ssFfItem *el;
if ((el = *pEl) != NULL)
    {
    ffFreeAli(&el->ff);
    freez(pEl);
    }
}

void ssFfItemFreeList(struct ssFfItem **pList)
/* Free a list of ssFfItems. */
{
struct ssFfItem *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ssFfItemFree(&el);
    }
*pList = NULL;
}

void ssBundleFree(struct ssBundle **pEl)
/* Free up one ssBundle */
{
struct ssBundle *el;
if ((el = *pEl) != NULL)
    {
    ssFfItemFreeList(&el->ffList);
    freez(pEl);
    }
}

void ssBundleFreeList(struct ssBundle **pList)
/* Free up list of ssBundles */
{
struct ssBundle *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ssBundleFree(&el);
    }
*pList = NULL;
}

void dumpNearCrossover(char *what, DNA *n, DNA *h, int size)
/* Print sequence near crossover */
{
int i;

printf("%s: ", what);
mustWrite(stdout, n, size);
printf("\n");
printf("%s: ", what);
mustWrite(stdout, h, size);
printf("\n");
}

void dumpFf(struct ffAli *left, DNA *needle, DNA *hay)
/* Print info on ffAli. */
{
struct ffAli *ff;
for (ff = left; ff != NULL; ff = ff->right)
    {
    printf("(%d - %d)[%d-%d] ", ff->hStart-hay, ff->hEnd-hay,
	ff->nStart - needle, ff->nEnd - needle);
    }
printf("\n");
}

void dumpBuns(struct ssBundle *bunList)
/* Print diagnostic info on bundles. */
{
struct ssBundle *bun;
struct ssFfItem *ffl;
for (bun = bunList; bun != NULL; bun = bun->next)
    {
    struct dnaSeq *qSeq = bun->qSeq;        /* Query sequence (not owned by bundle.) */
    struct dnaSeq *genoSeq = bun->genoSeq;     /* Genomic sequence (not owned by bundle.) */
    printf("Bundle of %d between %s and %s\n", slCount(bun->ffList), qSeq->name, genoSeq->name);
    for (ffl = bun->ffList; ffl != NULL; ffl = ffl->next)
	{
	dumpFf(ffl->ff, bun->qSeq->dna, bun->genoSeq->dna);
	}
    }
}


static int bioScoreMatch(boolean isProt, char *a, char *b, int size)
/* Return score of match (no inserts) between two bio-polymers. */
{
if (isProt)
    {
    return dnaOrAaScoreMatch(a, b, size, 2, -1, 'X');
    }
else
    {
    return dnaOrAaScoreMatch(a, b, size, 1, -1, 'n');
    }
}

// boolean uglier;

static int findCrossover(struct ffAli *left, struct ffAli *right, int overlap, boolean isProt)
/* Find ideal crossover point of overlapping blocks.  That is
 * the point where we should start using the right block rather
 * than the left block.  This point is an offset from the start
 * of the overlapping region (which is the same as the start of the
 * right block). */
{
int bestPos = 0;
char *nStart = right->nStart;
char *lhStart = left->hEnd - overlap;
char *rhStart = right->hStart;
int i;
int (*scoreMatch)(char a, char b);
int score, bestScore;

if (isProt)
    {
    scoreMatch = aaScore2;
    score = bestScore = aaScoreMatch(nStart, rhStart, overlap);
    }
else
    {
    scoreMatch = dnaScore2;
    score = bestScore = dnaScoreMatch(nStart, rhStart, overlap);
    }

for (i=0; i<overlap; ++i)
    {
    char n = nStart[i];
    score += scoreMatch(lhStart[i], n);
    score -= scoreMatch(rhStart[i], n);
    if (score > bestScore)
	{
	bestScore = score;
	bestPos = i+1;
	}
    }
// if (uglier) uglyf("crossover at %d\n\n", bestPos);
return bestPos;
}

static void trans3Offsets(struct trans3 *t3List, AA *startP, AA *endP,
	int *retStart, int *retEnd)
/* Figure out offset of peptide in context of larger sequences. */
{
struct trans3 *t3;
int frame;
aaSeq *seq;
int startOff;

for (t3 = t3List; t3 != NULL; t3 = t3->next)
    {
    for (frame = 0; frame < 3; ++frame)
        {
	seq = t3->trans[frame];
	if (seq->dna <= startP && startP < seq->dna + seq->size)
	    {
	    *retStart = startP - seq->dna + t3->start;
	    *retEnd = endP - seq->dna + t3->start;
	    return;
	    }
	}
    }
internalErr();
}

static struct ffAli *ffMergeExactly(struct ffAli *aliList)
/* Remove overlapping areas needle in alignment. Assumes ali is sorted on
 * ascending nStart field. Also merge perfectly abutting neighbors.*/
{
struct ffAli *mid, *ali;

for (mid = aliList->right; mid != NULL; mid = mid->right)
    {
    for (ali = aliList; ali != mid; ali = ali->right)
	{
	DNA *nStart, *nEnd;
	int nOverlap, diag;
	nStart = max(ali->nStart, mid->nStart);
	nEnd = min(ali->nEnd, mid->nStart);
	nOverlap = nEnd - nStart;
	/* Overlap or perfectly abut in needle, and needle/hay
	 * offset the same. */
	if (nOverlap >= 0)
	    {
	    int diag = ali->nStart - ali->hStart;
	    if (diag == mid->nStart - mid->hStart)
		{
		/* Make mid encompass both, and make ali empty. */
		mid->nStart = min(ali->nStart, mid->nStart);
		mid->nEnd = max(ali->nEnd, mid->nEnd);
		mid->hStart = mid->nStart-diag;
		mid->hEnd = mid->nEnd-diag;
		ali->hEnd = mid->hStart;
		ali->nEnd = mid->nStart;
		}
	    }
	}
    }
aliList = ffRemoveEmptyAlis(aliList, TRUE);
return aliList;
}

int cmpFflTrimScore(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct ssFfItem *a = *((struct ssFfItem **)va);
const struct ssFfItem *b = *((struct ssFfItem **)vb);
return b->trimScore - a->trimScore;
}

static boolean trimBundle(struct ssBundle *bundle, int maxFfCount,
	enum ffStringency stringency)
/* Trim out the relatively insignificant alignments from bundle. */
{
struct ssFfItem *ffl, *newFflList = NULL, *lastFfl = NULL;
int ffCountAll = 0;
int ffCountOne;

for (ffl = bundle->ffList; ffl != NULL; ffl = ffl->next)
    ffl->trimScore = ffScoreSomething(ffl->ff, stringency, bundle->isProt);
slSort(&bundle->ffList, cmpFflTrimScore);

for (ffl = bundle->ffList; ffl != NULL; ffl = ffl->next)
    {
    ffCountOne = ffAliCount(ffl->ff);
    if (ffCountOne + ffCountAll <= maxFfCount)
	ffCountAll += ffCountOne;
    else
	{
	if (lastFfl == NULL)
	    {
	    warn("Can't stitch, single alignment more than %d blocks", maxFfCount);
	    return FALSE;
	    }
	lastFfl->next = NULL;
	ssFfItemFreeList(&ffl);
	break;
	}
    lastFfl = ffl;
    }
return TRUE;
}

struct ffAli *smallMiddleExons(struct ffAli *aliList, struct ssBundle *bundle, 
	enum ffStringency stringency)
/* Look for small exons in the middle. */
{
if (bundle->t3List != NULL)
    return aliList;	/* Can't handle intense translated stuff. */
else
    {
    struct dnaSeq *qSeq =  bundle->qSeq;
    struct dnaSeq *genoSeq = bundle->genoSeq;
    struct ffAli *right, *left = NULL, *newLeft, *newRight;

    left = aliList;
    for (right = aliList->right; right != NULL; right = right->right)
        {
	if (right->hStart - left->hEnd >= 3 && right->nStart - left->nEnd >= 3)
	    {
	    newLeft = ffFind(left->nEnd, right->nStart, left->hEnd, right->hStart, stringency);
	    if (newLeft != NULL)
	        {
		newRight = ffRightmost(newLeft);
                if (left != NULL)
                    {
                    left->right = newLeft;
                    newLeft->left = left;
                    }
                else
                    {
                    aliList = newLeft;
                    }
                if (right != NULL)
                    {
                    right->left = newRight;
                    newRight->right = right;
                    }
		}
	    }
	left = right;
	}
    }
return aliList;
}

static enum ffStringency ssStringency;
static boolean ssIsProt;

static int ssGapCost(int dq, int dt)
/* Return gap penalty. */
{
if (dt < 0) dt = 0;
if (dq < 0) dq = 0;
return ffCalcGapPenalty(dt, dq, ssStringency);
}


static int findOverlap(struct boxIn *a, struct boxIn *b)
/* Figure out how much a and b overlap on either sequence. */
{
int dq = b->qStart - a->qEnd;
int dt = b->tStart - a->tEnd;
return -min(dq, dt);
}

static int ssConnectCost(struct boxIn *a, struct boxIn *b)
/* Calculate connection cost - including gap score
 * and overlap adjustments if any. */
{
struct ffAli *aFf = a->data, *bFf = b->data;
int overlapAdjustment = 0;
int overlap = findOverlap(a, b);
int dq = b->qStart - a->qEnd;
int dt = b->tStart - a->tEnd;

if (overlap > 0)
    {
    int aSize = aFf->hEnd - aFf->hStart;
    int bSize = bFf->hEnd - bFf->hStart;
    if (overlap >= bSize || overlap >= aSize)
	{
	/* Give stiff overlap adjustment for case where
	 * one block completely enclosed in the other on
	 * either sequence. This will make it never happen. */
	overlapAdjustment = a->score + b->score;
	}
    else
        {
	/* More normal case - partial overlap on one or both strands. */
	int crossover = findCrossover(aFf, bFf, overlap, ssIsProt);
	int remain = overlap - crossover;
	overlapAdjustment =
	    bioScoreMatch(ssIsProt, aFf->nEnd - remain, aFf->hEnd - remain, 
	    	remain)
	  + bioScoreMatch(ssIsProt, bFf->nStart, bFf->hStart, 
	  	crossover);
	dq -= overlap;
	dt -= overlap;
	}
    }
return overlapAdjustment + ssGapCost(dq, dt);
}


static void ssFindBest(struct ffAli *ffList, bioSeq *qSeq, bioSeq *tSeq,
	enum ffStringency stringency, boolean isProt, struct trans3 *t3List,
	struct ffAli **retBestAli, int *retScore, struct ffAli **retLeftovers)
/* Go set up things to call chainBlocks. */
{
struct boxIn *boxList = NULL, *box, *prevBox;
struct ffAli *ff;
struct lm *lm = lmInit(0);
int boxSize;
DNA *firstH = tSeq->dna;
struct chain *chainList, *chain, *bestChain;
int tMin = BIGNUM, tMax = -BIGNUM;
struct ffAli **newList;


/* Make up box list for chainer. */
// if (uglier) uglyf("ssFindBest on %d\n", ffAliCount(ffList));
for (ff = ffList; ff != NULL; ff = ff->right)
    {
    lmAllocVar(lm, box);
    boxSize = ff->nEnd - ff->nStart;
    box->qStart = ff->nStart - qSeq->dna;
    box->qEnd = box->qStart + boxSize;
    if (t3List)
        {
	trans3Offsets(t3List, ff->hStart, ff->hEnd, &box->tStart, &box->tEnd);
	}
    else
        {
	box->tStart = ff->hStart - firstH;
	box->tEnd = box->tStart + boxSize;
	}
    box->data = ff;
    box->score = bioScoreMatch(isProt, ff->nStart, ff->hStart, boxSize);
    if (tMin > box->tStart) tMin = box->tStart;
    if (tMax < box->tEnd) tMax = box->tEnd;
    slAddHead(&boxList, box);
    // if (uglier) uglyf(" %d,%d %d,%d size %d, score %d\n", box->qStart, box->qEnd, box->tStart, box->tEnd, box->qEnd - box->qStart, box->score);
    }

/* Adjust boxes so that tMin is always 0. */
for (box = boxList; box != NULL; box = box->next)
    {
    box->tStart -= tMin;
    box->tEnd -= tMin;
    }
tMax -= tMin;

ssStringency = stringency;
ssIsProt = isProt;
chainList = chainBlocks(qSeq->name, qSeq->size, '+', "tSeq", tMax, &boxList,
	ssConnectCost, ssGapCost);
// if (uglier) uglyf("%d chains\n", slCount(chainList));

/* Fixup crossovers on best (first) chain. */
bestChain = chainList;
prevBox = bestChain->blockList;
for (box = prevBox->next; box != NULL; box = box->next)
    {
    int dq = box->qStart - prevBox->qEnd;
    int dt = box->tStart - prevBox->tEnd;
    int overlap = findOverlap(prevBox, box);
    if (overlap > 0)
        {
	struct ffAli *left = prevBox->data;
	struct ffAli *right = box->data;
	int crossover = findCrossover(left, right, overlap, isProt);
        int remain = overlap - crossover;
	left->nEnd -= remain;
	left->hEnd -= remain;
	right->nStart += crossover;
	right->hStart += crossover;
	// if (uglier) uglyf("overlap %d, crossover %d\n", overlap, crossover);
	}
    prevBox = box;
    }

/* Copy stuff from first chain to bestAli, and from rest of
 * chains to leftovers. */
*retBestAli = NULL;
*retLeftovers = NULL;
newList = retBestAli;
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    struct ffAli *farRight = NULL;
    for (box = chain->blockList; box != NULL; box = box->next)
        {
	ff = box->data;
	ff->left = farRight;
	farRight = ff;
	}
    for (ff = farRight; ff != NULL; ff = ff->left)
        {
	ff->right = *newList;
	*newList = ff;
	}
    newList = retLeftovers;
    chain->blockList = NULL;	/* Don't want to free this, it's local. */
    }

*retScore = bestChain->score;
chainFreeList(&chainList);
lmCleanup(&lm);
}


int ssStitch(struct ssBundle *bundle, enum ffStringency stringency)
/* Glue together mrnas in bundle as much as possible. Returns number of
 * alignments after stitching. Updates bundle->ffList with stitched
 * together version. */
{
struct dnaSeq *qSeq =  bundle->qSeq;
struct dnaSeq *genoSeq = bundle->genoSeq;
struct ffAli *ff, *ffList = NULL;
struct ssFfItem *ffl;
struct ffAli *bestPath, *leftovers;
struct ssGraph *graph;
int score;
int newAliCount = 0;
int totalFfCount = 0;
int trimCount = qSeq->size/200 + genoSeq->size/1000 + 2000;
boolean firstTime = TRUE;

if (bundle->ffList == NULL)
    return 0;



/* Create ffAlis for all in bundle and move to one big list. */
for (ffl = bundle->ffList; ffl != NULL; ffl = ffl->next)
    ffCat(&ffList, &ffl->ff);
slFreeList(&bundle->ffList);

// uglyf("%s with %d\n", qSeq->name, ffAliCount(ffList));
// uglier = (ffAliCount(ffList) == 9 || ffAliCount(ffList) == 10);

ffAliSort(&ffList, ffCmpHitsNeedleFirst);
ffList = ffMergeExactly(ffList);
// if (uglier) uglyf(" %d after ffMergeExactly\n", ffAliCount(ffList));

while (ffList != NULL)
    {
    ssFindBest(ffList, qSeq, genoSeq, stringency, 
    	bundle->isProt, bundle->t3List,
    	&bestPath, &score, &ffList);

    bestPath = ffMergeNeedleAlis(bestPath, TRUE);
    bestPath = ffRemoveEmptyAlis(bestPath, TRUE);
    bestPath = ffMergeHayOverlaps(bestPath);
    bestPath = ffRemoveEmptyAlis(bestPath, TRUE);
    if (firstTime && stringency == ffCdna)
	{
	/* Only look for middle exons the first time.  Next times
	 * this might regenerate most of the first alignment... */
	bestPath = smallMiddleExons(bestPath, bundle, stringency);
	}
    if (!bundle->isProt)
	ffSlideIntrons(bestPath);
    bestPath = ffMergeNeedleAlis(bestPath, TRUE);
    bestPath = ffRemoveEmptyAlis(bestPath, TRUE);
    if (score >= 20)
	{
	AllocVar(ffl);
	ffl->ff = bestPath;
	slAddHead(&bundle->ffList, ffl);
	++newAliCount;
	}
    else
	{
	ffFreeAli(&bestPath);
	}
    firstTime = FALSE;
    // uglier = FALSE;
    }
slReverse(&bundle->ffList);
return newAliCount;
}

static struct ssBundle *findBundle(struct ssBundle *list,  
	struct dnaSeq *genoSeq)
/* Find bundle in list that has matching genoSeq pointer, or NULL
 * if none such.  This routine is used by psLayout but not blat. */
{
struct ssBundle *el;
for (el = list; el != NULL; el = el->next)
    if (el->genoSeq == genoSeq)
	return el;
return NULL;
}

struct ssBundle *ssFindBundles(struct patSpace *ps, struct dnaSeq *cSeq, 
	char *cName, enum ffStringency stringency, boolean avoidSelfSelf)
/* Find patSpace alignments.  This routine is used by psLayout but not blat. */
{
struct patClump *clumpList, *clump;
struct ssBundle *bundleList = NULL, *bun = NULL;
DNA *cdna = cSeq->dna;
int totalCdnaSize = cSeq->size;
DNA *endCdna = cdna+totalCdnaSize;
struct ssFfItem *ffl;
struct dnaSeq *lastSeq = NULL;
int maxSize = 700;
int preferredSize = 500;
int overlapSize = 250;

for (;;)
    {
    int cSize = endCdna - cdna;
    if (cSize > maxSize)
	cSize = preferredSize;
    clumpList = patSpaceFindOne(ps, cdna, cSize);
    for (clump = clumpList; clump != NULL; clump = clump->next)
	{
	struct ffAli *ff;
	struct dnaSeq *seq = clump->seq;
	DNA *tStart = seq->dna + clump->start;
	if (!avoidSelfSelf || !sameString(seq->name, cSeq->name))
	    {
	    ff = ffFind(cdna, cdna+cSize, tStart, tStart + clump->size, stringency);
	    if (ff != NULL)
		{
		if (lastSeq != seq)
		    {
		    lastSeq = seq;
		    if ((bun = findBundle(bundleList, seq)) == NULL)
			{
			AllocVar(bun);
			bun->qSeq = cSeq;
			bun->genoSeq = seq;
			bun->genoIx = clump->bacIx;
			bun->genoContigIx = clump->seqIx;
			slAddHead(&bundleList, bun);
			}
		    }
		AllocVar(ffl);
		ffl->ff = ff;
		slAddHead(&bun->ffList, ffl);
		}
	    }
	}
    cdna += cSize;
    if (cdna >= endCdna)
	break;
    cdna -= overlapSize;
    slFreeList(&clumpList);
    }
slReverse(&bundleList);
cdna = cSeq->dna;

for (bun = bundleList; bun != NULL; bun = bun->next)
    {
    ssStitch(bun, stringency);
    }
return bundleList;
}

