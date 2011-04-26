/* supStitch stitches together a bundle of ffAli alignments that share
 * a common query and target sequence into larger alignments if possible.
 * This is commonly used when the query sequence was broken up into
 * overlapping blocks in the initial alignment, and also to look for
 * introns larger than fuzzyFinder can handle. */
/* Copyright 2000-2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "dnautil.h"
#include "dlist.h"
#include "fuzzyFind.h"
#include "localmem.h"
#include "patSpace.h"
#include "trans3.h"
#include "supStitch.h"
#include "chainBlock.h"

static char const rcsid[] = "$Id: supStitch.c,v 1.38 2007/04/20 22:43:37 kent Exp $";

static void ssFindBestBig(struct ffAli *ffList, bioSeq *qSeq, bioSeq *tSeq,
	enum ffStringency stringency, boolean isProt, struct trans3 *t3List,
	struct ffAli **retBestAli, int *retScore, struct ffAli **retLeftovers);

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
    printf("(%ld - %ld)[%ld-%ld] ", (long)(ff->hStart-hay), (long)(ff->hEnd-hay),
	(long)(ff->nStart - needle), (long)(ff->nEnd - needle));
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
return bestPos;
}

static void trans3Offsets(struct trans3 *t3List, AA *startP, AA *endP,
	int *retStart, int *retEnd)
/* Figure out offset of peptide in context of larger sequences. */
{
struct trans3 *t3;
int frame;
aaSeq *seq;

for (t3 = t3List; t3 != NULL; t3 = t3->next)
    {
    for (frame = 0; frame < 3; ++frame)
        {
	seq = t3->trans[frame];
	if (seq->dna <= startP && startP < seq->dna + seq->size)
	    {
	    *retStart = 3*(startP - seq->dna)+frame + t3->start;
	    *retEnd = 3*(endP - seq->dna)+frame + t3->start;
	    return;
	    }
	}
    }
internalErr();
}

static boolean isMonotonic(struct ffAli *left)
/* Return TRUE if this is monotonic on both query and target */
{
struct ffAli *right;

while ((right = left->right) != NULL)
    {
    if (left->nEnd <= right->nStart && left->hEnd <= right->hStart)
	left = right;
    else
	{
	verbose(2, "not monotonic dn %d, dh %d\n", (int)(right->nStart - left->nEnd), 
		(int)(right->hStart - right->hEnd));
        return FALSE;
	}
    }
return TRUE;
}

static struct ffAli *forceMonotonic(struct ffAli *aliList,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, enum ffStringency stringency,
	boolean isProt, struct trans3 *t3List)
/* Remove any blocks that violate strictly increasing order in both coordinates. 
 * This is not optimal, but it turns out to be very rarely used. */
{
if (!isProt)
    {
    if (!isMonotonic(aliList))
	{
	struct ffAli *leftovers = NULL;
	int score;
	ssFindBestBig(aliList, qSeq, tSeq, stringency, isProt, t3List, &aliList, &score,
	   &leftovers);
	ffFreeAli(&leftovers);
	}
    }
return aliList;
}

