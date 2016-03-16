/* hacTree - Hierarchical Agglomerative Clustering a list of inputs into a binary tree */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dlist.h"
#include "hash.h"
#include "hacTree.h"
#include "synQueue.h"
#include "pthreadDoList.h"

static struct hacTree *leafNodesFromItems(const struct slList *itemList, int itemCount,
					  struct lm *localMem)
/* Allocate & initialize leaf nodes that contain only items. */
{
struct hacTree *leafNodes = lmAlloc(localMem, itemCount * sizeof(struct hacTree));
int i = 0;
const struct slList *item = itemList;
while (item != NULL && i < itemCount)
    {
    // needMem zeroes the memory, so initialize only non-NULL stuff.
    struct hacTree *node = &(leafNodes[i]);
    if (i < itemCount-1)
	node->next = &(leafNodes[i+1]);
    node->itemOrCluster = (struct slList *)item;
    i++;
    item = item->next;
    }
return leafNodes;
}

struct sortWrapper
/* We need to compare nodes' itemOrClusters using cmpF and extraData;
 * qsort's comparison function doesn't have a way to pass in extraData,
 * so we need to point to it from each qsort element. */
{
    struct hacTree *node;  // contains itemOrCluster to be compared
    hacCmpFunction *cmpF;  // user-provided itemOrCluster comparison function
    void *extraData;       // user-provided aux data for cmpF
};

static int sortWrapCmp(const void *v1, const void *v2)
/* Unpack sortWrappers and run cmpF on nodes' itemOrClusters with extraData. */
{
const struct sortWrapper *w1 = v1, *w2 = v2;
return w1->cmpF(w1->node->itemOrCluster, w2->node->itemOrCluster, w1->extraData);
}

static struct sortWrapper *makeSortedWraps(struct hacTree *leafNodes, int itemCount,
					   struct lm *localMem, hacCmpFunction cmpF,
					   void *extraData)
/* Use cmpF and extraData to sort wrapped leaves so that identical leaves will be adjacent. */
{
struct sortWrapper *leafWraps = lmAlloc(localMem, itemCount * sizeof(struct sortWrapper));
int i;
for (i=0;  i < itemCount;  i++)
    {
    leafWraps[i].node = &(leafNodes[i]);
    leafWraps[i].cmpF = cmpF;
    leafWraps[i].extraData = extraData;
    }
qsort(leafWraps, itemCount, sizeof(struct sortWrapper), sortWrapCmp);
return leafWraps;
}

INLINE void initNode(struct hacTree *node, const struct hacTree *left, const struct hacTree *right,
		     hacDistanceFunction *distF, hacMergeFunction *mergeF, void *extraData, bool distOnly)
/* Initialize node to have left and right as its children.  Leave parent pointers
 * alone -- they would be unstable during tree construction. */
{
node->left = (struct hacTree *)left;
node->right = (struct hacTree *)right;
if (left != NULL && right != NULL)
    {
    if (distOnly)
        {
	struct slList * el;
	AllocVar(el);
        node->childDistance = distF(left->itemOrCluster, right->itemOrCluster, extraData);
        node->itemOrCluster = el;
        }
    else {
        node->childDistance = distF(left->itemOrCluster, right->itemOrCluster, extraData);
        node->itemOrCluster = mergeF(left->itemOrCluster, right->itemOrCluster, extraData);
        }
    }
}

INLINE struct hacTree preClusterNodes(const struct sortWrapper *leafWraps, int i, int runLength,
				      hacDistanceFunction *distF, hacMergeFunction *mergeF,
				      void *extraData, struct lm *localMem)
/* Caller has allocated a node, and this returns what to store there:
 * a recursively constructed cluster of nodes extracted from wrapped
 * leafNodes (leafWraps) starting at i, for runLength items. */
{
struct hacTree ret = {NULL, NULL, NULL, NULL, 0, NULL};
if (runLength > 2)
    {
    struct hacTree *newClusters = lmAlloc(localMem, 2 * sizeof(struct hacTree));
    int halfLength = runLength/2;
    newClusters[0] = preClusterNodes(leafWraps, i, halfLength,
				     distF, mergeF, extraData, localMem);
    newClusters[1] = preClusterNodes(leafWraps, i+halfLength, runLength-halfLength,
				     distF, mergeF, extraData, localMem);
    initNode(&ret, &(newClusters[0]), &(newClusters[1]), distF, mergeF, extraData, FALSE);
    }
else if (runLength == 2)
    {
    initNode(&ret, leafWraps[i].node, leafWraps[i+1].node, distF, mergeF, extraData, FALSE);
    }
else
    ret = *(leafWraps[i].node);
return ret;
}

