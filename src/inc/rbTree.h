/* rbTree - rbTreeRed-rbTreeBlack Tree - a type of binary tree which 
 * automatically keeps relatively balanced during
 * inserts and deletions.
 *   original author: Shane Saunders
 *   adapted into local conventions: Jim Kent
 */
#ifndef RBTREE_H
#define RBTREE_H

typedef enum {rbTreeRed,rbTreeBlack} rbTreeColor;


/* Structure type for nodes in the red-black tree. */
struct rbTreeNode 
    {
    struct rbTreeNode *left, *right;		/* Children. */
    rbTreeColor color;				/* Heart of algorithm. */
    void *item;					/* Item stored in tree */
    };

/* Structure type for the red-black tree. */
struct rbTree 
    {
    struct rbTreeNode *root;			/* Root of tree */
    int n;					/* Number of items in tree. */
    int (* compare)(void *, void *);/* Comparison function */
    struct rbTreeNode **stack;                  /* Ancestor stack. */
    struct lm *lm;	                        /* Local memory pool. */
    struct rbTreeNode *freeList;		/* List of nodes to reuse. */
    };

struct rbTree *rbTreeNew(int (*compare)(void *, void *));
/* Allocates space for a red-black tree and returns a pointer
 * to it.  The function compare compares they keys of two items, and returns a
 * negative, zero, or positive integer depending on whether the first item is
 * less than, equal to, or greater than the second. */

void rbTreeFree(struct rbTree **pTree);
/* Frees space used by the red-black tree pointed to by t. */

void *rbTreeAdd(struct rbTree *t, void *item);
/* Inserts an item into the red-black tree pointed to by t,
 * according the the value its key.  The key of an item in the red-black
 * tree must be unique among items in the tree.  If an item with the same key
 * already exists in the tree, a pointer to that item is returned.  Otherwise,
 * NULL is returned, indicating insertion was successful.
 */

void *rbTreeFind(struct rbTree *t, void *item);
/* Find an item in the red-black tree with the same key as the
 * item pointed to by `item'.  Returns a pointer to the item found, or NULL
 * if no item was found.
 */

void *rbTreeRemove(struct rbTree *t, void *item);
/* rbTreeRemove() - Delete an item in the red-black tree with the same key as
 * the item pointed to by `item'.  Returns a pointer to the  deleted item,
 * and NULL if no item was found.
 */

void rbTreeTraverseRange(struct rbTree *tree, void *minItem, void *maxItem,
	void (*doItem)(void *item));
/* Apply doItem function to all items in tree such that
 * minItem <= item <= maxItem */

struct slRef *rbTreeItemsInRange(struct rbTree *tree, void *minItem, void *maxItem);
/* Return a sorted list of references to items in tree between range.
 * slFree this list when done. */

void rbTreeDump(struct rbTree *tree, FILE *f, 
	void (*dumpItem)(void *item, FILE *f));
/* Dump out rb tree to file, mostly for debugging. */

#endif /* RBTREE_H */