struct ffAli *smallMiddleExons(struct ffAli *aliList, 
	struct ssBundle *bundle, 
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
		newLeft = forceMonotonic(newLeft, qSeq, genoSeq, 
		    stringency, bundle->isProt, bundle->t3List );
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

struct ssNode
/* Node of superStitch graph. */
    {
    struct ssEdge *waysIn;	/* Edges leading into this node. */
    struct ffAli *ff;           /* Alignment block associated with node. */
    struct ssEdge *bestWayIn;   /* Dynamic programming best edge in. */
    int cumScore;               /* Dynamic programming score of edge. */
    int nodeScore;              /* Score of this node. */
    };

struct ssEdge
/* Edge of superStitch graph. */
    {
    struct ssEdge *next;         /* Next edge in waysIn list. */
    struct ssNode *nodeIn;       /* The node that leads to this edge. */
    int score;                   /* Score you get taking this edge. */
    int overlap;                 /* Overlap between two nodes. */
    int crossover;               /* Offset from overlap where crossover occurs. */
    };

struct ssGraph
/* Super stitcher graph for dynamic programming to find best
 * way through. */
    {
    int nodeCount;		/* Number of nodes. */
    struct ssNode *nodes;       /* Array of nodes. */
    int edgeCount;              /* Number of edges. */
    struct ssEdge *edges;       /* Array of edges. */
    };

static void ssGraphFree(struct ssGraph **pGraph)
/* Free graph. */
{
struct ssGraph *graph;
if ((graph = *pGraph) != NULL)
    {
    freeMem(graph->nodes);
    freeMem(graph->edges);
    freez(pGraph);
    }
}

boolean tripleCanFollow(struct ffAli *a, struct ffAli *b, aaSeq *qSeq, struct trans3 *t3List)
/* Figure out if a can follow b in any one of three reading frames of haystack. */
{
int ahStart, ahEnd, bhStart, bhEnd;
trans3Offsets(t3List, a->hStart, a->hEnd, &ahStart, &ahEnd);
trans3Offsets(t3List, b->hStart, b->hEnd, &bhStart, &bhEnd);
return  (a->nStart < b->nStart && a->nEnd < b->nEnd && ahStart < bhStart && ahEnd < bhEnd);
}


static struct ssGraph *ssGraphMake(struct ffAli *ffList, bioSeq *qSeq,
	enum ffStringency stringency, boolean isProt, struct trans3 *t3List)
/* Make a graph corresponding to ffList */
{
int nodeCount = ffAliCount(ffList);
int maxEdgeCount = (nodeCount+1)*(nodeCount)/2;
int edgeCount = 0;
struct ssEdge *edges, *e;
struct ssNode *nodes;
struct ssGraph *graph;
struct ffAli *ff, *mid;
int i, midIx;
int overlap;
boolean canFollow;

if (nodeCount == 1)
    maxEdgeCount = 1;
    
AllocVar(graph);
graph->nodeCount = nodeCount;
graph->nodes = AllocArray(nodes, nodeCount+1);
for (i=1, ff = ffList; i<=nodeCount; ++i, ff = ff->right)
    {
    nodes[i].ff = ff;
    nodes[i].nodeScore = bioScoreMatch(isProt, ff->nStart, ff->hStart, ff->hEnd - ff->hStart);
    }

graph->edges = AllocArray(edges, maxEdgeCount);
for (mid = ffList, midIx=1; mid != NULL; mid = mid->right, ++midIx)
    {
    int midScore;
    struct ssNode *midNode = &nodes[midIx];
    e = &edges[edgeCount++];
    assert(edgeCount <= maxEdgeCount);
    e->nodeIn = &nodes[0];
    e->score = midScore = midNode->nodeScore;
    midNode->waysIn = e;
    for (ff = ffList,i=1; ff != mid; ff = ff->right,++i)
	{
	int mhStart = 0, mhEnd = 0;
	if (t3List)
	    {
	    canFollow = tripleCanFollow(ff, mid, qSeq, t3List);
	    trans3Offsets(t3List, mid->hStart, mid->hEnd, &mhStart, &mhEnd);
	    }
	else 
	    {
	    canFollow = (ff->nStart < mid->nStart && ff->nEnd < mid->nEnd 
			&& ff->hStart < mid->hStart && ff->hEnd < mid->hEnd);
	    }
	if (canFollow)
	    {
	    struct ssNode *ffNode = &nodes[i];
	    int score;
	    int hGap;
	    int nGap;
	    int crossover;

	    nGap = mid->nStart - ff->nEnd;
	    if (t3List)
	        {
		int fhStart, fhEnd;
		trans3Offsets(t3List, ff->hStart, ff->hEnd, &fhStart, &fhEnd);
		hGap = mhStart - fhEnd;
		}
	    else
		{
		hGap = mid->hStart - ff->hEnd;
		}
	    e = &edges[edgeCount++];
	    assert(edgeCount <= maxEdgeCount);
	    e->nodeIn = ffNode;
	    e->overlap = overlap = -nGap;
	    if (overlap > 0)
		{
		int midSize = mid->hEnd - mid->hStart;
		int ffSize = ff->hEnd - ff->hStart;
		int newMidScore, newFfScore;
		e->crossover = crossover = findCrossover(ff, mid, overlap, isProt);
		newMidScore = bioScoreMatch(isProt, mid->nStart, mid->hStart, midSize-overlap+crossover);
		newFfScore = bioScoreMatch(isProt, ff->nStart+crossover, ff->hStart+crossover,
				ffSize-crossover);
		score = newMidScore - ffNode->nodeScore + newFfScore;
		nGap = 0;
		hGap -= overlap;
		}
	    else
		{
		score = midScore;
		}
	    score -= ffCalcGapPenalty(hGap, nGap, stringency);
	    e->score = score;
	    slAddHead(&midNode->waysIn, e);
	    }
	}
    slReverse(&midNode->waysIn);
    }
return graph;
}

static struct ssNode *ssDynamicProgram(struct ssGraph *graph)
/* Do dynamic programming to find optimal path though
 * graph.  Return best ending node (with back-trace
 * pointers and score set). */
{
int veryBestScore = -0x3fffffff;
struct ssNode *veryBestNode = NULL;
int nodeIx;

for (nodeIx = 1; nodeIx <= graph->nodeCount; ++nodeIx)
    {
    int bestScore = -0x3fffffff;
    int score;
    struct ssEdge *bestEdge = NULL;
    struct ssNode *curNode = &graph->nodes[nodeIx];
    struct ssEdge *edge;

    for (edge = curNode->waysIn; edge != NULL; edge = edge->next)
	{
	score = edge->score + edge->nodeIn->cumScore;
	if (score > bestScore)
	    {
	    bestScore = score;
	    bestEdge = edge;
	    }
	}
    curNode->bestWayIn = bestEdge;
    curNode->cumScore = bestScore;
    if (bestScore >= veryBestScore)
	{
	veryBestScore = bestScore;
	veryBestNode = curNode;
	}
    }
return veryBestNode;
}

static void ssGraphFindBest(struct ssGraph *graph, struct ffAli **retBestAli, 
   int *retScore, struct ffAli **retLeftovers)
/* Traverse graph and put best alignment in retBestAli.  Return score.
 * Put blocks not part of best alignment in retLeftovers. */
{
struct ssEdge *edge;
struct ssNode *node;
struct ssNode *startNode = &graph->nodes[0];
struct ffAli *bestAli = NULL, *leftovers = NULL;
struct ffAli *ff, *left = NULL;
int i;
int overlap, crossover, crossDif;

/* Find best path and save score. */
node = ssDynamicProgram(graph);
*retScore = node->cumScore;

/* Trace back and trim overlaps. */
while (node != startNode)
    {
    ff = node->ff;
    ff->right = bestAli;
    bestAli = ff;
    node->ff = NULL;
    edge = node->bestWayIn;
    if ((overlap = edge->overlap) > 0)
	{
	left = edge->nodeIn->ff;
	crossover = edge->crossover;
	crossDif = overlap-crossover;
	left->hEnd -= crossDif;
	left->nEnd -= crossDif;
	ff->hStart += crossover;
	ff->nStart += crossover;
	}
    node = node->bestWayIn->nodeIn;
    }

/* Make left links. */
left = NULL;
for (ff = bestAli; ff != NULL; ff = ff->right)
    {
    ff->left = left;
    left = ff;
    }

/* Make leftover list. */
for (i=1, node=graph->nodes+1; i<=graph->nodeCount; ++i,++node)
    {
    if ((ff = node->ff) != NULL)
	{
	ff->left = leftovers;
	leftovers = ff;
	}
    }
leftovers = ffMakeRightLinks(leftovers);

*retBestAli = bestAli;
*retLeftovers = leftovers;
}

static void ssFindBestSmall(struct ffAli *ffList, bioSeq *qSeq, bioSeq *tSeq,
	enum ffStringency stringency, boolean isProt, struct trans3 *t3List,
	struct ffAli **retBestAli, int *retScore, struct ffAli **retLeftovers)
/* Build a graph and do a dynamic program on it to find best chains. 
 * For small ffLists this is faster than the stuff in chainBlock. */
{
struct ssGraph *graph = ssGraphMake(ffList, qSeq, stringency, isProt, t3List);
ssGraphFindBest(graph, retBestAli, retScore, retLeftovers);
*retBestAli = forceMonotonic(*retBestAli, qSeq, tSeq, stringency, isProt, t3List);
ssGraphFree(&graph);
}



static enum ffStringency ssStringency;
static boolean ssIsProt;

static int ssGapCost(int dq, int dt, void *data)
/* Return gap penalty.  This just need be a lower bound on 
 * the penalty actually. */
{
int cost;
if (dt < 0) dt = 0;
if (dq < 0) dq = 0;
cost = ffCalcGapPenalty(dt, dq, ssStringency);
return cost;
}


static int findOverlap(struct cBlock *a, struct cBlock *b)
/* Figure out how much a and b overlap on either sequence. */
{
int dq = b->qStart - a->qEnd;
int dt = b->tStart - a->tEnd;
return -min(dq, dt);
}

static int ssConnectCost(struct cBlock *a, struct cBlock *b, void *data)
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
return overlapAdjustment + ssGapCost(dq, dt, data);
}


