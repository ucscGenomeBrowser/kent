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
    struct rbTree *next;			/* Next tree in list. */
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

void rbTreeFreeList(struct rbTree **pList);
/* Free up a list of rbTrees. */

struct rbTree *rbTreeNewDetailed(int (*compare)(void *, void *), struct lm *lm, 
	struct rbTreeNode *stack[128]);
/* Allocate rbTree on an existing local memory & stack.  This is for cases
 * where you want a lot of trees, and don't want the overhead for each one. 
 * Note, to clean these up, just do freez(&rbTree) rather than rbFreeTree(&rbTree). */

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
 * slFreeList this list when done. */

void rbTreeTraverse(struct rbTree *tree, void (*doItem)(void *item));
/* Apply doItem function to all items in tree */

void rbTreeTraverseWithContext(struct rbTree *tree, 
	void (*doItem)(void *item, void *context), void *context);
/* Traverse tree calling doItem on every item with context pointer passed through to doItem.
 * This often avoids having to declare global or static variables for the doItem callback to use. */

struct slRef *rbTreeItems(struct rbTree *tree);
/* Return sorted list of items.  slFreeList this when done.*/

void rbTreeDump(struct rbTree *tree, FILE *f, 
	void (*dumpItem)(void *item, FILE *f));
/* Dump out rb tree to file, mostly for debugging. */

int rbTreeCmpString(void *a, void *b);	
/* Set up rbTree so as to work on strings. */

int rbTreeCmpWord(void *a, void *b);	
/* Set up rbTree so as to work on case-insensitive strings. */

void rbTreeTraverseRangeWithContext(struct rbTree *tree, void *minItem, void *maxItem,
	void (*doItem)(void *item, void *context), void *context);
/* Apply doItem function to all items in tree such that
 * minItem <= item <= maxItem.  THREAD SAFE */

#endif /* RBTREE_H */

