/* newStitch - Experiments with new ways of stitching together alignments. 
 * Main thing is constructing a sparse rather than a full graph of
 * nodes that could potentially follow other nodes.   We make it
 * sparse by only including edges for nodes that could *directly*
 * follow another node. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "dlist.h"
#include "portable.h"
#include "options.h"
#include "localmem.h"
#include "dlist.h"
#include "fuzzyFind.h"
#include "axt.h"

int gridDim = 32; /* Number of divisions in each dimension of grid. */
int dimSquared;	  /* Square of gridDim. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "newStitch2 - Experiments with new ways of stitching together alignments\n"
  "usage:\n"
  "   newStitch2 input.axt output.axt\n"
  "options:\n"
  "   -gridDim=N Dimensions of grid\n"
  );
}

struct asBlock
/* A block in an alignment.  Used for input as
 * well as output for asStitch. */
    {
    struct asBlock *next;	/* Next in list. */
    int qStart, qEnd;		/* Range covered in query. */
    int tStart, tEnd;		/* Range covered in target. */
    int score;			/* Alignment score for this block. */
    void *ali;			/* Alignment in some format. */
    };

struct asChain
/* A chain of blocks.  Used for output of asStitch. */
    {
    struct asChain *next;	/* Next in list. */
    struct asBlock *blockList;	/* List of blocks. */
    };

struct asNode
/* Node of aliStitch graph. */
    {
    struct asNode *next;	/* Next in grid list. */
    struct asEdge *waysIn;	/* Edges leading into this node. */
    struct asBlock *block;	/* Input node. */
    struct asEdge *bestWayIn;   /* Dynamic programming best edge in. */
    int cumScore;               /* Dynamic programming score of edge. */
    struct dlNode *unchained;   /* Pointer to node in unchained list. */
                                /* Also used for scratch. */
    };

struct asGridEl
/* Nodes in a small rectangular part of overall space. */
    {
    struct asNode *nodeList;	/* List of nodes. */
    boolean live;		/* If true then we need to examine nodes here
                                 * as possible direct followers. */
    };

struct asGrid
/* Grid to help optimize graph construction. */
    {
    struct asGridEl *grid;	/* 2-D array. */
    int dim;			/* Dimensions of grid . */
    int area;			/* Area of grid (dim * dim) */
    int qStart, qEnd;		/* Min/max coordinates of q coordinates. */
    int tStart, tEnd;		/* Min/max coordinates of t coordinates. */
    int qSize, tSize;		/* Size of region covered. */
    int qDiv, tDiv;		/* What to divide coordinates by to get in grid */
    };

struct asEdge
/* Edge of aliStitch graph. */
    {
    struct asEdge *next;         /* Next edge in waysIn list. */
    struct asNode *nodeIn;       /* The node that leads to this edge. */
    int score;                   /* Score you get taking this edge. */
    };

struct asGraph
/* Ali stitcher graph for dynamic programming to find best
 * way through. */
    {
    int edgeCount;              /* Number of edges. */
    int nodeCount;		/* Number of nodes. */
    struct asNode *nodes;       /* Array of nodes. */
    struct dlNode *builderNodes;/* Scratch node used in building graph. */
    struct lm *lm;		/* Local memory. */
    struct dlList unchained; /* List of nodes not yet chained. */
    };

int asBlockCmp(const void *va, const void *vb)
/* Compare to sort based on target start. */
{
const struct asBlock *a = *((struct asBlock **)va);
const struct asBlock *b = *((struct asBlock **)vb);
return a->tStart - b->tStart;
}

int asNodeCmp(const void *va, const void *vb)
/* Compare to sort based on target start. */
{
const struct asNode *a = *((struct asNode **)va);
const struct asNode *b = *((struct asNode **)vb);
return a->block->tStart - b->block->tStart;
}


void asGraphDump(struct asGraph *graph, FILE *f)
/* Dump out graph. */
{
int i;
fprintf(f, "Graph dump: %d edges, %d nodes\n", 
	graph->edgeCount, graph->nodeCount);
for (i=1; i<=graph->nodeCount; ++i)
    {
    struct asNode *node = graph->nodes + i;
    struct asBlock *block = node->block;
    struct asEdge *e;
    fprintf(f, "  %d@(%d,%d)$%d:", i, block->qStart, block->tStart, 
    	block->score);
    for (e = node->waysIn; e != NULL; e = e->next)
	{
	fprintf(f, " %d($%d)", e->nodeIn - graph->nodes, e->score);
	}
    fprintf(f, "\n");
    }
}