static struct hacTree *sortAndPreCluster(struct hacTree *leafNodes, int *retItemCount,
					 struct lm *localMem, hacDistanceFunction *distF,
					 hacMergeFunction *mergeF, hacCmpFunction *cmpF,
					 void *extraData)
/* Use cmpF and extraData to sort wrapped leaf nodes so that identical leaves will be adjacent,
 * then replace leaves with clusters of identical leaves where possible.  Place new
 * (hopefully smaller) item count in retItemCount. */
{
int itemCount = *retItemCount;
struct sortWrapper *leafWraps = makeSortedWraps(leafNodes, itemCount, localMem, cmpF, extraData);
struct hacTree *newLeaves = lmAlloc(localMem, itemCount * sizeof(struct hacTree));
int i=0, newI=0;
while (i < itemCount)
    {
    int nextRunStart;
    for (nextRunStart = i+1;  nextRunStart < itemCount; nextRunStart++)
	if (distF(leafWraps[i].node->itemOrCluster, leafWraps[nextRunStart].node->itemOrCluster,
		  extraData) != 0)
	    break;
    int runLength = nextRunStart - i;
    newLeaves[newI] = preClusterNodes(leafWraps, i, runLength, distF, mergeF, extraData, localMem);
    i = nextRunStart;
    newI++;
    }
*retItemCount = newI;
return newLeaves;
}

static struct hacTree *pairUpItems(const struct slList *itemList, int itemCount,
				   int *retPairCount, struct lm *localMem,
				   hacDistanceFunction *distF, hacMergeFunction *mergeF,
				   hacCmpFunction *cmpF, void *extraData)
/* Allocate & initialize leaf nodes and all possible pairings of leaf nodes
 * which will be our seed clusters.  If cmpF is given, pre-sort the leaf nodes
 * and pre-cluster identical leaves before generating seed clusters. */
{
struct hacTree *leafNodes = leafNodesFromItems(itemList, itemCount, localMem);
if (cmpF != NULL)
    leafNodes = sortAndPreCluster(leafNodes, &itemCount, localMem,
				  distF, mergeF, cmpF, extraData);
int pairCount = (itemCount == 1) ? 1 : (itemCount * (itemCount-1) / 2);
struct hacTree *pairPool = lmAlloc(localMem, pairCount * sizeof(struct hacTree));
if (itemCount == 1)
    initNode(pairPool, leafNodes, NULL, distF, mergeF, extraData, FALSE);
else
    {
    int i, j, pairIx;
    for (i=0, pairIx=0;  i < itemCount-1;  i++)
	for (j=i+1;  j < itemCount;  j++, pairIx++)
	    initNode(&(pairPool[pairIx]), &(leafNodes[i]), &(leafNodes[j]), distF, mergeF,
		     extraData, TRUE);
    }
*retPairCount = pairCount;
return pairPool;
}

struct hacTree *hacTreeFromItems(const struct slList *itemList, struct lm *localMem,
				 hacDistanceFunction *distF, hacMergeFunction *mergeF,
				 hacCmpFunction *cmpF, void *extraData)
/* Using distF, mergeF, optionally cmpF and binary tree operations,
 * perform a hierarchical agglomerative (bottom-up) clustering of
 * items.  To free the resulting tree, lmCleanup(&localMem). */