static void ssFindBestBig(struct ffAli *ffList, bioSeq *qSeq, bioSeq *tSeq,
	enum ffStringency stringency, boolean isProt, struct trans3 *t3List,
	struct ffAli **retBestAli, int *retScore, struct ffAli **retLeftovers)
/* Go set up things to call chainBlocks to find best way to string together
 * blocks in alignment. */
{
struct cBlock *boxList = NULL, *box, *prevBox;
struct ffAli *ff, *farRight = NULL;
struct lm *lm = lmInit(0);
int boxSize;
DNA *firstH = tSeq->dna;
struct chain *chainList, *chain, *bestChain;
int tMin = BIGNUM, tMax = -BIGNUM;


/* Make up box list for chainer. */
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
	ssConnectCost, ssGapCost, NULL, NULL);

/* Fixup crossovers on best (first) chain. */
bestChain = chainList;
prevBox = bestChain->blockList;
for (box = prevBox->next; box != NULL; box = box->next)
    {
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
	}
    prevBox = box;
    }

/* Copy stuff from first chain to bestAli. */
farRight = NULL;
for (box = chainList->blockList; box != NULL; box = box->next)
    {
    ff = box->data;
    ff->left = farRight;
    farRight = ff;
    }
*retBestAli = ffMakeRightLinks(farRight);