struct asGrid *asGridNew(int dim, int qStart, int qEnd, int tStart, int tEnd)
/* Create new asGrid structure. */
{
/* Allocate grid and fill in from parameters. */
struct asGrid *grid;
AllocVar(grid);
grid->dim = dim;
grid->qStart = qStart;
grid->qEnd = qEnd;
grid->tStart = tStart;
grid->tEnd = tEnd;

/* Calculate and check sizes. */
grid->qSize = qEnd - qStart;
assert(grid->qSize > 0);
grid->tSize = tEnd - tStart;
assert(grid->tSize > 0);

/* Allocate two dimensional array of grid elements. */
grid->area = dim * dim;
AllocArray(grid->grid, grid->area);

/* Figure out what to divide coordinates by 
 * to get them to fit into grid,  rounding up. */
grid->qDiv = (grid->qSize + dim - 1)/dim;
grid->tDiv = (grid->tSize + dim - 1)/dim;

// uglyf("grid: qStart %d, qEnd %d, qSize %d, tStart %d, tEnd %d, tSize %d, qDiv %d, tDiv %d\n", grid->qStart, grid->qEnd, grid->qSize, grid->tStart, grid->tEnd, grid->tSize, grid->qDiv, grid->tDiv);
return grid;
}

void asGridFree(struct asGrid **pGrid)
/* Free up grid. */
{
struct asGrid *grid = *pGrid;
if (grid != NULL)
   {
   freeMem(grid->grid);
   freez(pGrid);
   }
}

struct asGridEl *gridCoordEl(struct asGrid *grid, int q, int t)
/* Return grid element associated with coordinate. */
{
int qIx = (q - grid->qStart) / grid->qDiv;
int tIx = (t - grid->tStart) / grid->tDiv;
int ix = qIx * grid->dim + tIx;
assert(ix < grid->area);
return &grid->grid[ix];
}

struct asGridEl *gridIxEl(struct asGrid *grid, int qIx, int tIx)
/* Return grid element at given indexes. */
{
int ix = qIx * grid->dim + tIx;
assert(ix < grid->area);
return &grid->grid[ix];
}

void gridAllLive(struct asGrid *grid)
/* Convert all squares in grid to live status */
{
int i;
struct asGridEl *g = grid->grid;
for (i=0; i<grid->area; ++i)
    g[i].live = TRUE;
}

void gridShortOut(struct asGrid *grid, int qIx, int tIx)
/* Mark grid squares past this one as no longer live. */
{
int q, t;
for (q = qIx+1; q < grid->dim; ++q)
    {
    struct asGridEl *g = grid->grid + q*grid->dim;
    for (t = tIx+1; t<grid->dim; ++t)
	g[t].live = FALSE;
    }
}

int defaultGapPenalty(int qSize, int tSize)
/* A logarithmic gap penalty scaled to axt defaults. */
{
int total = qSize + tSize;
if (total <= 0)
    return 0;
return 400 * pow(total, 1.0/2.5);
}


void considerNodes(struct asNode *prev, struct asGrid *grid, 
	struct dlList *candidateList, int qIxPrev, int tIxPrev,
	int qIx, int tIx)
/* Consider nodes in one cell of grid. If find any candidates
 * then screen off parts of grid that are no longer relevant. */
{
if (qIx >= qIxPrev && tIx >= tIxPrev)
    {
    struct asGridEl *g = gridIxEl(grid, qIx, tIx);
    if (g->live)
	{
	struct asBlock *prevBlock = prev->block;
	int qPrev = prevBlock->qStart;
	int tPrev = prevBlock->tStart;
	struct asNode  *node;
	boolean addedAny = FALSE;
	for (node = g->nodeList; node != NULL; node = node->next)
	    {
	    if (node != prev)
		{
		struct asBlock *block = node->block;
		int q = block->qStart;
		int t = block->tStart;
		if (q > qPrev && t > tPrev)
		    {
		    dlAddTail(candidateList, node->unchained);
		    addedAny = TRUE;
		    }
		}
	    }
	if (addedAny)
	    gridShortOut(grid, qIx, tIx);
	}
    }
}


void addFollowingNodes(struct asGraph *graph, 
	struct asGrid *grid, int nodeIx,
	int (*gapPenalty)(int qSize, int tSize))
