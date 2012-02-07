/* boxClump - put together 2 dimensional boxes that
 * overlap with each other into clumps. */

#include "common.h"
#include "dlist.h"
#include "localmem.h"
#include "boxClump.h"


/** Some simple utility function on globally declared 
 ** data structures. **/


void boxClumpFree(struct boxClump **pClump)
/* Free boxClump. */
{
struct boxClump *clump = *pClump;
if (clump != NULL)
    {
    slFreeList(&clump->boxList);
    freez(pClump);
    }
}

void boxClumpFreeList(struct boxClump **pList)
/* Free list of boxClumps. */
{
struct boxClump *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    boxClumpFree(&el);
    }
*pList = NULL;
}

int boxClumpCmpCount(const void *va, const void *vb)
/* Compare to sort based on count of boxes. */
{
const struct boxClump *a = *((struct boxClump **)va);
const struct boxClump *b = *((struct boxClump **)vb);
return b->boxCount - a->boxCount;
}

/** Local data structures. */

struct cluster
/* A cluster of boxes. */
    {
    struct box *boxList;	/* List of boxes. */
    struct dlNode *node;	/* Position in doubly-linked list. */
    };

struct box
/* A box in an alignment. */
    {
    struct box *next;		 /* Next in list (usually on cluster). */
    struct boxIn *in;		 /* Input box position. */
    struct cluster *cluster;     /* Cluster this is part of. */
    };

struct boxRef
/* Reference to a box. */
    {
    struct boxRef *next;
    struct box *box;
    };


/** Routines that cluster using mostly the local data
 ** structures. **/


static void mergeClusters(struct cluster *a, struct cluster *b)
/* Merge cluster b into cluster a, and free remnants of b. */
{
struct box *box;

/* Merging with yourself is always problematic. */
if (a == b)
    return;

/* Point all boxes in b to a. */
for (box = b->boxList; box != NULL; box = box->next)
    box->cluster = a;

/* Add b's boxList to end of a's. */
a->boxList = slCat(a->boxList, b->boxList);

/* Remove b from master cluster list. */
dlRemove(b->node);
}

static boolean allStartBy(struct boxRef *refList, int qStart, int tStart)
/* Return TRUE if qStart/qEnd of all boxes less than or equal to qStart/tStart 
 * This is an end condition for recursion along with only having two or
 * less boxes in the refList.  It handles the case where you have many
 * boxes stacked on top of each other. */
{
struct boxRef *ref;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct boxIn *box = ref->box->in;
    if (box->qStart > qStart || box->tStart > tStart)
        return FALSE;
    }
return TRUE;
}

static boolean allSameCluster(struct boxRef *refList)
/* Return TRUE if qStart/qEnd of all boxes is the same */
{
struct boxRef *ref;
struct cluster *cluster = refList->box->cluster;
for (ref = refList->next; ref != NULL; ref = ref->next)
    {
    if (ref->box->cluster != cluster)
        return FALSE;
    }
return TRUE;
}

static struct lm *lm;
/* Local memory manager used for boxRefs and other stuff. */

static void rBoxJoin(struct boxRef *refList, 
	int qStart, int qEnd, int tStart, int tEnd)
