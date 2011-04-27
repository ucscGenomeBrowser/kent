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

static void initNode(struct hacTree *node, struct hacTree *left, struct hacTree *right,
		     hacDistanceFunction *distF, hacMergeFunction *mergeF, void *extraData)
/* Initialize node to have left and right as its children.  Leave parent pointers
 * alone -- they would be unstable during tree construction. */
{
node->left = left;
node->right = right;
node->childDistance = distF(left->itemOrCluster, right->itemOrCluster, extraData);
node->itemOrCluster = mergeF(left->itemOrCluster, right->itemOrCluster, extraData);
}

static struct hacTree *pairUpItems(const struct slList *itemList, int itemCount,
				   struct lm *localMem,
				   hacDistanceFunction *distF, hacMergeFunction *mergeF,
				   void *extraData)
/* Allocate & initialize leaf nodes and all possible pairings of leaf nodes
 * which will be our seed clusters. */
{
struct hacTree *leafNodes = leafNodesFromItems(itemList, itemCount, localMem);
int pairCount = itemCount * (itemCount-1) / 2;
struct hacTree *pairHeap = lmAlloc(localMem, pairCount * sizeof(struct hacTree));
int i, j, pairIx;
for (i=0, pairIx=0;  i < itemCount-1;  i++)
    for (j=i+1;  j < itemCount;  j++, pairIx++)
	initNode(&(pairHeap[pairIx]), &(leafNodes[i]), &(leafNodes[j]), distF, mergeF, extraData);
return pairHeap;
}

struct hacTree *hacTreeFromItems(const struct slList *itemList, struct lm *localMem,
				 hacDistanceFunction *distF, hacMergeFunction *mergeF,
				 void *extraData)
/* Using distF, mergeF, and binary tree operations, perform a
 * hierarchical agglomerative (bottom-up) clustering of items.
 * To free the resulting tree, lmCleanup(&localMem). */
{
if (itemList == NULL)
    return NULL;
struct hacTree *root = NULL;
int itemCount = slCount(itemList);
struct hacTree *leafPairs = pairUpItems(itemList, itemCount, localMem, distF, mergeF, extraData);
struct hacTree *heapHead = leafPairs;
int heapLength = itemCount * (itemCount-1) / 2;
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
