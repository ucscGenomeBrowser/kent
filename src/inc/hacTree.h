/* hacTree - Hierarchical Agglomerative Clustering a list of inputs into a binary tree */
/*
 * This clustering algorithm can be used for problems with high
 * dimensionality, such as clustering sequences by similarity.  It can
 * be used for problems with low dimensionality too, such as
 * clustering a sequence of numbers (1-dimensional: points along the
 * number line), but there are more efficient algorithms for problems
 * with low dimensionality.
 *
 * In order to use this, you need to provide functions that operate on a
 * pair of input items (and extra data pointer):
 * 1. a distance function that takes two input items and returns a number,
 *    small for short distances/high similarity and large for long
 *    distances/low similarity
 * 2. a merging function that takes two input items and returns a new item
 *    that represents both of them.  Therefore the data structure that
 *    represents an input item must also be capable of representing a
 *    cluster of input items.  Usually this is done by combining values
 *    from input items into a new value that will compare properly with
 *    other items or clusters in the distance function.
 *
 * Then pass in an slList of items, pointers to those two functions, and
 * a pointer to data used in your distance and merging functions (or NULL).
 * You get back a pointer to the root node of a binary tree structure
 * where each leaf node contains one of your items and each node above
 * that contains a cluster.  The root node contains the cluster of all items.
 *
 * If a significant proportion of input items are identical to each other,
 * you can also pass in a comparison function that will be used to sort the
 * items before clustering.  After sorting, adjacent identical items will
 * be pre-clustered in order to reduce the number of inputs to the main
 * clustering step.
 *
 * kent/src/lib/tests/hacTreeTest.c contains a couple simple examples.
 *
 * To get the top k clusters, perform a breadth-first,
 * lowest-distance-child-first search of the tree to find the top k
 * nodes/clusters with the smallest distances from the root node/cluster.
 * [It would be nice for this module to provide that function, if/when a need for it arises.]
 */
#ifndef HACTREE_H
#define HACTREE_H

#include "localmem.h"

/* Caller-provided function to measure distance between two items or clusters */
/* Note: this must be able to handle identical inputs or NULL inputs */
/* Note: this should be monotonic and nonnegative, *not* a + or -
 * difference (as would be used for comparing/ordering/sorting) */
typedef double hacDistanceFunction(const struct slList *item1, const struct slList *item2,
				   void *extraData);

/* Caller-provided function to merge two items or clusters into a new cluster */
/* Note: this must be able to handle NULL inputs */
typedef struct slList *(hacMergeFunction)(const struct slList *item1, const struct slList *item2,
					  void *extraData);

/* Optional caller-provided function to compare two children and determine first born. */ 
typedef bool hacSibCmpFunction(const struct slList *item1, const struct slList *item2,
					  void *extraData);

/* Optional caller-provided function to compare two items or clusters for pre-sorting
 * and pre-clustering of identical items. */
typedef int hacCmpFunction(const struct slList *item1, const struct slList *item2,
			   void *extraData);

/* Structure of nodes of the binary tree that contains a hierarchical clustering of inputs */
struct hacTree
    {
    struct hacTree *next;       // Can have an unordered list of these
    struct hacTree *parent;     // Cluster that contains this cluster, NULL for root node
    struct hacTree *left;       // Left child, NULL for leaf node
    struct hacTree *right;      // Right child, NULL for leaf node
    double childDistance;       // Result of distance function on left and right kids
    struct slList *itemOrCluster;  // If leaf node, one of the items passed in;
                                // otherwise, result of merging left and right kids' itemOrClusters
    };

struct hacTree *hacTreeFromItems(const struct slList *itemList, struct lm *localMem,
				 hacDistanceFunction *distF, hacMergeFunction *mergeF,
				 hacCmpFunction *cmpF, void *extraData);
/* Using distF, mergeF, optionally cmpF and binary tree operations,
 * perform a hierarchical agglomerative (bottom-up) clustering of
 * items.  To free the resulting tree, lmCleanup(&localMem). */

struct hacTree *hacTreeMultiThread(int threadCount, struct slList *itemList, struct lm *localMem,
				 hacDistanceFunction *distF, hacMergeFunction *mergeF,
				 hacSibCmpFunction *cmpF, 
				 void *extraData, struct hash *precalcDistanceHash);
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
 *	extraData - parameter passed through to distF and mergeF, otherwise unused, may be NULL
 *	precalcDistanceHash - a hash containing at least some of the pairwise distances
 *	            between items on itemList, set with hacTreeDistanceHashAdd. 
 *	            As a side effect this hash will be expanded to include all distances 
 *	            including those between intermediate nodes.  May be NULL. */

void hacTreeDistanceHashAdd(struct hash *hash, void *itemA, void *itemB, double distance);
/* Add an item to distance hash */

double * hacTreeDistanceHashLookup(struct hash *hash, void *itemA, void *itemB);
/* Look up pair in distance hash.  Returns NULL if not found, otherwise pointer to
 * distance */

#endif//def HACTREE_H
