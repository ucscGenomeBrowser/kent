/* rbmTree - Red-Black Tree with Merging of overlapping values - 
 * a type of binary tree which automatically keeps itself relatively balanced 
 * during inserts and deletions.
 *   original author: Shane Saunders
 *   adapted into local conventions: Jim Kent
 *   extended to support merging of overlaps: Angie Hinrichs
 */

/* Copyright (C) 2004 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef RBMTREE_H
#define RBMTREE_H

/* Use same color and node structure as regular (not merging) rbTree: */
#include "rbTree.h"

/* Comparison makes an extra distinction: instead of equality, there's 
 * now overlap which is either partial or complete: */
typedef enum 
    {
    RBMT_LESS, 
    RBMT_GREATER, 
    RBMT_COMPLETE, 
    RBMT_PARTIAL
    } rbmTreeCompareResult;

typedef rbmTreeCompareResult rbmTreeCompareFunction(void *a, void *b);
/* Comparison function with distinct return values for complete vs. partial 
 * overlap of values:
 * Return RBMT_LESS if a is strictly less than b, 
 * RBMT_GREATER if a is strictly greater than b,
 * RBMT_COMPLETE if a is completely contained/encompassed by b 
 *   (not nec. vice versa!  mnemonic: b may be bigger),
 * RBMT_PARTIAL if a and b have a partial overlap requiring a merge. */

typedef void rbmTreeItemMergeFunction(void *a, void *b);
/* Item value-merging function for use by rbmTreeAdd.  
 * Change b to encompass both a and b (again, b may be bigger). */

typedef void *rbmTreeItemSubtractFunction(void *a, void *b);
/* Item value-intersection/subtraction function for use by rbmTreeRemove.  
 * Change b to remove any overlap with a.  
 * If this splits b into two parts, return the lesser part and store the 
 * greater part in b; else return NULL. */

typedef void rbmTreeItemFreeFunction(void *a);
/* Item freeing function for use by rbmTreeAdd, to avoid memory leaks 
 * when existing tree nodes are merged into a newly expanded node. 
 * Also used by rbmTreeFreeAll.
 * Note: a is not a pointer to a pointer to item!  It's just a pointer to 
 * item. */

/* Structure type for the red-black merging tree. */
struct rbmTree 
    {
    struct rbTreeNode *root;			/* Root of tree */
    int n;					/* Number of items in tree. */
    struct lm *lm;	                        /* Local memory pool. */
    struct rbTreeNode *freeList;		/* List of nodes to reuse. */
    struct rbTreeNode **stack;                  /* Ancestor stack. */
    rbmTreeCompareFunction *compare;		/* Fancy compare function. */
    rbmTreeItemMergeFunction *itemMerge;	/* Item value merging function 
						 * for rbmTreeAdd(). */
    rbmTreeItemSubtractFunction *itemSubtract;	/* Item value merging function 
						 * for rbmTreeRemove(). */
    rbmTreeItemFreeFunction *itemFree;		/* Item freeing function. */
    };

struct rbmTree *rbmTreeNew(rbmTreeCompareFunction *compare,
			   rbmTreeItemMergeFunction *itemMerge,
			   rbmTreeItemSubtractFunction *itemSubtract,
			   rbmTreeItemFreeFunction *itemFree);
/* Allocates space for a red-black merging tree 
 * and returns a pointer to it.  */

void rbmTreeFree(struct rbmTree **pTree);
/* Frees space used by the red-black tree pointed to by t. */

void rbmTreeFreeAll(struct rbmTree **pTree);
/* Frees space used by the red-black tree pointed to by t and t's items. */

struct rbmTree *rbmTreeNewDetailed(rbmTreeCompareFunction *compare,
				   rbmTreeItemMergeFunction *itemMerge,
				   rbmTreeItemSubtractFunction *itemSubtract,
				   rbmTreeItemFreeFunction *itemFree,
				   struct lm *lm,
				   struct rbTreeNode *stack[256]);
/* Allocate rbmTree on an existing local memory & stack.  This is for cases
 * where you want a lot of trees, and don't want the overhead for each one. 
 * Note, to clean these up, just do freez(&rbmTree) rather than 
 * rbmTreeFree(&rbmTree)! */

void *rbmTreeAdd(struct rbmTree *t, void *item);
/* Inserts an item into the red-black merging tree pointed to by t,
 * according to its value compared to other items in t.  
 * If item is completely contained by a current item in t, a pointer to that 
 * item is returned.  
 * If item partially overlaps with a current item in t, the two items are 
 * merged (and other items in the tree might also be merged in if the new 
 * composite item encompasses other existing items).  A pointer to the new 
 * merged item is returned.  
 * Otherwise, a new node for item is added to the tree and NULL is returned,
 * indicating a non-merging addition. */

void *rbmTreeFindComplete(struct rbmTree *t, void *item);
/* Find an item in the red-black tree whose value encompasses the given item.
 * Return that item, or return NULL if a partial overlap or no overlap 
 * was found. */

void *rbmTreeRemove(struct rbmTree *t, void *item);
/* UNIMPLEMENTED! */
/* Delete *any* node from t whose value is completely contained by
 * item, and subtract out any partial overlap with item from existing
 * values in a tree.  Returns an item containing the values removed 
 * from the tree. */
/* UNIMPLEMENTED! */

void rbmTreeTraverseRange(struct rbmTree *tree, void *minItem, void *maxItem,
	void (*doItem)(void *item));
/* Apply doItem function to all items in tree such that
 * minItem <= item <= maxItem */

void rbmTreeTraverse(struct rbmTree *tree, void (*doItem)(void *item));
/* Apply doItem function to all items in tree. */

struct slRef *rbmTreeItemsInRange(struct rbmTree *tree, void *minItem, void *maxItem);
/* Return a sorted list of references to items in tree between range.
 * slFree this list when done. */

void rbmTreeTraverse(struct rbmTree *tree, void (*doItem)(void *item));
/* Apply doItem function to all items in tree */

struct slRef *rbmTreeItems(struct rbmTree *tree);
/* Return sorted list of items. */

void rbmTreeDump(struct rbmTree *tree, FILE *f, 
	void (*dumpItem)(void *item, FILE *f));
/* Dump out rb tree to file, mostly for debugging. */

#endif /* RBMTREE_H */

