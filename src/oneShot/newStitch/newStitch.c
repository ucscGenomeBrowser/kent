/* newStitch - Experiments with new ways of stitching together alignments. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "dlist.h"
#include "options.h"
#include "localmem.h"
#include "dlist.h"
#include "fuzzyFind.h"
#include "axt.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "newStitch - Experiments with new ways of stitching together alignments\n"
  "usage:\n"
  "   newStitch input.axt output.axt\n"
  "options:\n"
  "   -xxx=XXX\n"
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
    struct asEdge *waysIn;	/* Edges leading into this node. */
    struct asBlock *block;	/* Input node. */
    struct asEdge *bestWayIn;   /* Dynamic programming best edge in. */
    int cumScore;               /* Dynamic programming score of edge. */
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
    };

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


int defaultGapPenalty(int qSize, int tSize)
/* A logarithmic gap penalty scaled to axt defaults. */
{
int total = qSize + tSize;
if (total <= 0)
    return 0;
return 400 * pow(total, 1.0/3.3);
}



void addFollowingNodes(struct asGraph *graph, int nodeIx,
	int (*gapPenalty)(int qSize, int tSize))
/* Connect current node to all nodes that could _immediately_
 * follow it. */
{
struct dlList scratchList;
struct asNode *prev = &graph->nodes[nodeIx];
struct asBlock *prevBlock = prev->block;
struct asNode  *asNode;
struct asBlock *block;
struct dlNode *dlHead, *dlNode, *dlNext;
int qPrev = prevBlock->qStart, tPrev = prevBlock->tStart;
int i;

/* Build up list of nodes that might possibly follow this one. */
dlListInit(&scratchList);
for (i=nodeIx+1; i<graph->nodeCount+1; ++i)
    {
    dlNode = graph->builderNodes+i;
    dlAddTail(&scratchList, dlNode);
    }

/* Pop head of list of possible followers and check if it
 * actually could follow.   If so create an edge into it,
 * and remove nodes that could follow it from list since
 * they could no longer be _immediate_ followers. */
while ((dlHead = dlPopHead(&scratchList)) != NULL)
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
	for (dlNode = scratchList.head; !dlEnd(dlNode); dlNode = dlNext)
	    {
	    dlNext = dlNode->next;
	    asNode = dlNode->val;
	    block = asNode->block;
	    if (block->qStart >= qCur && block->tStart >= tCur)
	        dlRemove(dlNode);
	    }
	}
    }
}

static struct asGraph *asGraphMake(struct asBlock *blockList,
	int (*gapPenalty)(int qSize, int tSize))
/* Make a graph corresponding to blockList */
{
int nodeCount = slCount(blockList);
int maxEdgeCount = (nodeCount+1)*(nodeCount)/2;
int edgeCount = 0;
struct asEdge *e;
struct asNode *nodes, *n, *z;
struct dlNode *builderNodes;
struct asGraph *graph;
struct asBlock *block;
int i;
int overlap;

if (nodeCount == 1)
    maxEdgeCount = 1;
    
/* Build up nodes from 1 to nodeCount based on data. */
AllocVar(graph);
graph->nodeCount = nodeCount;
graph->nodes = AllocArray(nodes, nodeCount+1);
graph->builderNodes = AllocArray(builderNodes, nodeCount+1);
graph->lm = lmInit(1024*sizeof(struct asEdge));
for (i=1, block = blockList; i<=nodeCount; ++i, block = block->next)
    {
    n = &nodes[i];
    n->block = block;
    builderNodes[i].val = n;
    }

/* Fake a node 0 at 0,0 and connect all nodes to it... */
z = &nodes[0];
lmAllocVar(graph->lm, z->block);
for (i=1; i<=nodeCount; ++i)
    {
    n = &nodes[i];
    lmAllocVar(graph->lm, e);
    e->nodeIn = z;
    e->score = n->block->score;
    slAddHead(&n->waysIn, e);
    graph->edgeCount += 1;
    }

for (i=1; i<=nodeCount; ++i)
    addFollowingNodes(graph, i, gapPenalty);

freez(&graph->builderNodes);
return graph;
}