//
// Implementation:
//
// Create a pool containing all pairs of items (N*(N-1)/2), and build
// a hierarchical binary tree of items from the bottom up.  In each
// iteration, first we find the closest pair and swap it into the head
// of the pool; then we advance the head pointer, so the closest pair
// now has a stable location in memory.  Next, for all pairs still in
// the pool, we replace references to the elements of the closest pair
// with the closest pair itself, but delete half of such pairs because
// they would be duplicates.  Specifically, we keep pairs that had the
// left element of the closest pair, and delete pairs that had the
// right element of the closest pair.  We rescore the pairs that have
// the closest pair swapped in for an element.  The code to do all
// this is surprisingly simple -- in the second for loop below.  Note
// that with each iteration, the pool will reduce in size, by N-2 the
// first iteration, N-3 the second, and so forth.
//
// An example may help: say we start with items A, B, C and D.  Initially
// the pool contains all pairs:
//    (A, B)   (A, C)   (A, D)   (B, C)   (B, D)   (C, D)
//
// If (A, B) is the closest pair, we pop it from the pool and the pool
// becomes
//    (A, C)   (A, D)   (B, C)   (B, D)   (C, D)
//
// Now we substitute (A, B) for pool pairs containing A, and delete pool
// pairs contining B because they would be duplicates of those containing
// A.  [X] shows where a pair was deleted:
//
//    ((A, B), C)  ((A, B), D)  [X]   [X]  (C, D)
//
// Now say ((A, B), D) is the closest remaining pair, and is popped from
// the head of the pool.  We substitute into pairs containing (A, B) and
// delete pairs containing D.  After the replacement step, the pool is
// down to a single element:
//
//    (((A, B), D), C)   [X]
{
if (itemList == NULL)
    return NULL;
struct hacTree *root = NULL;
int itemCount = slCount(itemList);
int pairCount = 0;
struct hacTree *leafPairs = pairUpItems(itemList, itemCount, &pairCount, localMem,
					distF, mergeF, cmpF, extraData);
int *nodesToDelete = needMem(pairCount * sizeof(int));
struct hacTree *poolHead = leafPairs;
int poolLength = pairCount;
while (poolLength > 0)
    {
    // Scan pool for node with lowest childDistance; swap that node w/head
    int bestIx = 0;
    double minScore = poolHead[0].childDistance;
    int i;
    for (i=1;  i < poolLength;  i++)
	if (poolHead[i].childDistance < minScore)
	    {
	    minScore = poolHead[i].childDistance;
	    bestIx = i;
	    }
    if (bestIx != 0)
	swapBytes((char *)&(poolHead[0]), (char *)&(poolHead[bestIx]), sizeof(struct hacTree));
    // Pop the best (lowest-distance) node from poolHead, make it root (for now).
    root = poolHead;
    if (root->left && root->right)
        root->itemOrCluster = mergeF(root->left->itemOrCluster, root->right->itemOrCluster,
                                     extraData);
    else if (root->left)
        root->itemOrCluster = root->left->itemOrCluster;
    else if (root->right)
        root->itemOrCluster = root->right->itemOrCluster;
    poolHead = &(poolHead[1]);
    poolLength--;
    // Where root->left is found in the pool, replace it with root.
    // Where root->right is found, drop that node so it doesn't become
    // a duplicate of the replacement cases.
    int numNodesToDelete = 0;
    for (i=0;  i < poolLength;  i++)
	{
	struct hacTree *node = &(poolHead[i]);
	if (node->left == root->left)
	    // found root->left; replace node->left with root (merge root with node->right):
	    initNode(node, root, node->right, distF, mergeF, extraData, FALSE);
	else if (node->right == root->left)
	    // found root->left; replace node->right with root (merge root with node->left):
	    initNode(node, node->left, root, distF, mergeF, extraData, FALSE);
	else if (node->left == root->right || node->right == root->right)
	    // found root->right; mark this node for deletion:
	    nodesToDelete[numNodesToDelete++] = i;
	}
    if (numNodesToDelete > 0)
	{
	int newPoolLen = nodesToDelete[0];
	// This will be "next node to delete" for the last marked node:
	nodesToDelete[numNodesToDelete] = poolLength;
	for (i = 0;  i < numNodesToDelete;  i++)
	    {
	    int nodeToDel = nodesToDelete[i];
	    int nextNodeToDel = nodesToDelete[i+1];
	    int blkSize = nextNodeToDel - (nodeToDel+1);
	    if (blkSize == 0)
		continue;
	    struct hacTree *fromNode = &(poolHead[nodeToDel+1]);
	    struct hacTree *toNode = &(poolHead[newPoolLen]);
	    memmove(toNode, fromNode, blkSize * sizeof(struct hacTree));
	    newPoolLen += blkSize;
	    }
	poolLength = newPoolLen;
	}
    // root now has a stable address, unlike nodes still in the pool, so set parents here:
    if (root->left != NULL)
	root->left->parent = root;
    if (root->right != NULL)
	root->right->parent = root;
    }
// This shouldn't be necessary as long as initNode leaves parent pointers alone,
// but just in case that changes:
root->parent = NULL;
return root;
}

