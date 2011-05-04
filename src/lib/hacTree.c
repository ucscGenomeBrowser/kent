/* hacTree - Hierarchical Agglomerative Clustering a list of inputs into a binary tree */

#include "common.h"
#include "hacTree.h"

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
		     hacDistanceFunction *distF, hacMergeFunction *mergeF, void *extraData)
/* Initialize node to have left and right as its children.  Leave parent pointers
 * alone -- they would be unstable during tree construction. */
{
node->left = (struct hacTree *)left;
node->right = (struct hacTree *)right;
node->childDistance = distF(left->itemOrCluster, right->itemOrCluster, extraData);
node->itemOrCluster = mergeF(left->itemOrCluster, right->itemOrCluster, extraData);
}

INLINE struct hacTree preClusterNodes(const struct sortWrapper *leafWraps, int i, int runLength,
				      hacDistanceFunction *distF, hacMergeFunction *mergeF,
				      void *extraData, struct lm *localMem)
/* Caller has allocated a node, and this returns what to store there:
 * a recursively constructed cluster of nodes extracted from wrapped
 * leafNodes (leafWraps) starting at i, for runLength items. */
{
struct hacTree ret;
if (runLength > 2)
    {
    struct hacTree *newClusters = lmAlloc(localMem, 2 * sizeof(struct hacTree));
    int halfLength = runLength/2;
    newClusters[0] = preClusterNodes(leafWraps, i, halfLength,
				     distF, mergeF, extraData, localMem);
    newClusters[1] = preClusterNodes(leafWraps, i+halfLength, runLength-halfLength,
				     distF, mergeF, extraData, localMem);
    initNode(&ret, &(newClusters[0]), &(newClusters[1]), distF, mergeF, extraData);
    }
else if (runLength == 2)
    {
    initNode(&ret, leafWraps[i].node, leafWraps[i+1].node, distF, mergeF, extraData);
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
int pairCount = itemCount * (itemCount-1) / 2;
struct hacTree *pairHeap = lmAlloc(localMem, pairCount * sizeof(struct hacTree));
int i, j, pairIx;
for (i=0, pairIx=0;  i < itemCount-1;  i++)
    for (j=i+1;  j < itemCount;  j++, pairIx++)
	initNode(&(pairHeap[pairIx]), &(leafNodes[i]), &(leafNodes[j]), distF, mergeF, extraData);
*retPairCount = pairCount;
return pairHeap;
}

struct hacTree *hacTreeFromItems(const struct slList *itemList, struct lm *localMem,
				 hacDistanceFunction *distF, hacMergeFunction *mergeF,
				 hacCmpFunction *cmpF, void *extraData)
/* Using distF, mergeF, optionally cmpF and binary tree operations,
 * perform a hierarchical agglomerative (bottom-up) clustering of
 * items.  To free the resulting tree, lmCleanup(&localMem). */
{
if (itemList == NULL)
    return NULL;
struct hacTree *root = NULL;
int itemCount = slCount(itemList);
int pairCount = 0;
struct hacTree *leafPairs = pairUpItems(itemList, itemCount, &pairCount, localMem,
					distF, mergeF, cmpF, extraData);
struct hacTree *heapHead = leafPairs;
int heapLength = pairCount;
while (heapLength > 0)
    {
    // Scan heap for node with lowest childDistance; swap that node w/head
    int i;
    int bestIx = 0;
    double minScore = heapHead[0].childDistance;
    for (i=1;  i < heapLength;  i++)
	if (heapHead[i].childDistance < minScore)
	    {
	    minScore = heapHead[i].childDistance;
	    bestIx = i;
	    }
    if (bestIx != 0)
	swapBytes((char *)&(heapHead[0]), (char *)&(heapHead[bestIx]), sizeof(struct hacTree));
    // Pop the best (lowest-distance) node from heapHead, make it root (for now).
    root = heapHead;
    heapHead = &(heapHead[1]);
    heapLength--;
    // Where root->left is found in the heap, replace it with root.
    // Where root->right is found, drop that node so it doesn't become
    // a duplicate of the replacement cases.
    for (i=0;  i < heapLength;  i++)
	{
	struct hacTree *node = &(heapHead[i]);
	if (node->left == root->left)
	    initNode(node, root, node->right, distF, mergeF, extraData);
	else if (node->right == root->left)
	    initNode(node, root, node->left, distF, mergeF, extraData);
	else if (node->left == root->right || node->right == root->right)
	    {
	    if (i < heapLength-1)
		memcpy(node, &(heapHead[i+1]), (heapLength-i-1)*sizeof(struct hacTree));
	    heapLength--;
	    i--;
	    }
	}
    // root now has a stable address, unlike nodes still in the heap, so set parents here:
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