void asGraphFree(struct asGraph **pGraph)
/* Free up a graph. */
{
struct asGraph *graph = *pGraph;
if (graph != NULL)
    {
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
    struct asBlock *blockList; /* List of alignment blocks. */
    };

static struct asNode *asDynamicProgram(struct asGraph *graph)
/* Do dynamic programming to find optimal path though
 * graph.  Return best ending node (with back-trace
 * pointers and score set). */
{
int veryBestScore = -0x3fffffff;
struct asNode *veryBestNode = NULL;
int nodeIx;

for (nodeIx = 1; nodeIx <= graph->nodeCount; ++nodeIx)
    {
    int bestScore = -0x3fffffff;
    int score;
    struct asEdge *bestEdge = NULL;
    struct asNode *curNode = &graph->nodes[nodeIx];
    struct asNode *prevNode;
    struct asEdge *edge;

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


void newStitch(char *axtFile, char *output)
/* newStitch - Experiments with new ways of stitching together alignments. */
{
struct hash *pairHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct dyString *dy = newDyString(512);
struct lineFile *lf = lineFileOpen(axtFile, TRUE);
struct axt *axt;
struct seqPair *spList = NULL, *sp;
struct asBlock *block;
struct asGraph *graph = NULL;
FILE *f = mustOpen(output, "w");

/* Read input file and divide alignments into various parts. */
while ((axt = axtRead(lf)) != NULL)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", axt->qName, axt->qStrand, axt->tName);
    sp = hashFindVal(pairHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pairHash, dy->string, sp, &sp->name);
	}
    AllocVar(block);
    block->qStart = axt->qStart;
    block->qEnd = axt->qEnd;
    block->tStart = axt->tStart;
    block->tEnd = axt->tEnd;
    block->score = axt->score;
    block->ali = axt;
    slAddHead(&sp->blockList, block);
    }
for (sp = spList; sp != NULL; sp = sp->next)
    {
    time_t startTime, dt;
    int fullSize; 
    struct asNode *n, *nList = NULL;
    slReverse(&sp->blockList);
    uglyf("%s\t%d blocks\n", sp->name, slCount(sp->blockList));
    startTime = time(NULL);
    graph = asGraphMake(sp->blockList, defaultGapPenalty);
    dt = time(NULL) - startTime;
    fullSize = (graph->nodeCount + 1)*(graph->nodeCount)/2;
    uglyf("%d edges (%d in full graph) %4.1f%% of full in %ld seconds\n",
	graph->edgeCount, fullSize, 100.0*graph->edgeCount/fullSize, dt);
    fprintf(f, 
    	"%s %d edges (%d in full graph) %4.1f%% of full in %ld seconds\n",
	sp->name, graph->edgeCount, fullSize, 
	100.0*graph->edgeCount/fullSize, dt);
    asGraphDump(graph, f);
    fprintf(f, "\n");
    fprintf(f, "Best subset:");
    nList = asDynamicProgram(graph);
    for (n = nList; n != NULL; )
        {
	struct asEdge *e = n->bestWayIn;
	fprintf(f, " %d", n - graph->nodes);
	if (e != NULL)
	    n = e->nodeIn;
	else
	    break;
	}
    fprintf(f, "\n\n");
    fprintf(f, "Ordered\n");
    for (n = nList; n != NULL; )
        {
	struct asEdge *e = n->bestWayIn;
	int nodeIx = n - graph->nodes;
	if (nodeIx != 0)
	    {
	    struct asBlock *block = n->block;
	    fprintf(f, "  %d@( %d %d , %d %d) $core %d ( %d)\n", nodeIx, block->qStart, block->qEnd, block->tStart, block->tEnd, block->score, e->score);
	    }
	if (e != NULL)
	    n = e->nodeIn;
	else
	    break;
	}
    fprintf(f, "\n");

    asGraphFree(&graph);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
newStitch(argv[1], argv[2]);
return 0;
}
