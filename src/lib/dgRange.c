/* dgRange - stuff to tell if a graph which has a range
 * of values associated with each edge is internally consistent.  
 * See comment under bfGraphFromRangeGraph for details. */

#include "common.h"
#include "diGraph.h"


struct bfEdge
/* An edge in a (lightweight) Belman Ford graph. */
   {
   struct bfEdge *next;    /* Pointer to next element. */
   int distance;           /* Distance - may be negative. */
   struct bfVertex *in;    /* Connecting node. */
   struct bfVertex *out;   /* Connecting node. */
   };

struct bfVertex
/* A node in a (lightweight) Belman Ford graph. */
   {
   int position;               /* Currently assigned position. */
   struct bfEdge *waysOut;     /* Ways out. */
   };

struct bfGraph
/* A lightweight, but fixed sized graph for Belman Ford algorithm. */
    {
    int vertexCount;	         /* Number of vertices */
    struct bfVertex *vertices;   /* Array of vertices. */
    int edgeCount;               /* Number of edges. */
    struct bfEdge *edges;        /* Array of edges. */
    };

struct bfRange 
/* Min and max distances allowed for an edge. */
   {
   int minDist, maxDist;
   };

static void bfGraphFree(struct bfGraph **pGraph)
/* Free up Belman-Ford graph. */
{
struct bfGraph *graph = *pGraph;
if (graph != NULL)
    {
    freeMem(graph->vertices);
    freeMem(graph->edges);
    freez(pGraph);
    }
}

static struct bfGraph *bfGraphFromRangeGraph(struct diGraph *rangeGraph, 
   boolean (*findEdgeRange)(struct dgEdge *edge, int *retMin, int *retMax),
   struct dgNode *a, struct dgNode *b, int abMin, int abMax)
/* Construct a directed graph with two edges for each
 * edge of the range graph.  Where the range graph
 * has the following info:
 *     sourceNode destNode minDistance maxDistance
 *    -------------------------------------------
 *         A         B         2          5
 * The bfGraph would have:
 *     sourceNode destNode   edgeVal  meaning
 *    --------------------------------------------
 *         A         B         5     D(A) - D(B) <= 5 
 *         B         A        -2     D(B) - D(A) <= -2
 * Furthermore the bfGraph introduces a new node,
 * "zero", which has a zero-valued connection to
 * all other nodes.
 *
 * This graph can then be processed via the Bellman-
 * Ford algorithm (see p. 532 of Cormen,Leiserson and
 * Rivest's Introduction to Algorithms, MIT Press 1990)
 * to see if the bfGraph, and by extension the range
 * graph, is consistent.
 */
{
struct bfGraph *bfGraph;
struct bfVertex *vertices, *freeVertex, *newSource, *newDest, *zeroVertex;
struct bfEdge *edges, *newEdge, *freeEdge;
struct dgNode *oldSource, *oldDest, *oldNode;
int oldEdgeCount = dlCount(rangeGraph->edgeList);
int oldVertexCount = 0;

/* Coopt topoOrder field to store unique ID for each
 * node, starting at 1.  (We'll reserve 0 for the
 * node we insert - the Belman Ford "zero" node.)*/
for (oldNode = rangeGraph->nodeList; oldNode != NULL; oldNode = oldNode->next)
    oldNode->topoOrder = ++oldVertexCount;

/* Allocate space for new graph. */
AllocVar(bfGraph);
bfGraph->vertexCount = oldVertexCount + 1;
bfGraph->vertices = freeVertex = AllocArray(vertices, bfGraph->vertexCount);
bfGraph->edgeCount = 2*oldEdgeCount + oldVertexCount;
if (a != NULL)
  bfGraph->edgeCount += 2;
bfGraph->edges = freeEdge = AllocArray(edges, bfGraph->edgeCount);

/* Get zero vertex. */
zeroVertex = freeVertex++;

/* Scan through old graph and add vertices to new graph. */
for (oldSource = rangeGraph->nodeList; oldSource != NULL; 
    oldSource = oldSource->next)
    {
    struct dgConnection *dgConn;
    int rangeMin, rangeMax;

    /* Allocate new source vertex. */
    newSource = freeVertex++;

    /* Make connection from zero vertex to this one. */
    newEdge = freeEdge++;
    newEdge->distance = 0;
    newEdge->in = zeroVertex;
    newEdge->out = newSource;
    slAddHead(&zeroVertex->waysOut, newEdge);

    /* Loop through all ways out of source. */
    for (dgConn = oldSource->nextList; dgConn != NULL; dgConn = dgConn->next)
        {
	/* Find destination vertex and size range of edge. */
	if ((*findEdgeRange)(dgConn->edgeOnList->val, &rangeMin, &rangeMax))
	    {
	    oldDest = dgConn->node;
	    newDest = &vertices[oldDest->topoOrder];

	    /* Make two new edges corresponding to old edge. */
	    newEdge = freeEdge++;
	    newEdge->distance = rangeMax;
	    newEdge->in = newSource;
	    newEdge->out = newDest;
	    slAddHead(&newSource->waysOut, newEdge);
	    newEdge = freeEdge++;
	    newEdge->distance = -rangeMin;
	    newEdge->in = newDest;
	    newEdge->out = newSource;
	    slAddHead(&newDest->waysOut, newEdge);
	    }
	}
    }
/* Add additional pair of edges for extra range if applicable. */
if (a != NULL)
    {
    newSource = &vertices[a->topoOrder];
    newDest = &vertices[b->topoOrder];
    newEdge = freeEdge++;
    newEdge->distance = abMax;
    newEdge->in = newSource;
    newEdge->out = newDest;
    slAddHead(&newSource->waysOut, newEdge);
    newEdge = freeEdge++;
    newEdge->distance = -abMin;
    newEdge->in = newDest;
    newEdge->out = newSource;
    slAddHead(&newDest->waysOut, newEdge);
    }
bfGraph->edgeCount = freeEdge - bfGraph->edges;
return bfGraph;
}

