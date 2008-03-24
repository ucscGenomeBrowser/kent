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
#include "localmem.h"
#include "rbTree.h"
#include "rangeTree.h"
#include "dlist.h"
#include "dnautil.h"
#include "bed.h"
#include "ggTypes.h"
#include "nibTwo.h"
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

static boolean trustedEdge(struct edge *edge)
/* Return TRUE if any member of edge evidence is from a trusted source. */
{
struct evidence *ev;
for (ev = edge->evList; ev != NULL; ev = ev->next)
    {
    if (trustedSource(ev->lb->sourceType))
        return TRUE;
    }
return FALSE;
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

static void removeEmptyEdges(struct rbTree *vertexTree, struct rbTree *edgeTree)
/* Remove edges that are zero or negative in length. */
{
int removeCount = 0;
struct slRef *edgeRef, *edgeRefList = rbTreeItems(edgeTree);
for (edgeRef = edgeRefList; edgeRef != NULL; edgeRef = edgeRef->next)
    {
    struct edge *edge = edgeRef->val;
    if (edge->start->position >= edge->end->position)
        {
	removeCount += 1;
	rbTreeRemove(edgeTree, edge);
	}
    }
if (removeCount)
    removeUnusedVertices(vertexTree, edgeTree);
slFreeList(&edgeRefList);
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

static void addWaysInAndOut(struct rbTree *vertexTree, struct rbTree *edgeTree,
	struct lm *lm)
/* Put a bunch of ways in and out of the graph onto the edges. */
{
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

static boolean checkSeqSimilar(struct dnaSeq *a, struct dnaSeq *b, int minScore)
/* Check that two sequences are similar.  Assumes sequences are same size. */
{
int size = a->size;
int size1 = size-1;
return dnaScoreMatch(a->dna, b->dna, size) >= minScore 
	|| dnaScoreMatch(a->dna+1, b->dna, size1) >= minScore
	|| dnaScoreMatch(a->dna, b->dna+1, size1) >= minScore;
}

static boolean checkSnapOk(struct vertex *vOld, struct vertex *vNew, boolean isRev, int bleedSize,
	struct nibTwoCache *seqCache, char *chromName)
/* Load sequence that corresponds to bleed-over, and  make sure that sequence of next
 * exon is similar. */
{
int minScore = bleedSize-2;
boolean similar = FALSE;
if (isRev)
    {
    int oldStart = vOld->position;
    struct slRef *eRef;
    struct dnaSeq *oldSeq = nibTwoCacheSeqPartExt(seqCache, chromName, oldStart, bleedSize, FALSE, NULL);
    for (eRef = vNew->waysIn; eRef != NULL; eRef = eRef->next)
        {
	struct edge *edge = eRef->val;
	struct vertex *vRest = edge->start;
	int newStart = vRest->position - bleedSize;
	struct dnaSeq *newSeq = nibTwoCacheSeqPartExt(seqCache, chromName, newStart, bleedSize, FALSE, NULL);
	similar = checkSeqSimilar(oldSeq, newSeq, minScore);
	dnaSeqFree(&newSeq);
	if (similar)
	    break;
	}
    dnaSeqFree(&oldSeq);
    }
else
    {
    int oldStart = vOld->position - bleedSize;
    struct slRef *eRef;
    struct dnaSeq *oldSeq = nibTwoCacheSeqPartExt(seqCache, chromName, oldStart, bleedSize, FALSE, NULL);
    for (eRef = vNew->waysOut; eRef != NULL; eRef = eRef->next)
        {
	struct edge *edge = eRef->val;
	struct vertex *vRest = edge->end;
	int newStart = vRest->position;
	struct dnaSeq *newSeq = nibTwoCacheSeqPartExt(seqCache, chromName, newStart, bleedSize, FALSE, NULL);
	similar = checkSeqSimilar(oldSeq, newSeq, minScore);
	dnaSeqFree(&newSeq);
	if (similar)
	    break;
	}
    }
return similar;
}

static boolean snapVertex(struct dlNode *oldNode, int maxSnapSize, int maxUncheckedSnapSize,
	struct nibTwoCache *seqCache, char *chromName)
/* Snap vertex to nearby previous hard vertex. */
{
/* Figure out hard type we want to snap to, and return if not
 * soft to begin with. */
struct vertex *vOld = oldNode->val;
int oldPos = vOld->position;
enum ggVertexType currentType = vOld->type;

/* If no seqCache, then set up things so not bothering to check it. */
if (seqCache == NULL)
    maxUncheckedSnapSize = maxSnapSize;

/* Search backwards */
if (currentType == ggSoftEnd)
    {
    struct dlNode *node;
    for (node = oldNode->prev; !dlStart(node); node = node->prev)
	{
	struct vertex *v = node->val;
	int newPos = v->position;
	int dif = oldPos - newPos;
	if (dif > maxSnapSize)
	    break;
	if (v->type == ggHardEnd)
	    {
	    if (dif <= maxUncheckedSnapSize || checkSnapOk(vOld, v, FALSE, dif, seqCache, chromName))
		{
		vOld->movedTo = v;
		return TRUE;
		}
	    }
	}
    }

/* Search forward */
else if (currentType == ggSoftStart)
    {
    struct dlNode *node;
    for (node = oldNode->next; !dlEnd(node); node = node->next)
	{
	struct vertex *v = node->val;
	int newPos = v->position;
	int dif = newPos - oldPos;
	if (dif > maxSnapSize)
	    break;
	/* TODO - check if over maxUnchecked. */
	if (v->type == ggHardStart)
	    {
	    if (dif <= maxUncheckedSnapSize || checkSnapOk(vOld, v, TRUE, dif, seqCache, chromName))
		{
		vOld->movedTo = v;
		return TRUE;
		}
	    }
	}
    }
return FALSE;
}

static void mergeOrAddEdge(struct rbTree *edgeTree, struct edge *edge)
/* Add edge back if it is still unique, otherwise move evidence from
 * edge into existing edge. */
{
struct edge *existing = rbTreeFind(edgeTree, edge);
if (existing)
    {
    existing->evList = slCat(existing->evList, edge->evList);
    edge->evList = NULL;
    }
else
    rbTreeAdd(edgeTree, edge);
}

static void updateForwardedEdges(struct rbTree *edgeTree)
/* Go through edges, following the movedTo's in the
 * vertices if need be. */
{
struct slRef *ref, *refList = rbTreeItems(edgeTree);
int forwardCount = 0;
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
	mergeOrAddEdge(edgeTree, edge);
	}
    }
if (forwardCount > 0)
    verbose(3, "Forwarded %d edges.\n", forwardCount);
slFreeList(&refList);
}

static void snapSoftToCloseHard(struct rbTree *vertexTree, struct rbTree *edgeTree, int maxSnapSize,
	int maxUncheckedSnapSize, struct nibTwoCache *seqCache, char *chromName)
/* Snap hard vertices to nearby soft vertices of same type. */
{
struct lm *lm = lmInit(0);
addWaysInAndOut(vertexTree, edgeTree, lm);
struct dlList *vList = sortedListFromTree(vertexTree);
struct dlNode *node;
int snapCount = 0;
for (node = vList->head; !dlEnd(node); node = node->next)
    {
    if (snapVertex(node, maxSnapSize, maxUncheckedSnapSize, seqCache, chromName))
	{
	rbTreeRemove(vertexTree, node->val);
        ++snapCount;
	}
    }
/* Clean up ways in and out since have removed some nodes. */
for (node = vList->head; !dlEnd(node); node = node->next)
    {
    struct vertex *v = node->val;
    v->waysIn = v->waysOut = NULL;
    }
if (snapCount > 0)
    {
    verbose(3, "Snapped %d close edges, now have %d vertices\n", snapCount, vertexTree->n);
    updateForwardedEdges(edgeTree);
    }
dlListFree(&vList);
lmCleanup(&lm);
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
// enum ggVertex otherHardType = (hardType == ggHardStart ? ggHardEnd : ggHardStart);
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
    mergeOrAddEdge(edgeTree, softEdge);
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
// enum ggVertex otherHardType = (hardType == ggHardStart ? ggHardEnd : ggHardStart);
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
    mergeOrAddEdge(edgeTree, softEdge);
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
struct lm *lm = lmInit(0);
addWaysInAndOut(vertexTree, edgeTree, lm);
struct slRef *vRef, *vRefList = rbTreeItems(vertexTree);
for (vRef = vRefList; vRef != NULL; vRef = vRef->next)
    {
    struct vertex *v = vRef->val;
    snapHalfHard(v, edgeTree);
    v->waysIn = v->waysOut = NULL;	/* These are trashed so remove them. */
    }
slFreeList(&vRefList);
removeUnusedVertices(vertexTree, edgeTree);
lmCleanup(&lm);
}

struct sourceAndPos
/* A vertex source and position. */
    {
    struct sourceAndPos *next;
    int position;		/* Position in genome. */
    bool trustedSource;		/* If true this is trusted source (refSeq) */
    };

int sourceAndPosCmp(const void *va, const void *vb)
/* Compare two sourceAndPos by position. */
{
const struct sourceAndPos *a = *((struct sourceAndPos **)va);
const struct sourceAndPos *b = *((struct sourceAndPos **)vb);
return a->position - b->position;
}

int sourceAndPosCmpRev(const void *va, const void *vb)
/* Compare two sourceAndPos in reverse direction. */
{
const struct sourceAndPos *a = *((struct sourceAndPos **)va);
const struct sourceAndPos *b = *((struct sourceAndPos **)vb);
return b->position - a->position;
}

static struct vertex *consensusVertex(struct rbTree *vertexTree, struct sourceAndPos *list, 
	int listSize, enum ggVertexType softType)
/* Return first trusted (refSeq) vertex, or if no trusted one, then 
 * a soft vertex 1/4 of way (rounded down) through list. */
{
struct sourceAndPos *el;
for (el = list; el != NULL; el = el->next)
    {
    if (el->trustedSource)
        break;
    }
if (el == NULL)
    el = slElementFromIx(list, listSize/4);
return matchingVertex(vertexTree, el->position, softType);
}

static void halfConsensusForward(struct vertex *v, 
	struct rbTree *vertexTree, struct rbTree *edgeTree,
	enum ggVertexType softType, struct lm *lm)
/* Figure out consensus end of all edges beginning at v that have soft end. */
{
/* Collect a list of all attached softies. */
struct sourceAndPos *list = NULL, *el;
struct slRef *edgeRef;
int softCount = 0;
for (edgeRef = v->waysOut; edgeRef != NULL; edgeRef = edgeRef->next)
    {
    struct edge *edge = edgeRef->val;
    struct vertex *v = edge->end;
    if (v->type == softType)
        {
	struct evidence *ev;
	for (ev = edge->evList; ev != NULL; ev = ev->next)
	    {
	    lmAllocVar(lm, el);
	    el->position = ev->end;
	    el->trustedSource = trustedSource(ev->lb->sourceType);
	    slAddHead(&list, el);
	    ++softCount;
	    }
	}
    }

/* See if have enough elements to make consensus forming
 * worthwhile. */
if (softCount > 1)
    {
    slSort(&list, sourceAndPosCmpRev);
    struct vertex *end = consensusVertex(vertexTree, list, softCount, softType);
    for (edgeRef = v->waysOut; edgeRef != NULL; edgeRef = edgeRef->next)
	{
	struct edge *edge = edgeRef->val;
	struct vertex *v = edge->end;
	if (v != end && v->type == softType)
	    {
	    rbTreeRemove(edgeTree, edge);
	    edge->end = end;
	    mergeOrAddEdge(edgeTree, edge);  // Will always merge. 
	    }
	}
    }
}


static void halfConsensusBackward(struct vertex *v, 
	struct rbTree *vertexTree, struct rbTree *edgeTree,
	enum ggVertexType softType, struct lm *lm)
/* Figure out consensus start of all edges end at v that have soft start. */
{
/* Collect a list of all attached softies. */
struct sourceAndPos *list = NULL, *el;
struct slRef *edgeRef;
int softCount = 0;
for (edgeRef = v->waysIn; edgeRef != NULL; edgeRef = edgeRef->next)
    {
    struct edge *edge = edgeRef->val;
    struct vertex *v = edge->start;
    if (v->type == softType)
        {
	struct evidence *ev;
	for (ev = edge->evList; ev != NULL; ev = ev->next)
	    {
	    lmAllocVar(lm, el);
	    el->position = ev->start;
	    el->trustedSource = trustedSource(ev->lb->sourceType);
	    slAddHead(&list, el);
	    ++softCount;
	    }
	}
    }

/* See if have enough elements to make consensus forming
 * worthwhile. */
if (softCount > 1)
    {
    slSort(&list, sourceAndPosCmp);
    struct vertex *start = consensusVertex(vertexTree, list, softCount, softType);
    for (edgeRef = v->waysIn; edgeRef != NULL; edgeRef = edgeRef->next)
	{
	struct edge *edge = edgeRef->val;
	struct vertex *v = edge->start;
	if (v != start && v->type == softType)
	    {
	    rbTreeRemove(edgeTree, edge);
	    edge->start = start;
	    mergeOrAddEdge(edgeTree, edge);  // Will always merge. 
	    }
	}
    }
}

static void halfHardConsensus(struct vertex *v, 
	struct rbTree *vertexTree, struct rbTree *edgeTree,
	struct lm *lm)
/* Form consensus of soft vertices connected to hard vertex v. 
 * Consensus is:
 *    1) The largest refSeq.
 *    2) If no refSeq, something 3/4 of the way to the largest. */
{
enum ggVertexType currentType = v->type;
enum ggVertexType softType;

/* Figure out what types look for at the other end.  If we're
 * a soft vertex we can't do anything. */
if (currentType == ggHardStart)
    softType = ggSoftEnd;
else if (currentType == ggHardEnd)
    softType = ggSoftStart;
else
    return;

halfConsensusForward(v, vertexTree, edgeTree, softType, lm);
halfConsensusBackward(v, vertexTree, edgeTree, softType, lm);
}

static void halfHardConsensuses(struct rbTree *vertexTree, struct rbTree *edgeTree)
/* Collect soft edges that share a hard edge. Move all of them to a consensus
 * vertex, which is either:
 *    1) The largest refSeq.
 *    2) If no refSeq, something 3/4 of the way to the largest. */
{
struct lm *lm = lmInit(0);
addWaysInAndOut(vertexTree, edgeTree, lm);
struct slRef *vRef, *vRefList = rbTreeItems(vertexTree);
for (vRef = vRefList; vRef != NULL; vRef = vRef->next)
    {
    struct vertex *v = vRef->val;
    halfHardConsensus(v, vertexTree, edgeTree, lm);
    v->waysIn = v->waysOut = NULL;	/* These are trashed so remove them. */
    }
slFreeList(&vRefList);
removeUnusedVertices(vertexTree, edgeTree);
lmCleanup(&lm);
}

struct edge *edgeFromConsensusOfEvidence(struct rbTree *vertexTree, struct evidence *evList,
	struct lm *lm)
/* Attempt to create a single edge from a list of overlapping evidence ranges.
 * The start will be the consensus of all evidence starts.  Likewise
 * the end will be the consensus of all evidence ends.  The evidence that
 * overlaps this edge will be included in the edge. */
{
/* Gather up lists of starts and ends. */
struct sourceAndPos *startList = NULL, *endList = NULL;
struct evidence *ev, *nextEv;
int listSize = 0;
for (ev = evList; ev != NULL; ev = ev->next)
    {
    struct sourceAndPos *x;
    boolean trusted = trustedSource(ev->lb->sourceType);
    lmAllocVar(lm, x);
    x->position = ev->start;
    x->trustedSource = trusted;
    slAddHead(&startList, x);
    lmAllocVar(lm, x);
    x->position = ev->end;
    x->trustedSource = trusted;
    slAddHead(&endList, x);
    ++listSize;
    }

/* Get consensus starts and ends. */
slSort(&startList, sourceAndPosCmp);
struct vertex *start = consensusVertex(vertexTree, startList, listSize, ggSoftStart);
slSort(&endList, sourceAndPosCmpRev);
struct vertex *end = consensusVertex(vertexTree, endList, listSize, ggSoftEnd);

/* Make edge */
struct edge *edge;
AllocVar(edge);
edge->start = start;
edge->end = end;

/* Add overlapping evidence to edge. */
for (ev = evList; ev != NULL; ev = nextEv)
    {
    nextEv = ev->next;
    if (rangeIntersection(ev->start, ev->end, start->position, end->position) > 0)
        slAddHead(&edge->evList, ev);
    }

return edge;
}

static void removeEnclosedDoubleSofts(struct rbTree *vertexTree, struct rbTree *edgeTree, 
	int maxBleedOver, double singleExonMaxOverlap)
/* Move double-softs that overlap spliced things to a very great extent into
 * the spliced things. Also remove tiny double-softs (no more than 2*maxBleedOver). */
{
/* Traverse graph and build up range tree covering spliced exons*/
struct rbTree *rangeTree = rangeTreeNew(0);
struct slRef *edgeRef, *edgeRefList = rbTreeItems(edgeTree);
int removedCount = 0;
for (edgeRef = edgeRefList; edgeRef != NULL; edgeRef = edgeRef->next)
    {
    struct edge *edge = edgeRef->val;
    struct vertex *start = edge->start;
    struct vertex *end = edge->end;
    if (start->type == ggHardStart || end->type == ggHardEnd)
	{
	struct range *r = rangeTreeAdd(rangeTree, start->position, end->position);
	r->val = edge;
	}
    }

/* Traverse graph yet one more time looking for doubly-soft exons
 * that are overlapping the spliced exons. */
for (edgeRef = edgeRefList; edgeRef != NULL; edgeRef = edgeRef->next)
    {
    struct edge *edge = edgeRef->val;
    struct vertex *start = edge->start;
    struct vertex *end = edge->end;
    if (start->type == ggSoftStart && end->type == ggSoftEnd)
        {
	int s = start->position;
	int e = end->position;
	int size = e - s;
	if (size <= maxBleedOver+maxBleedOver)
	     {
	     /* Tiny case, just remove edge and forget it. */
	     rbTreeRemove(edgeTree, edge);
	     ++removedCount;
	     }
	else
	     {
	     /* Normal case, look for exon list that encloses us, and
	      * if any single exon in that list encloses us, merge into it. */
	     int splicedOverlap = rangeTreeOverlapSize(rangeTree, s, e);
	     if (splicedOverlap > 0 && splicedOverlap > singleExonMaxOverlap*size)
	         {
		 if (!trustedEdge(edge))
		     {
		     struct range *r = rangeTreeMaxOverlapping(rangeTree, s, e);
		     struct edge *bigEdge = r->val;
		     bigEdge->evList = slCat(bigEdge->evList, edge->evList);
		     edge->evList = NULL;
		     rbTreeRemove(edgeTree, edge);
		     ++removedCount;
		     }
		 }
	     }
	}
    }

/* Clean up and go home. */
if (removedCount > 0)
    removeUnusedVertices(vertexTree, edgeTree);
slFreeList(&edgeRefList);
rbTreeFree(&rangeTree);
}

static void mergeDoubleSofts(struct rbTree *vertexTree, struct rbTree *edgeTree)
/* Merge together overlapping edges with soft ends. */
{
struct mergedEdge
/* Hold together info on a merged edge. */
    {
    struct evidence *evidence;
    };

/* Traverse graph and build up range tree */
struct rbTree *rangeTree = rangeTreeNew(0);
struct slRef *edgeRef, *edgeRefList = rbTreeItems(edgeTree);
for (edgeRef = edgeRefList; edgeRef != NULL; edgeRef = edgeRef->next)
    {
    struct edge *edge = edgeRef->val;
    struct vertex *start = edge->start;
    struct vertex *end = edge->end;
    if (start->type == ggSoftStart && end->type == ggSoftEnd)
        rangeTreeAdd(rangeTree, start->position, end->position);
    }

/* Traverse graph again merging edges */
for (edgeRef = edgeRefList; edgeRef != NULL; edgeRef = edgeRef->next)
    {
    struct edge *edge = edgeRef->val;
    struct vertex *start= edge->start;
    struct vertex *end = edge->end;
    if (start->type == ggSoftStart && end->type == ggSoftEnd)
        {
	struct range *r = rangeTreeFindEnclosing(rangeTree,
		start->position, end->position);
	assert(r != NULL);
	struct mergedEdge *mergeEdge = r->val;
	if (mergeEdge == NULL)
	    {
	    lmAllocVar(rangeTree->lm, mergeEdge);
	    r->val = mergeEdge;
	    }
	mergeEdge->evidence = slCat(edge->evList, mergeEdge->evidence);
	edge->evList = NULL;
	rbTreeRemove(edgeTree, edge);
	}
    }

/* Traverse merged edge list, making a single edge from each range.  */
struct range *r;
struct lm *lm = lmInit(0);
for (r = rangeTreeList(rangeTree); r != NULL; r = r->next)
    {
    struct mergedEdge *mergedEdge = r->val;
    struct edge *edge = edgeFromConsensusOfEvidence(vertexTree, mergedEdge->evidence, lm);
    if (edge != NULL)
	rbTreeAdd(edgeTree, edge);
    }

/* Clean up and go home. */
lmCleanup(&lm);
removeUnusedVertices(vertexTree, edgeTree);
slFreeList(&edgeRefList);
rbTreeFree(&rangeTree);
}

#ifdef DEBUG
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
#endif /* DEBUG */

struct txGraph *treeTreeToTxg(struct rbTree *vertexTree, struct rbTree *edgeTree, char *name,
	struct linkedBeds *lbList)
/* Convert from vertexTree/edgeTree representation to txGraph */
{
/* Allocate txGraph, and fill in some of the relatively easy fields. */
struct txGraph *txg;
AllocVar(txg);
struct bed *bed = lbList->bedList;
txg->name = cloneString(name);
txg->tName = cloneString(bed->chrom);
txg->strand[0] = bed->strand[0];
txg->vertexCount = vertexTree->n;
txg->edgeCount = edgeTree->n;

/* Get vertex list and number sequentially. Fill in vertex array */
int i;
struct slRef *vRef, *vRefList = rbTreeItems(vertexTree);
struct txVertex *tv = AllocArray(txg->vertices, vertexTree->n);
int tStart = BIGNUM, tEnd = -BIGNUM;
for (vRef = vRefList, i=0; vRef != NULL; vRef = vRef->next, i++)
    {
    struct vertex *v = vRef->val;
    int position = v->position;
    v->count = i;
    tv->position = position;
    tv->type = v->type;
    tStart = min(tStart, position);
    tEnd = max(tEnd, position);
    ++tv;
    }
slFreeList(&vRefList);

/* Fill in overall bounds. */
txg->tStart = tStart;
txg->tEnd = tEnd;

/* Deal with sources. */
int sourceCount = 0;
struct linkedBeds *lb;
for (lb = lbList; lb != NULL; lb = lb->next)
    lb->id = sourceCount++;
struct txSource *source = AllocArray(txg->sources, sourceCount);
for (lb = lbList; lb != NULL; lb = lb->next)
    {
    source->accession = cloneString(lb->bedList->name);
    source->type = cloneString(lb->sourceType);
    ++source;
    }
txg->sourceCount = sourceCount;

/* Convert edges */
struct slRef *edgeRef, *edgeRefList = rbTreeItems(edgeTree);
for (edgeRef = edgeRefList; edgeRef != NULL; edgeRef = edgeRef->next)
    {
    struct edge *edge = edgeRef->val;

    /* Allocate edge and fill in start and end. */
    struct txEdge *te;
    AllocVar(te);
    struct vertex *start = edge->start;
    struct vertex *end = edge->end;
    te->startIx = start->count;
    te->endIx = end->count;

    /* Figure out whether it's an intron or exon. */
    enum ggVertexType startType = edge->start->type;
    if (startType == ggHardStart || startType == ggSoftStart)
        te->type = ggExon;
    else
        te->type = ggIntron;

    /* Convert the evidence. */
    struct evidence *ev;
    for (ev = edge->evList; ev != NULL; ev = ev->next)
        {
	struct txEvidence *tev;
	AllocVar(tev);
	tev->sourceId = ev->lb->id;
	tev->start = ev->start;
	tev->end = ev->end;
	slAddHead(&te->evList, tev);
	te->evCount += 1;
	}
    slReverse(&te->evList);

    slAddHead(&txg->edgeList, te);
    }
slReverse(&txg->edgeList);
return txg;
}

struct txGraph *makeGraph(struct linkedBeds *lbList, int maxBleedOver, 
	int maxUncheckedBleed, struct nibTwoCache *seqCache,
	double singleExonMaxOverlap, char *name)
/* Create a graph corresponding to linkedBedsList.
 * The maxBleedOver parameter controls how much of a soft edge that
 * can be cut off when snapping to a hard edge.  The singleExonMaxOverlap
 * controls what ratio of a single exon transcript can overlap spliced 
 * transcripts */
{
char *chromName = lbList->bedList->chrom;

/* Create tree of all unique vertices. */
struct rbTree *vertexTree = makeVertexTree(lbList);
verbose(2, "%d unique vertices\n", vertexTree->n);

/* Create tree of all unique edges */
struct rbTree *edgeTree = makeEdgeTree(lbList, vertexTree);
verbose(2, "%d unique edges\n", edgeTree->n);

snapSoftToCloseHard(vertexTree, edgeTree, maxBleedOver, maxUncheckedBleed, seqCache, chromName);
verbose(2, "%d edges, %d vertices after snapSoftToCloseHard\n", 
	edgeTree->n, vertexTree->n);

removeEmptyEdges(vertexTree, edgeTree);
verbose(2, "%d edges, %d vertices after removeEmptyEdges\n",
	edgeTree->n, vertexTree->n);

snapHalfHards(vertexTree, edgeTree);
verbose(2, "%d edges, %d vertices after snapHalfHards\n", 
	edgeTree->n, vertexTree->n);

halfHardConsensuses(vertexTree, edgeTree);
verbose(2, "%d edges, %d vertices after medianHalfHards\n", 
	edgeTree->n, vertexTree->n);

removeEnclosedDoubleSofts(vertexTree, edgeTree, maxBleedOver, singleExonMaxOverlap);
verbose(2, "%d edges, %d vertices after mergeEnclosedDoubleSofts\n",
	edgeTree->n, vertexTree->n);

mergeDoubleSofts(vertexTree, edgeTree);
verbose(2, "%d edges, %d vertices after mergeDoubleSofts\n",
	edgeTree->n, vertexTree->n);

struct txGraph *txg = treeTreeToTxg(vertexTree, edgeTree, name, lbList);

/* Clean up and go home. */
rbTreeFree(&vertexTree);
rbTreeFree(&edgeTree);
return txg;
}