/* Recursively cluster boxes. */
{
int boxCount = slCount(refList);

if (boxCount <= 1)
    {
    /* Easy: no merging required. */
    }
else if (boxCount == 2)
    {
    /* Decide if pair overlaps and if so merge. */
    struct box *a = refList->box;
    struct box *b = refList->next->box;
    if (rangeIntersection(a->in->qStart, a->in->qEnd, b->in->qStart, b->in->qEnd) > 0 &&
        rangeIntersection(a->in->tStart, a->in->tEnd, b->in->tStart, b->in->tEnd) > 0 )
	{
	mergeClusters(a->cluster, b->cluster);
	}
    else
        {
	/* Two non-overlapping boxes, we don't have to do anything. */
	}
    }
else if (allStartBy(refList, qStart, tStart))
    {
    /* If everybody contains the upper left corner, then they all can 
     * be merged.   This is the route taken often by clumps with lots
     * of overlap. */
    struct cluster *aCluster = refList->box->cluster;
    struct boxRef *ref;
    for (ref = refList->next; ref != NULL; ref = ref->next)
        {
	struct cluster *bCluster = ref->box->cluster;
	mergeClusters(aCluster, bCluster);
	}
    }
else if (allSameCluster(refList))
    {
    /* Everything is in the same cluster, no action required. */
    }
else
    {
    /* We can't yet figure out clumping, so break
     * up our window in two along larger dimension and
     * recurse on both subwindows. */
    struct boxRef *list1 = NULL, *list2 = NULL, *ref, *next;
    if (qEnd - qStart > tEnd - tStart)
        {
	int mid = (qStart + qEnd)>>1;
	for (ref = refList; ref != NULL; ref = next)
	    {
	    struct box *box = ref->box;
	    next = ref->next;
	    if (box->in->qEnd <= mid)
	        {
		slAddHead(&list1, ref);
		}
	    else if (box->in->qStart >= mid)
	        {
		slAddHead(&list2, ref);
		}
	    else
	        {
		/* Box crosses boundary, have to put it on both lists. */
		slAddHead(&list1, ref);
		lmAllocVar(lm, ref);
		ref->box = box;
		slAddHead(&list2, ref);
		}
	    }
	rBoxJoin(list1, qStart, mid, tStart, tEnd);
	rBoxJoin(list2, mid, qEnd, tStart, tEnd);
	}
    else
        {
	int mid = (tStart + tEnd)>>1;
	for (ref = refList; ref != NULL; ref = next)
	    {
	    struct box *box = ref->box;
	    next = ref->next;
	    if (box->in->tEnd <= mid)
	        {
		slAddHead(&list1, ref);
		}
	    else if (box->in->tStart >= mid)
	        {
		slAddHead(&list2, ref);
		}
	    else
	        {
		/* Box crosses boundary, have to put it on both lists. */
		slAddHead(&list1, ref);
		lmAllocVar(lm, ref);
		ref->box = box;
		slAddHead(&list2, ref);
		}
	    }
	rBoxJoin(list1, qStart, qEnd, tStart, mid);
	rBoxJoin(list2, qStart, qEnd, mid, tEnd);
	}
    }
}


struct boxClump *boxFindClumps(struct boxIn **pBoxList)
/* Convert list of boxes to a list of clumps.  Clumps
 * are collections of boxes that overlap.  Note that
 * the original boxList is overwritten as the boxes
 * are moved from it to the clumps. */
{
struct boxIn *in;
struct boxClump *clumpList = NULL, *clump;
struct boxRef *refList = NULL, *ref;
struct box *box;
struct cluster *cluster;
struct dlNode *node;
struct dlList clusterList;
int qStart = BIGNUM, qEnd = -BIGNUM;
int tStart = BIGNUM, tEnd = -BIGNUM;


dlListInit(&clusterList);
lm = lmInit(0);

/* Make local box structure, cluster, and reference
 * for each input box.  Calculate overall bounds. */
for (in = *pBoxList; in != NULL; in = in->next)
    {
    lmAllocVar(lm, box);
    box->in = in;
    if (in->qStart < qStart) qStart = in->qStart;
    if (in->qEnd > qEnd) qEnd = in->qEnd;
    if (in->tStart < tStart) tStart = in->tStart;
    if (in->tEnd > tEnd) tEnd = in->tEnd;
    lmAllocVar(lm,cluster);
    lmAllocVar(lm, cluster->node);
    cluster->node->val = cluster;
    dlAddTail(&clusterList, cluster->node);
    cluster->boxList = box;
    box->cluster = cluster;
    lmAllocVar(lm, ref);
    ref->box = box;
    slAddHead(&refList, ref);
    }

/* Call to recursive joiner. */
rBoxJoin(refList, qStart, qEnd, tStart, tEnd);

/* Copy from local memory and local data structures
 * to global memory and global data structures. */
for (node = clusterList.head; !dlEnd(node); node = node->next)
    {
    int boxCount = 0;
    int qStart = BIGNUM, qEnd = -BIGNUM;
    int tStart = BIGNUM, tEnd = -BIGNUM;
    struct boxIn *boxList = NULL;
    cluster = node->val;
    for (box = cluster->boxList; box != NULL; box = box->next)
	{
	in = box->in;
	slAddHead(&boxList, in);
	if (in->qStart < qStart) qStart = in->qStart;
	if (in->qEnd > qEnd) qEnd = in->qEnd;
	if (in->tStart < tStart) tStart = in->tStart;
	if (in->tEnd > tEnd) tEnd = in->tEnd;
	++boxCount;
	}
    if (boxCount > 0)
        {
	AllocVar(clump);
	slAddHead(&clumpList, clump);
	clump->boxList = boxList;
	clump->boxCount = boxCount;
	clump->qStart = qStart;
	clump->qEnd = qEnd;
	clump->tStart = tStart;
	clump->tEnd = tEnd;
	}
    }
*pBoxList = NULL;
lmCleanup(&lm);
return clumpList;
}