/* Connect current node to all nodes that could _immediately_
 * follow it. */
{
struct dlList candidateList;
struct asNode *prev = &graph->nodes[nodeIx];
struct asBlock *prevBlock = prev->block;
struct asNode  *asNode;
struct asBlock *block;
struct dlNode *dlHead, *dlNode, *dlNext;
int qPrev = prevBlock->qStart, tPrev = prevBlock->tStart;
int qIxPrev = (qPrev - grid->qStart)/ grid->qDiv;
int tIxPrev = (tPrev - grid->tStart)/ grid->tDiv;
int r, i;

/* Set up grid to help build a relatively short candidate
 * list of following nodes quickly. */
gridAllLive(grid);
dlListInit(&candidateList);

/* Consider possible following nodes one square of
 * grid at a time.  Explore grid in the order:
 *      1  3  7 13
 *      2  4  8 14
 *      5  6  9 15
 *     10 11 12 16
 * so that hopefully can eliminate as not _immediately_
 * following as much as possible as soon as possible. */
for (r=0; r<grid->dim; ++r)
    {
    for (i=0; i<r; ++i)
        {
	considerNodes(prev, grid, &candidateList, qIxPrev, tIxPrev, r, i);
	considerNodes(prev, grid, &candidateList, qIxPrev, tIxPrev, i, r);
	}
    considerNodes(prev, grid, &candidateList, qIxPrev, tIxPrev, r, r);
    }

// uglyf("following %d,%d %d (of %d usual) candidates\n", prevBlock->qStart, prevBlock->tStart, dlCount(&candidateList), graph->nodeCount - nodeIx - 1);

/* Pop head of list of possible followers and check if it
 * actually could follow.   If so create an edge into it,
 * and remove nodes that could follow it from list since
 * they could no longer be _immediate_ followers. */
while ((dlHead = dlPopHead(&candidateList)) != NULL)
    {
    struct asNode *cur = dlHead->val;
    struct asBlock *curBlock = cur->block;
    int qCur = curBlock->qStart;
    int tCur = curBlock->tStart;
    if (qCur > qPrev && tCur > tPrev)
	{
	int gap;
	int eScore;
	struct asEdge *e;

	/* Calculate score get by taking this edge. */
	int dq = qCur - prevBlock->qEnd;
	int dt = tCur - prevBlock->tEnd;
	if (dq < 0) dq = 0;
	if (dt < 0) dt = 0;
	gap = (*gapPenalty)(dq, dt);
	eScore = curBlock->score - gap;

	/* Add edge between node passed in and this one. */
	lmAllocVar(graph->lm, e);
	e->nodeIn = prev;
	slAddHead(&cur->waysIn, e);
	graph->edgeCount += 1;
	if (dq < 0) dq = 0;
	if (dt < 0) dt = 0;
	e->score = eScore;

	/* Remove from consideration nodes that could come
	 * after this one. */
	for (dlNode = candidateList.head; !dlEnd(dlNode); dlNode = dlNext)
	    {
	    dlNext = dlNode->next;
	    asNode = dlNode->val;
	    block = asNode->block;
	    if (block->qStart > qCur && block->tStart > tCur)
	        dlRemove(dlNode);
	    }
	}
    }
}

void gridDump(struct asGrid *grid, FILE *f)
/* Dump out grid. */
{
int q,t;
fprintf(f, "gridDump: %dx%d, qSize %d, tSize %d, qStart %d, tStart %d, qDiv %d, tDiv %d\n",
	grid->dim, grid->dim, grid->qSize, grid->tSize, 
	grid->qStart, grid->tStart, grid->qDiv, grid->tDiv);
for (q=0; q<grid->dim; ++q)
    for (t=0; t<grid->dim; ++t)
        {
	int ix = q * grid->dim + t;
	struct asGridEl *g = &grid->grid[ix];
	struct asNode *node;
	struct asBlock *block;
	fprintf(f, " q %d, t %d\n", q, t);
	for (node = g->nodeList; node != NULL; node = node->next)
	    {
	    block = node->block;
	    fprintf(f, "    %d,%d (%p)\n", block->qStart, block->tStart, node->unchained);
	    }
	}
fprintf(f, "\n");
	
}

static struct asGraph *asGraphMake(struct asBlock *blockList,
	int (*gapPenalty)(int qSize, int tSize))