static boolean bfCheckGraph(struct bfGraph *graph)
/* Run Belman-Ford algorithm to check graph for consistency. */
{
struct bfVertex *vertices = graph->vertices, *in, *out;
int vertexCount = graph->vertexCount;
int edgeCount = graph->edgeCount;
struct bfEdge *edges = graph->edges, *edge;
int i, edgeIx;
int maxIterations = vertexCount - 1;
boolean anyChange = FALSE;
int newPos;

for (i=0; i<vertexCount; ++i)
    vertices[i].position = 0x3fffffff;    /* Mighty big number. */
for (i=0; i<maxIterations; ++i)
    {
    anyChange = FALSE;
    for (edgeIx = 0, edge = edges; edgeIx < edgeCount; ++edgeIx, ++edge)
        {
	in = edge->in;
	out = edge->out;
	newPos = in->position + edge->distance;
	if (newPos < out->position)
	    {
	    out->position = newPos;
	    anyChange = TRUE;
	    }
	}
    if (!anyChange)
        break;
    }
return !anyChange;
}

boolean dgAddedRangeConsistent(struct diGraph *rangeGraph,
   struct dgNode *a, struct dgNode *b, int abMin, int abMax,
   boolean (*findEdgeRange)(struct dgEdge *edge, int *retMin, int *retMax) )
/* Return TRUE if graph with a range of allowable distances associated
 * with each edge would be internally consistent if add edge from a to b
 * with given min/max values. */
{
struct bfGraph *bfGraph;
boolean ok; 

bfGraph = bfGraphFromRangeGraph(rangeGraph, findEdgeRange, a, b, abMin, abMax);
ok = bfCheckGraph(bfGraph);
bfGraphFree(&bfGraph);
return ok;
}

boolean dgRangesConsistent(struct diGraph *rangeGraph,
   boolean (*findEdgeRange)(struct dgEdge *edge, int *retMin, int *retMax) )
/* Return TRUE if graph with a range of allowable distances associated
 * with each edge is internally consistent. */
{
return dgAddedRangeConsistent(rangeGraph, NULL, NULL, 0, 0, findEdgeRange);
}