/* Copy stuff from other chains to leftovers. */
farRight = NULL;
for (chain = chainList->next; chain != NULL; chain = chain->next)
    {
    for (box = chain->blockList; box != NULL; box = box->next)
        {
        ff = box->data;
	ff->left = farRight;
	farRight = ff;
	}
    }
*retLeftovers = ffMakeRightLinks(farRight);

*retScore = bestChain->score;
for (chain = chainList; chain != NULL; chain = chain->next)
    chain->blockList = NULL;	/* Don't want to free this, it's local. */
chainFreeList(&chainList);
lmCleanup(&lm);
}

static void ssFindBest(struct ffAli *ffList, bioSeq *qSeq, bioSeq *tSeq,
	enum ffStringency stringency, boolean isProt, struct trans3 *t3List,
	struct ffAli **retBestAli, int *retScore, struct ffAli **retLeftovers)
/* String together blocks in alignment into chains. */
{
int count = ffAliCount(ffList);
if (count >= 10)
    {
    ssFindBestBig(ffList, qSeq, tSeq, stringency, isProt, t3List,
    	retBestAli, retScore, retLeftovers);
    }
else
    {
    ssFindBestSmall(ffList, qSeq, tSeq, stringency, isProt, t3List,
    	retBestAli, retScore, retLeftovers);
    }
}