/* Make a graph corresponding to blockList */
{
int nodeCount = slCount(blockList);
int edgeCount = 0;
struct asEdge *e;
struct asNode *nodes, *n, *z;
struct dlNode *builderNodes;
struct asGraph *graph;
struct asBlock *block;
int i,ix;
int overlap;
int qStart = BIGNUM, qEnd = -BIGNUM;
int tStart = BIGNUM, tEnd = -BIGNUM;
struct asGrid *grid;

/* Figure out dimensions of bounding box of all blocks and
 * allocate a grid based on this to speed up graph building. */
for (block = blockList; block != NULL; block = block->next)
    {
    if (qStart > block->qStart) qStart = block->qStart;
    if (qEnd < block->qEnd) qEnd = block->qEnd;
    if (tStart > block->tStart) tStart = block->tStart;
    if (tEnd < block->tEnd) tEnd = block->tEnd;
    }
grid = asGridNew(gridDim, qStart, qEnd, tStart, tEnd);

/* Build up nodes from 1 to nodeCount based on data. */
AllocVar(graph);
graph->nodeCount = nodeCount;
graph->nodes = AllocArray(nodes, nodeCount+1);
graph->builderNodes = AllocArray(builderNodes, nodeCount+1);
graph->lm = lmInit(1024*sizeof(struct asEdge));
for (i=1, block = blockList; i<=nodeCount; ++i, block = block->next)
    {
    struct asGridEl *g = gridCoordEl(grid, block->qStart, block->tStart);
    n = &nodes[i];
    n->block = block;
    builderNodes[i].val = n;
    slAddHead(&g->nodeList, n);
    }

/* Keep grid elements internally sorted. */
for (i=0; i<grid->area; ++i)
    slReverse(&grid->grid[i].nodeList);

/* Fake a node 0 at 0,0 and connect all nodes to it... */
z = &nodes[0];
lmAllocVar(graph->lm, z->block);
for (i=1; i<=nodeCount; ++i)
    {
    n = &nodes[i];
    n->unchained = &builderNodes[i];
    lmAllocVar(graph->lm, e);
    e->nodeIn = z;
    e->score = n->block->score;
    slAddHead(&n->waysIn, e);
    graph->edgeCount += 1;
    }


/* Add other edges to graph. */
for (i=1; i<=nodeCount; ++i)
    addFollowingNodes(graph, grid, i, gapPenalty);

/* Make list of unchained nodes. */
dlListInit(&graph->unchained);
for (i=1; i<=nodeCount; ++i)
    {
    dlAddTail(&graph->unchained, &builderNodes[i]);
    }
nodes[0].unchained = &builderNodes[0]; /* This helps end conditions. */

/* Clean up and return. */
asGridFree(&grid);
return graph;
}

void asGraphFree(struct asGraph **pGraph)
/* Free up a graph. */
{
struct asGraph *graph = *pGraph;
if (graph != NULL)
    {
    freeMem(graph->builderNodes);
    freeMem(graph->nodes);
    lmCleanup(&graph->lm);
    freez(pGraph);
    }
}

struct seqPair
/* Pair of sequences. */
    {
    struct seqPair *next;
    char *name;	/* Allocated in hash */
    struct axt *axtList; /* List of alignments. */
    };

