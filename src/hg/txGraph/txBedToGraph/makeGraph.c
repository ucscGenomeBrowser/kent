/* makeGraph - this module creates a txGraph from a list of 
 * linkedBeds.  The main steps to this process are:
 *   1) Create list of all unique start and end points in
 *      beds.  These are 'hard' if they are at a real splice
 *      site and 'soft' otherwise.
 *   2) Create list of all unique edges. Edges include evidence
 *      if any.
 *   3) Snap soft ends to nearby hard ends of compatible type.
 *   4) For half-soft edges, examine all other edges that share
 *      the hard end, and and that are hard on both ends.  Merge
 *      the half-soft edge with the most similar such double-hard
 *      edge, favoring extensions of edge over truncations. 
 *   5) For edges that are half soft that can't yet be snapped, 
 *      merge all soft ends to a single point that is a consensus favoring 
 *      larger transcripts.
 *    6) Merge together all overlapping edges that are soft on both sides.  Make
 *     the edge boundaries equal to the median value of all the merged boundaries. */

#include "common.h"
#include "hash.h"
#include "rbTree.h"
#include "dlist.h"
#include "diGraph.h"
#include "bed.h"
#include "ggTypes.h"
#include "txGraph.h"
#include "txBedToGraph.h"

struct evidence
/* One piece of evidence. */
    {
    struct evidence *next;
    struct linkedBeds *lb;	/* Associated input data. */
    int start,end;		/* Bounds of evidence. */
    };

struct vertex
/* A point in our graph */
    {
    int position;		/* Position in genome. */
    enum ggVertexType type;	/* hard/soft? start/end? */
    };

struct edge
/* An edge in our graph */
    {
    struct vertex *start;	/* Starting vertex. */
    struct vertex *end;		/* Ending vertex. */
    struct evidence *evList;	/* List of evidence. */
    };

static int vertexCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct vertex *a = va;
struct vertex *b = vb;
int diff = a->position - b->position;
if (diff == 0)
    diff = a->type - b->type;
return diff;
}

static int edgeCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct edge *a = va;
struct edge *b = vb;
int diff = vertexCmp(a->start, b->start);
if (diff == 0)
    diff = vertexCmp(a->end, b->end);
return diff;
}

struct vertex *matchingVertex(struct rbTree *tree, int position, enum ggVertexType type)
/* Find matching vertex.  Return NULL if none. */
{
struct vertex temp;
temp.position = position;
temp.type = type;
return rbTreeFind(tree, &temp);
}

struct vertex *addUniqueVertex(struct rbTree *tree, int position, enum ggVertexType type)
/* Find existing vertex if it exists, otherwise create and return new one. */
{
struct vertex *v = matchingVertex(tree, position, type);
if (v == NULL)
    {
    lmAllocVar(tree->lm, v);
    v->position = position;
    v->type = type;
    rbTreeAdd(tree, v);
    }
return v;
}

struct edge *matchingEdge(struct rbTree *tree, struct vertex *start, struct vertex *end)
/* Find matching edge. Return NULL if none. */
{
struct edge temp;
temp.start = start;
temp.end = end;
return rbTreeFind(tree, &temp);
}

struct edge *addUniqueEdge(struct rbTree *tree, struct vertex *start, struct vertex *end,
	struct linkedBeds *lb)
/* Find existing edge if it exists.  Otherwise create and return new one. 
 * Regardless add lb as evidence to edge. */
{
struct edge *e = matchingEdge(tree, start, end);
if (e == NULL)
    {
    lmAllocVar(tree->lm, e);
    e->start = start;
    e->end = end;
    rbTreeAdd(tree, e);
    }
struct evidence *ev;
lmAllocVar(tree->lm, ev);
ev->lb = lb;
ev->start = start->position;
ev->end = end->position;
slAddHead(&e->evList, ev);
return e;
}

struct txGraph *makeGraph(struct linkedBeds *lbList)
/* Create a graph corresponding to linkedBedsList */
{
/* Create tree of all unique vertices. */
struct rbTree *vertexTree = rbTreeNew(vertexCmp);
struct linkedBeds *lb;
for (lb = lbList; lb != NULL; lb = lb->next)
    {
    struct bed *bed;
    for (bed = lb->bedList; bed != NULL; bed = bed->next)
        {
	/* Add very beginning and end, they'll be soft. */
	addUniqueVertex(vertexTree, bed->chromStart, ggSoftStart);
	addUniqueVertex(vertexTree, bed->chromEnd, ggSoftEnd);

	/* Add internal hard ends. */
	int i, lastBlock = bed->blockCount-1;
	for (i=0; i<lastBlock; ++i)
	    {
	    addUniqueVertex(vertexTree, 
	    	bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i], ggHardEnd);
	    addUniqueVertex(vertexTree, 
	    	bed->chromStart + bed->chromStarts[i+1], ggHardStart);
	    }
	}
    }
verbose(2, "%d unique vertices\n", vertexTree->n);

/* Create tree of all unique edges */
struct rbTree *edgeTree = rbTreeNew(edgeCmp);
for (lb = lbList; lb != NULL; lb = lb->next)
    {
    struct bed *bed, *nextBed;
    for (bed = lb->bedList; bed != NULL; bed = nextBed)
        {
	nextBed = bed->next;

	/* Loop to add all introns and all but last exon. */
	struct vertex *start = matchingVertex(vertexTree, bed->chromStart, ggSoftStart);
	int i, lastBlock = bed->blockCount-1;
	for (i=0; i<lastBlock; ++i)
	    {
	    /* Add exon */
	    struct vertex *end = matchingVertex(vertexTree,
		start->position + bed->blockSizes[i], ggHardEnd);
	    addUniqueEdge(edgeTree, start, end, lb);

	    /* Add intron */
	    start = matchingVertex(vertexTree, 
		    bed->chromStart + bed->chromStarts[i+1], ggHardStart);
	    addUniqueEdge(edgeTree, end, start, lb);
	    }

	/* Add final exon */
	struct vertex *end = matchingVertex(vertexTree, bed->chromEnd, ggSoftEnd);
	addUniqueEdge(edgeTree, start, end, lb);

	/* If there's another bed to go, add a soft intron connecting it. */
	if (nextBed != NULL)
	    {
	    start = matchingVertex(vertexTree, nextBed->chromStart, ggSoftStart);
	    addUniqueEdge(edgeTree, end, start, lb);
	    }
	}
    }
verbose(2, "%d unique edges\n", edgeTree->n);

/* Clean up and go home. */
rbTreeFree(&vertexTree);
rbTreeFree(&edgeTree);
return NULL;	/* ugly */
}