struct ffAli *cutAtBigIntrons(struct ffAli *ffList, int maxIntron, 
	int *pScore, enum ffStringency stringency,
	struct ffAli **returnLeftovers)
/* Return ffList up to the first intron that's too big.
 * Put the rest of the blocks back onto the leftovers list. */
{
struct ffAli *prevFf, *ff, *cutFf = NULL;
prevFf = ffList;
for (ff = prevFf->right; ff != NULL; ff = ff->right)
    {
    int dt = ff->hStart - prevFf->hEnd;
    if (dt > maxIntron)
        {
	cutFf = prevFf;
	break;
	}
    prevFf = ff;
    }
if (cutFf != NULL)
    {
    ff = cutFf->right;
    cutFf->right = NULL;
    ff->left = NULL;
    ffCat(returnLeftovers, &ff);
    *pScore = ffScore(ffList, stringency);
    }
return ffList;
}

void ssStitch(struct ssBundle *bundle, enum ffStringency stringency, 
	int minScore, int maxToReturn)
/* Glue together mrnas in bundle as much as possible. Returns number of
 * alignments after stitching. Updates bundle->ffList with stitched
 * together version. */
{
struct dnaSeq *qSeq =  bundle->qSeq;
struct dnaSeq *genoSeq = bundle->genoSeq;
struct ffAli *ffList = NULL;
struct ssFfItem *ffl;
struct ffAli *bestPath;
int score;
boolean firstTime = TRUE;

if (bundle->ffList == NULL)
    return;

/* The score may improve when we stitch together more alignments,
 * so don't let minScore be too harsh at this stage. */
if (minScore > 20)
    minScore = 20;

/* Create ffAlis for all in bundle and move to one big list. */
for (ffl = bundle->ffList; ffl != NULL; ffl = ffl->next)
    {
    ffCat(&ffList, &ffl->ff);
    }
slFreeList(&bundle->ffList);

ffAliSort(&ffList, ffCmpHitsNeedleFirst);
ffList = ffMergeClose(ffList, qSeq->dna, genoSeq->dna);

while (ffList != NULL)
    {
    ssFindBest(ffList, qSeq, genoSeq, stringency, 
    	bundle->isProt, bundle->t3List,
    	&bestPath, &score, &ffList);

    bestPath = ffMergeNeedleAlis(bestPath, TRUE);
    bestPath = ffRemoveEmptyAlis(bestPath, TRUE);
    if (!bestPath)
	{
	ffFreeAli(&ffList);
	break;
	}
    bestPath = ffMergeHayOverlaps(bestPath);
    bestPath = ffRemoveEmptyAlis(bestPath, TRUE);
    bestPath = forceMonotonic(bestPath, qSeq, genoSeq, stringency,
    	bundle->isProt, bundle->t3List);

    if (firstTime && stringency == ffCdna && bundle->avoidFuzzyFindKludge == FALSE)
	{
	/* Only look for middle exons the first time.  Next times
	 * this might regenerate most of the first alignment... */
	bestPath = smallMiddleExons(bestPath, bundle, stringency);
	}
    bestPath = ffMergeNeedleAlis(bestPath, TRUE);
    if (ffIntronMax != ffIntronMaxDefault)
	{
	bestPath = cutAtBigIntrons(bestPath, ffIntronMax, &score, stringency,
		&ffList);
	}
    if (!bundle->isProt)
	ffSlideIntrons(bestPath);
    bestPath = ffRemoveEmptyAlis(bestPath, TRUE);
    if (score >= minScore)
	{
	AllocVar(ffl);
	ffl->ff = bestPath;
	slAddHead(&bundle->ffList, ffl);
	}
    else
	{
	ffFreeAli(&bestPath);
	ffFreeAli(&ffList);
	break;
	}
    firstTime = FALSE;
    if (--maxToReturn <= 0)
	{
	ffFreeAli(&ffList);
	break;
	}
    }
slReverse(&bundle->ffList);
return;
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
    ssStitch(bun, stringency, 20, 16);
    }
return bundleList;
}

