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
#include "patSpace.h"
#include "supStitch.h"

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

#ifdef DEBUG
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

#endif /* DEBUG */


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
int bestScore = ffScoreMatch(nStart, rhStart, overlap);
int score = bestScore;
int i;
int (*scoreMatch)(char a, char b) = (isProt ? aaScore2 : dnaScore2);


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

static struct ssGraph *ssGraphMake(struct ffAli *ffList, DNA *needle, DNA *haystack, 
	enum ffStringency stringency, boolean isProt)
/* Make a graph corresponding to ffList */
{
int nodeCount = ffAliCount(ffList);
int maxEdgeCount = (nodeCount+1)*(nodeCount)/2;
int edgeCount = 0;
struct ssEdge *edges, *e;
struct ssNode *nodes, *n;
struct ssGraph *graph;
struct ffAli *ff, *mid;
int i, midIx;
int overlap;

if (nodeCount == 1)
    maxEdgeCount = 1;
    
AllocVar(graph);
graph->nodeCount = nodeCount;
graph->nodes = AllocArray(nodes, nodeCount+1);
for (i=1, ff = ffList; i<=nodeCount; ++i, ff = ff->right)
    {
    nodes[i].ff = ff;
    nodes[i].nodeScore = ffScoreMatch(ff->nStart, ff->hStart, ff->hEnd - ff->hStart);
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
	if (ff->nStart < mid->nStart && ff->nEnd < mid->nEnd 
	    && ff->hStart < mid->hStart && ff->hEnd < mid->hEnd)
	    {
	    struct ssNode *ffNode = &nodes[i];
	    int score;
	    int hGap = mid->hStart - ff->hEnd;
	    int nGap = mid->nStart - ff->nEnd;
	    int crossover;
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
		newMidScore = ffScoreMatch(mid->nStart, mid->hStart, midSize-overlap+crossover);
		newFfScore = ffScoreMatch(ff->nStart+crossover, ff->hStart+crossover,
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
    struct ssNode *prevNode;
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

struct ffAli *ffMergeExactly(struct ffAli *aliList, DNA *needle, DNA *haystack)
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

int cmpFflScore(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct ssFfItem *a = *((struct ssFfItem **)va);
const struct ssFfItem *b = *((struct ssFfItem **)vb);
return b->score - a->score;
}

static void trimBundle(struct ssBundle *bundle, int maxFfCount,
	enum ffStringency stringency)
/* Trim out the relatively insignificant alignments from bundle. */
{
struct ssFfItem *ffl, *newFflList = NULL, *lastFfl = NULL;
int ffCountAll = 0;
int ffCountOne;

for (ffl = bundle->ffList; ffl != NULL; ffl = ffl->next)
    {
    ffl->score = ffScore(ffl->ff, stringency);
    }
slSort(&bundle->ffList, cmpFflScore);

for (ffl = bundle->ffList; ffl != NULL; ffl = ffl->next)
    {
    ffCountOne = ffAliCount(ffl->ff);
    if (ffCountOne + ffCountAll <= maxFfCount)
	ffCountAll += ffCountOne;
    else
	{
	if (lastFfl == NULL)
	    errAbort("Can't stitch, single alignment more than %d blocks", maxFfCount);
	lastFfl->next = NULL;
	ssFfItemFreeList(&ffl);
	break;
	}
    lastFfl = ffl;
    }
}

int ssStitch(struct ssBundle *bundle, enum ffStringency stringency)
/* Glue together mrnas in bundle as much as possible. Returns number of
 * alignments after stitching. Updates bundle->ffList with stitched
 * together version. */
{
struct dnaSeq *mrnaSeq =  bundle->qSeq;
struct dnaSeq *genoSeq = bundle->genoSeq;
struct ffAli *ff, *ffList = NULL;
struct ssFfItem *ffl;
struct ffAli *bestPath, *leftovers;
struct ssGraph *graph;
int score;
int newAliCount = 0;
int totalFfCount = 0;
int trimCount = mrnaSeq->size/200 + 1000;

if (bundle->ffList == NULL)
    return;

for (ffl = bundle->ffList; ffl != NULL; ffl = ffl->next)
    totalFfCount += ffAliCount(ffl->ff);

/* In certain repeat situations the number of blocks can explode.  If so
 * try to recover by taking only a fairly large section of the best looking
 * blocks. */
if (trimCount > 8000)	/* This is all the memory we can spare, sorry. */
    trimCount = 8000;
if (totalFfCount > trimCount)
    {
    if (totalFfCount > 6*trimCount)
	warn("In %s vs. %s trimming from %d to %d blocks", mrnaSeq->name, genoSeq->name, totalFfCount, trimCount);
    trimBundle(bundle, trimCount, stringency);
    }

/* Create ffAlis for all in bundle and move to one big list. */
for (ffl = bundle->ffList; ffl != NULL; ffl = ffl->next)
    ffCat(&ffList, &ffl->ff);
slFreeList(&bundle->ffList);

ffAliSort(&ffList, ffCmpHitsNeedleFirst);
ffList = ffMergeExactly(ffList, mrnaSeq->dna, genoSeq->dna);


while (ffList != NULL)
    {
    graph = ssGraphMake(ffList, mrnaSeq->dna, genoSeq->dna, stringency, bundle->isProt);
    ssGraphFindBest(graph, &bestPath, &score, &ffList);
    bestPath = ffMergeNeedleAlis(bestPath, TRUE);
    bestPath = ffRemoveEmptyAlis(bestPath, TRUE);
    bestPath = ffMergeHayOverlaps(bestPath);
    bestPath = ffRemoveEmptyAlis(bestPath, TRUE);
    ffSlideIntrons(bestPath);
    bestPath = ffRemoveEmptyAlis(bestPath, TRUE);
    if (score > 32)
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
    ssGraphFree(&graph);
    }
slReverse(&bundle->ffList);
return newAliCount;
}

static struct ssBundle *findBundle(struct ssBundle *list,  struct dnaSeq *genoSeq)
/* Find bundle in list that has matching genoSeq pointer, or NULL
 * if none such. */
{
struct ssBundle *el;
for (el = list; el != NULL; el = el->next)
    if (el->genoSeq == genoSeq)
	return el;
return NULL;
}

struct ssBundle *ssFindBundles(struct patSpace *ps, struct dnaSeq *cSeq, 
	char *cName, enum ffStringency stringency, boolean avoidSelfSelf)
/* Find patSpace alignments. */
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

