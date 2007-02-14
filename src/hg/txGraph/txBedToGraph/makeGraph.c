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
 *     the edge boundaries equal to the median value of all the merged boundaries. 
 * Through most of the algorithm the graph is represented as two binary trees, one
 * for vertices and one for edges. At the end this is converted to the txGraph
 * structure. */

#include "common.h"
#include "hash.h"
#include "rbTree.h"
#include "dlist.h"
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
    struct vertex *movedTo;	/* Forwarding address (temporary) */
    struct slRef *waysIn;	/* References to preceeding edges (temporary). */
    struct slRef *waysOut;	/* References to following edges (temporary). */
    int count;			/* Number of times used, or index of vertex (temporary). */
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

static struct vertex *matchingVertex(struct rbTree *tree, int position, enum ggVertexType type)
/* Find matching vertex.  Return NULL if none. */
{
struct vertex temp;
temp.position = position;
temp.type = type;
return rbTreeFind(tree, &temp);
}

static struct vertex *addUniqueVertex(struct rbTree *tree, int position, enum ggVertexType type)
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

static struct edge *matchingEdge(struct rbTree *tree, struct vertex *start, struct vertex *end)
/* Find matching edge. Return NULL if none. */
{
struct edge temp;
temp.start = start;
temp.end = end;
return rbTreeFind(tree, &temp);
}

static struct edge *addUniqueEdge(struct rbTree *tree, struct vertex *start, struct vertex *end,
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

static struct rbTree *makeVertexTree(struct linkedBeds *lbList)
/* Make tree of unique vertices. */
{
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
return vertexTree;
}

static struct rbTree *makeEdgeTree(struct linkedBeds *lbList, struct rbTree *vertexTree)
/* Make tree of unique edges. */
{
struct rbTree *edgeTree = rbTreeNew(edgeCmp);
struct linkedBeds *lb;
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
return edgeTree;
}

static struct dlList *sortedListFromTree(struct rbTree *tree)
/* Create a double-linked list from tree. List will be sorted.  */
{
struct slRef *ref, *refList = rbTreeItems(tree);
struct dlList *list = dlListNew();
for (ref = refList; ref != NULL; ref = ref->next)
    dlAddValTail(list, ref->val);
slFreeList(&refList);
return list;
}

static boolean snapVertex(struct dlNode *oldNode, int maxSnapSize)
/* Snap vertex to nearby hard vertex in forward direction */
{
/* Figure out hard type we want to snap to, and return if not
 * soft to begin with. */
struct vertex *vOld = oldNode->val;
enum ggVertexType targetType, currentType = vOld->type;
if (currentType == ggSoftStart)
    targetType = ggHardStart;
else if (currentType == ggSoftEnd)
    targetType = ggHardEnd;
else
    return FALSE;

/* Search forward */
int newLimit = vOld->position + maxSnapSize;
struct dlNode *node;
for (node = oldNode->next; !dlEnd(node); node = node->next)
    {
    struct vertex *v = node->val;
    if (v->position > newLimit)
        break;
    if (v->type == targetType)
        {
	vOld->movedTo = v;
	return TRUE;
	}
    }

/* Search backwards */
newLimit = vOld->position - maxSnapSize;
for (node = oldNode->prev; !dlStart(node); node = node->prev)
    {
    struct vertex *v = node->val;
    if (v->position < newLimit)
        break;
    if (v->type == targetType)
        {
	vOld->movedTo = v;
	return TRUE;
	}
    }

return FALSE;
}

static void updateForwardedEdges(struct rbTree *edgeTree)
/* Go through edges, following the movedTo's in the
 * vertices if need be. */
{
struct slRef *ref, *refList = rbTreeItems(edgeTree);
int forwardCount = 0, addedBackCount = 0;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct edge *edge = ref->val;
    struct vertex *start = edge->start, *end = edge->end;
    if (start->movedTo || end->movedTo)
        {
	++forwardCount;
	rbTreeRemove(edgeTree, edge);
	if (start->movedTo)
	    edge->start = start->movedTo;
	if (end->movedTo)
	    edge->end = end->movedTo;
	if (!rbTreeFind(edgeTree, edge))
	    {
	    rbTreeAdd(edgeTree, edge);
	    ++addedBackCount;
	    }
	}
    }
if (forwardCount > 0)
    verbose(3, "Forwarded %d edges, of which %d are still unique\n", 
	    forwardCount, addedBackCount);
slFreeList(&refList);
}