/** The code from here on down is an alternative implementation that calls the merge
 ** function and for that matter the distance function much less than the function
 ** above, and also is multithreaded.  It does seem to produce the same output
 ** but the algorithm is sufficiently different, at least for now, I'm keeping
 ** both. */


#define distKeySize 64  

static void calcDistKey(void *a, void *b, char key[distKeySize])
/* Put key for distance in key */
{
safef(key, distKeySize, "%p %p", a, b);
}

void hacTreeDistanceHashAdd(struct hash *hash, void *itemA, void *itemB, double distance)
/* Add an item to distance hash */
{
char key[distKeySize];
calcDistKey(itemA, itemB, key);
hashAdd(hash, key, CloneVar(&distance));
}

double *hacTreeDistanceHashLookup(struct hash *hash, void *itemA, void *itemB)
/* Look up pair in distance hash.  Returns NULL if not found, otherwise pointer to
 * distance */
{
char key[distKeySize];
calcDistKey(itemA, itemB, key);
return hashFindVal(hash, key);
}

struct hctPair
/* A pair of hacTree nodes and the distance between them */
    {
    struct hctPair *next;  /* Next in list */
    struct hacTree *a, *b;	   /* The pair of nodes */
    double distance;	   /* Distance between pair */
    };

struct distanceWorkerContext
/* Context for a distance worker */
    {
    hacDistanceFunction *distF;	/* Function to call to calc distance */
    void *extraData;		/* Extra data for that function */
    };

static void distanceWorker(void *item, void *context)
/* fill in distance for pair */
{
struct hctPair *pair = item;
struct distanceWorkerContext *dc = context;
struct hacTree *aHt = pair->a, *bHt = pair->b;
pair->distance = (dc->distF)((struct slList *)(aHt->itemOrCluster), 
    (struct slList *)(bHt->itemOrCluster), dc->extraData);
}

static void precacheMissingDistances(struct dlList *list, struct hash *distanceHash,
    hacDistanceFunction *distF, void *extraData, int threadCount)
/* Force computation of all distances not already in hash */
{
/* Make up list of all missing distances in pairList */
struct synQueue *sq = synQueueNew();
struct hctPair *pairList = NULL, *pair;
struct dlNode *aNode;
for (aNode = list->head; !dlEnd(aNode); aNode = aNode->next)
    {
    struct hacTree *aHt = aNode->val;
    struct dlNode *bNode;
    for (bNode = aNode->next; !dlEnd(bNode); bNode = bNode->next)
        {
	struct hacTree *bHt = bNode->val;
	char key[64];
	calcDistKey(aHt->itemOrCluster, bHt->itemOrCluster, key);
	double *pd = hashFindVal(distanceHash, key);
	if (pd == NULL)
	     {
	     AllocVar(pair);
	     pair->a = aHt;
	     pair->b = bHt;
	     slAddHead(&pairList, pair);
	     synQueuePut(sq, pair);
	     }
	}
    }

/* Parallelize distance calculations */
struct distanceWorkerContext context = {.distF=distF, .extraData=extraData};
pthreadDoList(threadCount, pairList, distanceWorker, &context);

for (pair = pairList; pair != NULL; pair = pair->next)
    {
    hacTreeDistanceHashAdd(distanceHash, 
	pair->a->itemOrCluster, pair->b->itemOrCluster, pair->distance);
    }
slFreeList(&pairList);
}

static double pairParallel(struct dlList *list, struct hash *distanceHash, 
    hacDistanceFunction *distF, void *extraData, int threadCount,
    struct dlNode **retNodeA, struct dlNode **retNodeB)
/* Loop through list returning closest two nodes */
{
precacheMissingDistances(list, distanceHash, distF, extraData, threadCount);
struct dlNode *aNode;
double closestDistance = BIGDOUBLE;
struct dlNode *closestA = NULL, *closestB = NULL;
for (aNode = list->head; !dlEnd(aNode); aNode = aNode->next)
    {
    struct hacTree *aHt = aNode->val;
    struct dlNode *bNode;
    for (bNode = aNode->next; !dlEnd(bNode); bNode = bNode->next)
        {
	struct hacTree *bHt = bNode->val;
	char key[64];
	safef(key, sizeof(key), "%p %p", aHt->itemOrCluster, bHt->itemOrCluster);
	double *pd = hashMustFindVal(distanceHash, key);
	double d = *pd;
	if (d < closestDistance)
	    {
	    closestDistance = d;
	    closestA = aNode;
	    closestB = bNode;
	    }
	}
    }
*retNodeA = closestA;
*retNodeB = closestB;
return closestDistance;
}