static struct asNode *asDynamicProgram(struct asGraph *graph)
/* Do dynamic programming to find optimal path though
 * graph.  Return best ending node (with back-trace
 * pointers and score set). */
{
int veryBestScore = -0x3fffffff;
struct asNode *veryBestNode = NULL;
struct dlNode *el;

for (el = graph->unchained.head; !dlEnd(el); el = el->next)
    {
    int bestScore = -0x3fffffff;
    int score;
    struct asEdge *bestEdge = NULL;
    struct asNode *curNode = el->val;
    struct asNode *prevNode;
    struct asEdge *edge;

    for (edge = curNode->waysIn; edge != NULL; edge = edge->next)
	{
	prevNode = edge->nodeIn;
	if (prevNode->unchained != NULL)
	    {
	    score = edge->score + prevNode->cumScore;
	    if (score > bestScore)
		{
		bestScore = score;
		bestEdge = edge;
		}
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


void chainPair(struct seqPair *sp, FILE *f)
/* Make chains for all alignments in sp. */
{
struct asGraph *graph = NULL;
long startTime, dt;
int fullSize; 
struct asNode *n, *nList = NULL;
struct axt *axt, *first, *last;
struct dlList *axtList = newDlList();
struct dlNode *axtNode;
struct asBlock *blockList = NULL, *block;

/* Make initial list containing all axt's. */
uglyf("ChainPair %s - %d axt's initially\n", sp->name, slCount(sp->axtList));
for (axt = sp->axtList; axt != NULL; axt = axt->next)
    {
    axtNode = dlAddValTail(axtList, axt);
    }

/* Make block list */
for (axtNode = axtList->head; !dlEnd(axtNode); axtNode = axtNode->next)
    {
    axt = axtNode->val;
    AllocVar(block);
    block->qStart = axt->qStart;
    block->qEnd = axt->qEnd;
    block->tStart = axt->tStart;
    block->tEnd = axt->tEnd;
    block->score = axt->score;
    block->ali = axtNode;
    slAddHead(&blockList, block);
    }
slSort(&blockList, asBlockCmp);

/* Make up graph. */
/* Make up graph and time and dump it for debugging. */
startTime = clock1000();
graph = asGraphMake(blockList, defaultGapPenalty);
dt = clock1000() - startTime;
fullSize = (graph->nodeCount + 1)*(graph->nodeCount)/2;
uglyf("%d edges (%d in full graph) %4.1f%% of full in %5.3f seconds\n",
    graph->edgeCount, fullSize, 100.0*graph->edgeCount/fullSize, 0.001*dt);
fprintf(f, 
    "%s %d edges (%d in full graph) %4.1f%% of full in %ld seconds\n",
    sp->name, graph->edgeCount, fullSize, 
    100.0*graph->edgeCount/fullSize, dt);
// asGraphDump(graph, f);
// fprintf(f, "\n");

/* Call chainer as long as there are axt's left unchained. */
while (!dlEmpty(&graph->unchained))
    {
    struct dlList *axtChain = newDlList();

    /* Run dynamic program on graph and move chain to axtChain. */
    nList = asDynamicProgram(graph);
    for (n = nList; n != graph->nodes; n = n->bestWayIn->nodeIn)
	{
	axtNode = n->block->ali;
	dlRemove(axtNode);
	dlAddHead(axtChain, axtNode);
	if (n != graph->nodes)
	    {
	    dlRemove(n->unchained);	
	    n->unchained = NULL;
	    }
	}

    /* Print out axtChain. */
    first = axtChain->head->val;
    last = axtChain->tail->val;
    fprintf(f, "%s Chain %d, score %d, %d %d, %d %d:\n", 
    	sp->name, dlCount(axtChain), nList->cumScore,
	first->qStart, last->qEnd, first->tStart, last->tEnd);
    for (axtNode = axtChain->head; !dlEnd(axtNode); axtNode = axtNode->next)
        {
	axt = axtNode->val;
	fprintf(f, " %s%c%s %d %d, %d %d, %d\n",
		axt->qName, axt->qStrand, axt->tName, 
		axt->qStart, axt->qEnd, axt->tStart, axt->tEnd,
		axt->score);
	}
    fprintf(f, "\n");
    freeDlList(&axtChain);
    }
asGraphFree(&graph);
fprintf(f, "\n");
slFreeList(&blockList);
freeDlList(&axtList);
}

void newStitch(char *axtFile, char *output)
/* newStitch - Experiments with new ways of stitching together alignments. */
{
struct hash *pairHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct dyString *dy = newDyString(512);
struct lineFile *lf = lineFileOpen(axtFile, TRUE);
struct axt *axt;
struct seqPair *spList = NULL, *sp;
FILE *f = mustOpen(output, "w");

/* Read input file and divide alignments into various parts. */
while ((axt = axtRead(lf)) != NULL)
    {
    if (axt->score < 500)
        {
	axtFree(&axt);
	continue;
	}
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", axt->qName, axt->qStrand, axt->tName);
    sp = hashFindVal(pairHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pairHash, dy->string, sp, &sp->name);
	}
    slAddHead(&sp->axtList, axt);
    }
for (sp = spList; sp != NULL; sp = sp->next)
    {
    slReverse(&sp->axtList);
    chainPair(sp, f);
    }
dyStringFree(&dy);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
gridDim = optionInt("gridDim", gridDim);
dimSquared = gridDim * gridDim;
newStitch(argv[1], argv[2]);
return 0;
}