static void snapSoftToCloseHard(struct rbTree *vertexTree, struct rbTree *edgeTree, int maxSnapSize)
/* Snap hard vertices to nearby soft vertices of same type. */
{
struct dlList *vList = sortedListFromTree(vertexTree);
struct dlNode *node;
int snapCount = 0;
for (node = vList->head; !dlEnd(node); node = node->next)
    {
    if (snapVertex(node, maxSnapSize))
	{
	rbTreeRemove(vertexTree, node->val);
        ++snapCount;
	}
    }
if (snapCount > 0)
    {
    verbose(3, "Snapped %d close edges, now have %d vertices\n", snapCount, vertexTree->n);
    updateForwardedEdges(edgeTree);
    }
dlListFree(&vList);
}


static void addWaysInAndOut(struct rbTree *vertexTree, struct rbTree *edgeTree)
/* Put a bunch of ways in and out of the graph onto the edges. */
{
struct lm *lm = vertexTree->lm;	/* We'll store the waysIn/Out here. */
struct slRef *edgeRef, *edgeRefList = rbTreeItems(edgeTree);
for (edgeRef = edgeRefList; edgeRef != NULL; edgeRef = edgeRef->next)
    {
    struct edge *edge = edgeRef->val;
    struct slRef *inRef, *outRef;
    lmAllocVar(lm, inRef);
    lmAllocVar(lm, outRef);
    inRef->val = outRef->val = edge;
    slAddHead(&edge->start->waysOut, outRef);
    slAddHead(&edge->end->waysIn, inRef);
    }
slFreeList(&edgeRefList);
}

int edgeRefCmpEnd(const void *va, const void *vb)
/* Comparison to sort a slRef list of edges
 * by the edge end location. */
{
const struct slRef *a = *((struct slRef **)va);
const struct slRef *b = *((struct slRef **)vb);
struct edge *aEdge = a->val;
struct edge *bEdge = b->val;
return aEdge->end->position - bEdge->end->position;
}

int edgeRefCmpStartRev(const void *va, const void *vb)
/* Comparison to sort a slRef list of edges
 * by the edge start location, with biggest biggest
 * starts first. */
{
const struct slRef *a = *((struct slRef **)va);
const struct slRef *b = *((struct slRef **)vb);
struct edge *aEdge = a->val;
struct edge *bEdge = b->val;
return bEdge->end->position - aEdge->end->position;
}

int snapHalfHardForward(struct vertex *v, struct rbTree *edgeTree,
	enum ggVertexType softType, enum ggVertexType hardType)
/* V is a hard vertex. Try to snap soft end vertices connected to v
 * to nearest hard vertex connected to v. */
{
int snapCount = 0;
slSort(&v->waysOut, edgeRefCmpEnd); 
struct slRef *hardRef = v->waysOut, *softRef = v->waysOut;
for (;;)
    {
    /* Advance softRef to next soft ended edge. */
    for (;softRef != NULL; softRef = softRef->next)
        {
	struct edge *edge = softRef->val;
	if (edge->end->type == softType)
	    break;
	}
    if (softRef == NULL)
        break;
    struct edge *softEdge = softRef->val;

    /* If hardRef is before softRef (or it's not hard) advance it */
    for (;hardRef != NULL; hardRef = hardRef->next)
        {
	struct edge *edge = hardRef->val;
	if (edge->end->type == hardType && edge->end->position >= softEdge->end->position)
	   break;
	}
    if (hardRef == NULL)
        break;
    rbTreeRemove(edgeTree, softEdge);
    struct edge *hardEdge = hardRef->val;
    softEdge->end = hardEdge->end;
    ++snapCount;
    if (!rbTreeFind(edgeTree, softEdge))
        rbTreeAdd(edgeTree, softEdge);
    softRef = softRef->next;
    }
if (snapCount > 0)
    verbose(3, "Snapped %d forward\n", snapCount);
return snapCount;
}

int snapHalfHardBackward(struct vertex *v, struct rbTree *edgeTree,
	enum ggVertexType softType, enum ggVertexType hardType)
/* V is a hard vertex. Try to snap soft start vertices connected to v
 * to nearest hard vertex connected to v. */
{
int snapCount = 0;
slSort(&v->waysIn, edgeRefCmpStartRev); 
struct slRef *hardRef = v->waysIn, *softRef = v->waysIn;
for (;;)
    {
    /* Advance softRef to next soft ended edge. */
    for (;softRef != NULL; softRef = softRef->next)
        {
	struct edge *edge = softRef->val;
	if (edge->start->type == softType)
	    break;
	}
    if (softRef == NULL)
        break;
    struct edge *softEdge = softRef->val;

    /* If hardRef is before softRef (or it's not hard) advance it */
    for (;hardRef != NULL; hardRef = hardRef->next)
        {
	struct edge *edge = hardRef->val;
	if (edge->start->type == hardType && edge->start->position <= softEdge->start->position)
	   break;
	}
    if (hardRef == NULL)
        break;
    rbTreeRemove(edgeTree, softEdge);
    struct edge *hardEdge = hardRef->val;
    softEdge->start = hardEdge->start;
    ++snapCount;
    if (!rbTreeFind(edgeTree, softEdge))
        rbTreeAdd(edgeTree, softEdge);
    softRef = softRef->next;
    }
if (snapCount > 0)
    verbose(3, "Snapped %d reverse\n", snapCount);
return snapCount;
}