static void lmDlAddValTail(struct lm *lm, struct dlList *list, void *val)
/* Allocate new dlNode out of lm, initialize it with val, and add it to end of list */
{
struct dlNode *node;
lmAllocVar(lm, node);
node->val = val;
dlAddTail(list, node);
}

struct hacTree *hacTreeMultiThread(int threadCount, struct slList *itemList, 
		    struct lm *localMem, hacDistanceFunction *distF, 
		    hacMergeFunction *mergeF, hacSibCmpFunction *cmpF,
		    void *extraData, struct hash *precalcDistanceHash)
/* Construct hacTree minimizing number of merges called, and doing distance calls
 * in parallel when possible.   Do a lmCleanup(localMem) to free returned tree. 
 * The inputs are
 *	threadCount - number of threads - at least one, recommended no more than 15
 *	itemList - list of items to tree up.  Format can vary, but must start with a
 *	           pointer to next item in list.
 *	localMem - memory pool where hacTree and a few other things are allocated from
 *	distF - function that calculates distance between two items, passed items and extraData
 *	mergeF - function that creates a new item in same format as itemList from two passed
 *	         in items and the extraData.  Typically does average of two input items
 *	cmpF - function that compares two items being merged to decide who becomes left child
 *		and who is the right child. 
 *	extraData - parameter passed through to distF and mergeF, otherwise unused, may be NULL
 *	precalcDistanceHash - a hash containing at least some of the pairwise distances
 *	            between items on itemList, set with hacTreeDistanceHashAdd. 
 *	            As a side effect this hash will be expanded to include all distances 
 *	            including those between intermediate nodes. */
{
/* Make up a doubly-linked list in 'remaining' with all items in it */
struct dlList remaining;
dlListInit(&remaining);
struct slList *item;
int count = 0;
struct hash *distanceHash = (precalcDistanceHash ? precalcDistanceHash : hashNew(0));
for (item = itemList; item != NULL; item = item->next)
    {
    struct hacTree *ht;
    lmAllocVar(localMem, ht);
    ht->itemOrCluster = item;
    lmDlAddValTail(localMem, &remaining, ht);
    count += 1;
    }

/* Loop through finding closest and merging until only one node left on remaining. */
int i;
for (i=1; i<count; ++i)
    {
    /* Find closest pair */
    struct dlNode *aNode, *bNode;
    double distance = pairParallel(&remaining, distanceHash, distF, extraData, threadCount,
	&aNode, &bNode);

    /* Allocated new hacTree item for them and fill it in with a merged value. */
    struct hacTree *ht, *left, *right;
    lmAllocVar(localMem, ht);
    
    /* Check if the user provided a function to determine sibling perference */ 
    
    if (cmpF != NULL)
	{
	if (cmpF(((struct hacTree *)aNode->val)->itemOrCluster, ((struct hacTree *)bNode->val)->itemOrCluster, extraData))
	    {
	    left = ht->left = aNode->val;
	    right = ht->right = bNode->val;
	    }
	else{
	    left = ht->left = bNode->val;
	    right = ht->right = aNode->val;
	    }
	}
    else{
	left = ht->left = aNode->val;
	right = ht->right = bNode->val;
	}
    left->parent = right->parent = ht;
    ht->itemOrCluster = mergeF(left->itemOrCluster, right->itemOrCluster, extraData);
    ht->childDistance = distance;

    /* Put merged item onto remaining list before first matching node in pair. */
    struct dlNode *mergedNode;
    lmAllocVar(localMem, mergedNode);
    mergedNode->val = ht;
    dlAddBefore(aNode, mergedNode);

    /* Remove nodes we merged out */
    dlRemove(aNode);
    dlRemove(bNode);
    }

/* Clean up and go home. */
if (distanceHash != precalcDistanceHash)
    hashFree(&distanceHash);
struct dlNode *lastNode = dlPopHead(&remaining);
return lastNode->val;
}