static int snapHalfHard(struct vertex *v, struct rbTree *edgeTree)
/* Snap soft ends of half-hard edges connected to v to 
 * compatible hard end.  Returns number of snaps it's made. */
{
enum ggVertexType currentType = v->type;
enum ggVertexType softType, hardType;

/* Figure out what types look for at the other end.  If we're
 * a soft vertex we can't do anything. */
if (currentType == ggHardStart)
    {
    softType = ggSoftEnd;
    hardType = ggHardEnd;
    }
else if (currentType == ggHardEnd)
    {
    softType = ggSoftStart;
    hardType = ggHardStart;
    }
else
    return 0;

int snapCount = 0;
snapCount += snapHalfHardForward(v, edgeTree, softType, hardType);
snapCount += snapHalfHardBackward(v, edgeTree, softType, hardType);
return snapCount;
}

static void snapHalfHards(struct rbTree *vertexTree, struct rbTree *edgeTree)
/* Snap edges that are half hard to edges that are fully hard and that
 * share the original hard end */
{
addWaysInAndOut(vertexTree, edgeTree);
struct slRef *vRef, *vRefList = rbTreeItems(vertexTree);
for (vRef = vRefList; vRef != NULL; vRef = vRef->next)
    {
    struct vertex *v = vRef->val;
    snapHalfHard(v, edgeTree);
    v->waysIn = v->waysOut = NULL;	/* These are trashed so remove them. */
    }
slFreeList(&vRefList);
}


static void incVertexUses(void *item)
/* Callback to clear edge->start->useCount and edge->end->useCount. */
{
struct edge *edge = item;
edge->start->count += 1; 
edge->end->count += 1;
}

static void removeUnusedVertices(struct rbTree *vertexTree, struct rbTree *edgeTree)
/* Remove vertices not connected to any edges. */
{
/* Get vertex list and clear counts. */
struct slRef *vRef, *vRefList = rbTreeItems(vertexTree);
for (vRef = vRefList; vRef != NULL; vRef = vRef->next)
    {
    struct vertex *v = vRef->val;
    v->count = 0;
    }

/* Inc counts of vertices connected to edges. */
rbTreeTraverse(edgeTree, incVertexUses);

/* Remove unused vertices. */
for (vRef = vRefList; vRef != NULL; vRef = vRef->next)
    {
    struct vertex *v = vRef->val;
    if (v->count == 0)
        rbTreeRemove(vertexTree, v);
    }

slFreeList(&vRefList);
}

static void dumpVertices(struct rbTree *vertexTree)
{
struct slRef *vRef, *vRefList = rbTreeItems(vertexTree);
for (vRef = vRefList; vRef != NULL; vRef = vRef->next)
    {
    struct vertex *v = vRef->val;
    printf(" %d(%d)", v->position, v->type);
    }
printf("\n");
slFreeList(&vRefList);
}
#ifdef DEBUG
#endif /* DEBUG */

struct txGraph *makeGraph(struct linkedBeds *lbList, int maxBleedOver, char *name)
/* Create a graph corresponding to linkedBedsList.
 * The maxBleedOver parameter controls how much of a soft edge that
 * can be cut off when snapping to a hard edge. */
{
/* Create tree of all unique vertices. */
struct rbTree *vertexTree = makeVertexTree(lbList);
verbose(2, "%d unique vertices\n", vertexTree->n);

/* Create tree of all unique edges */
struct rbTree *edgeTree = makeEdgeTree(lbList, vertexTree);
verbose(2, "%d unique edges\n", edgeTree->n);

dumpVertices(vertexTree);
snapSoftToCloseHard(vertexTree, edgeTree, maxBleedOver);
verbose(2, "%d edges, %d vertices after snapSoftToCloseHard\n", 
	edgeTree->n, vertexTree->n);
dumpVertices(vertexTree);
snapHalfHards(vertexTree, edgeTree);
verbose(2, "%d edges, %d vertices after snapHalfHards\n", 
	edgeTree->n, vertexTree->n);
dumpVertices(vertexTree);
removeUnusedVertices(vertexTree, edgeTree);
dumpVertices(vertexTree);
verbose(2, "%d edges, %d vertices after removeUnusedVertices\n", 
	edgeTree->n, vertexTree->n);


/* Clean up and go home. */
rbTreeFree(&vertexTree);
rbTreeFree(&edgeTree);
return NULL;	/* ugly */
}
