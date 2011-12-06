/* rbmTree - Red-Black Tree with Merging of overlapping values - 
 * a type of binary tree which automatically keeps itself relatively balanced 
 * during inserts and deletions.
 *   original author: Shane Saunders
 *   adapted into local conventions: Jim Kent
 *   extended to support merging of overlaps: Angie Hinrichs
 */

#include "common.h"
#include "localmem.h"
#include "rbmTree.h"


/* The stack keeps us from having to keep explicit
 * parent, grandparent, greatgrandparent variables.
 * It needs to be big enough for the maximum depth
 * of tree.  Since the whole point of rb trees is
 * that they are self-balancing, this is not all
 * that deep, just 2*log2(N).  Therefore a stack of
 * 256 is good for up to 2^128 items in stack, which
 * should keep us for the next couple of millenia... */


static struct rbTreeNode *restructure(struct rbmTree *t, int tos, 
	struct rbTreeNode *x, struct rbTreeNode *y, struct rbTreeNode *z)
/* General restructuring function - checks for all
 * restructuring cases.  Call when insert has messed up tree.
 * Sadly delete has to do even more work. */
{
struct rbTreeNode *parent, *midNode;

if(y == x->left) 
    {
    if(z == y->left) 
        {  /* in-order:  z, y, x */
	midNode = y;
	y->left = z;
	x->left = y->right;
	y->right = x;
	}
    else 
        {  /* in-order:  y, z, x */
	midNode = z;
	y->right = z->left;
	z->left = y;
	x->left = z->right;
	z->right = x;
	}
    }
else 
    {
    if(z == y->left) 
	{  /* in-order:  x, z, y */
	midNode = z;
	x->right = z->left;
	z->left = x;
	y->left = z->right;
	z->right = y;
	}
    else 
	{  /* in-order:  x, y, z */
	midNode = y;
	x->right = y->left;
	y->left = x;
	y->right = z;
	}
    }
if(tos != 0) 
    {
    parent = t->stack[tos-1];
    if(x == parent->left) 
	parent->left = midNode;
    else 
	parent->right = midNode;
    }
else 
    t->root = midNode;

return midNode;
}

struct rbmTree *rbmTreeNewDetailed(rbmTreeCompareFunction *compare,
				   rbmTreeItemMergeFunction *itemMerge,
				   rbmTreeItemSubtractFunction *itemSubtract,
				   rbmTreeItemFreeFunction *itemFree,
				   struct lm *lm,
				   struct rbTreeNode *stack[256])
/* Allocate rbmTree on an existing local memory & stack.  This is for cases
 * where you want a lot of trees, and don't want the overhead for each one. 
 * Note, to clean these up, just do freez(&rbmTree) rather than 
 * rbmTreeFree(&rbmTree)! */
{
struct rbmTree *t;
AllocVar(t);
t->root = NULL;
t->lm = lm;
t->stack = stack;	
t->n = 0;
t->compare = compare;
t->itemMerge = itemMerge;
t->itemSubtract = itemSubtract;
t->itemFree = itemFree;
return t;
}

struct rbmTree *rbmTreeNew(rbmTreeCompareFunction *compare,
			   rbmTreeItemMergeFunction *itemMerge,
			   rbmTreeItemSubtractFunction *itemSubtract,
			   rbmTreeItemFreeFunction *itemFree)
/* Allocates space for a red-black merging tree 
 * and returns a pointer to it.  */
{
struct lm *lm = lmInit(0);
struct rbTreeNode **stack = lmAlloc(lm, 256 * sizeof(stack[0]));	
return rbmTreeNewDetailed(compare, itemMerge, itemSubtract, itemFree,
			  lm, stack);
}


void rbmTreeFree(struct rbmTree **pTree)
/* rbmTreeFree() - Frees space used by the red-black tree pointed to by t. */
{
struct rbmTree *tree = *pTree;
if (tree != NULL)
    {
    lmCleanup(&tree->lm);
    freez(pTree);
    }
}


void rbmTreeFreeAll(struct rbmTree **pTree)
/* Frees space used by the red-black tree pointed to by t and t's items. */
{
struct rbmTree *tree = *pTree;
if (tree != NULL)
    {
    rbmTreeTraverse(tree, tree->itemFree);
    lmCleanup(&tree->lm);
    freez(pTree);
    }
}


static void rbmTreeInsertionCleanup(struct rbmTree *t, struct rbTreeNode *p,
				   struct rbTreeNode *x, int tos)
/* Restructuring or recolouring will be needed if node x and its parent, p,
 * are both red.
 */
{
struct rbTreeNode **stack = t->stack;
struct rbTreeNode *q, *m;

while(p->color == rbTreeRed) 
    {  /* Double red problem. */

    /* Obtain a pointer to p's parent, m, and sibling, q. */
    m = stack[--tos];
    q = p == m->left ? m->right : m->left;

    /* Determine whether restructuring or recolouring is needed. */
    if(!q || q->color == rbTreeBlack) 
	{
	/* Sibling is black.  ==>  Perform restructuring. */
	/* Restructure according to the left to right order, of nodes
	 * m, p, and x.
	 */
	m = restructure(t, tos, m, p, x);
	m->color = rbTreeBlack;
	m->left->color = m->right->color = rbTreeRed;

	/* Restructuring eliminates the double red problem. */
	break;
	}
    /* else just need to flip color */
	
    /* Sibling is also red.  ==>  Perform recolouring. */
    p->color = rbTreeBlack;
    q->color = rbTreeBlack;

    if(tos == 0) break;  /* The root node always remains black. */
	    
    m->color = rbTreeRed;

    /* Continue, checking colouring higher up. */
    x = m;
    p = stack[--tos];
    }
}


void *rbmTreeFindComplete(struct rbmTree *t, void *item)
/* Find an item in the red-black tree whose value encompasses the given item.
 * Return that item, or return NULL if a partial overlap or no overlap 
 * was found. */
{
rbmTreeCompareFunction *compare = t->compare;
struct rbTreeNode *p, *nextP = NULL;
    
/* Repeatedly explore either the left or right branch, depending on the
 * value of the key, until the correct item is found.  */
for (p = t->root; p != NULL; p = nextP)
    {
    rbmTreeCompareResult cmpResult = compare(item, p->item);
    if (cmpResult == RBMT_LESS)
	nextP = p->left;
    else if (cmpResult == RBMT_GREATER)
	nextP = p->right;
    else if (cmpResult == RBMT_COMPLETE)
	return p->item;
    else
	return NULL;
    }
return NULL;
}


static void rbmTreeDeletionCleanup(struct rbmTree *t, struct rbTreeNode *x,
				  struct rbTreeNode *y, struct rbTreeNode *r,
				  int tos)
/* Removal of a black node requires some adjustment. */
/* The pointers x, y, and r point to nodes which may be involved in
 * restructuring and recolouring.
 *  x - the parent of the removed node.
 *  y - the sibling of the removed node.
 *  r - the node which replaced the removed node.
 * The next entry off the stack will be the parent of node x.
 */
{
struct rbTreeNode **stack = t->stack;
struct rbTreeNode *z, *b, *newY;

if(!r || r->color == rbTreeBlack) 
    {
    /* A black node replaced the deleted black node.  Note that
     * external nodes (NULL child pointers) are always black, so
     * if r is NULL it is treated as a black node.
     */

    /* This causes a double-black problem, since node r would need to
     * be coloured double-black in order for the black color on
     * paths through r to remain the same as for other paths.
     */

    /* If r is the root node, the double-black color is not necessary
     * to maintain the color balance.  Otherwise, some adjustment of
     * nearby nodes is needed in order to eliminate the double-black
     * problem.  NOTE:  x points to the parent of r.
     */
    if(x) for(;;) 
	{

	/* There are three adjustment cases:
	 *  1.  r's sibling, y, is black and has a red child, z.
	 *  2.  r's sibling, y, is black and has two black children.
	 *  3.  r's sibling, y, is red.
	 */
	if(y->color == rbTreeBlack) 
	    {

	    /* Note the conditional evaluation for assigning z. */
	    if(((z = y->left) && z->color == rbTreeRed) ||
	       ((z = y->right) && z->color == rbTreeRed)) 
		{		    
		/* Case 1:  perform a restructuring of nodes x, y, and
		 * z.
		 */
		    
		b = restructure(t, tos, x, y, z);
		b->color = x->color;
		b->left->color = b->right->color = rbTreeBlack;
		    
		break;
		}
	    else 
		{
		/* Case 2:  recolour node y red. */
		    
		y->color = rbTreeRed;
		    
		if(x->color == rbTreeRed) 
		    {
		    x->color = rbTreeBlack;
		    break;
		    }
		/* else */

		if(tos == 0) break;  /* Root level reached. */
		/* else */
		    
		r = x;
		x = stack[--tos];  /* x <- parent of x. */
		y = x->left == r ? x->right : x->left;
		}
	    }
	else 
	    {
	    /* Case 3:  Restructure nodes x, y, and z, where:
	     *  - If node y is the left child of x, then z is the left
	     *    child of y.  Otherwise z is the right child of y.
	     */
	    if(x->left == y) 
		{
		newY = y->right;
		z = y->left;
		}
	    else 
		{
		newY = y->left;
		z = y->right;
		}
		
	    restructure(t, tos, x, y, z);
	    y->color = rbTreeBlack;
	    x->color = rbTreeRed;

	    /* Since x has moved down a place in the tree, and y is the
	     * new the parent of x, the stack must be adjusted so that
	     * the parent of x is correctly identified in the next call
	     * to restructure().
	     */
	    stack[tos++] = y;

	    /* After restructuring, node r has a black sibling, newY,
	     * so either case 1 or case 2 applies.  If case 2 applies
	     * the double-black problem does not reappear.
	     */
	    y = newY;
		
	    /* Note the conditional evaluation for assigning z. */
	    if(((z = y->left) && z->color == rbTreeRed) ||
	       ((z = y->right) && z->color == rbTreeRed)) 
		{		    
		/* Case 1:  perform a restructuring of nodes x, y, and
		 * z.
		 */
		    
		b = restructure(t, tos, x, y, z);
		b->color = rbTreeRed;  /* Since node x was red. */
		b->left->color = b->right->color = rbTreeBlack;
		}
	    else 
		{
		/* Case 2:  recolour node y red. */

		/* Note that node y is black and node x is red. */
		    
		y->color = rbTreeRed;
		x->color = rbTreeBlack;
		}

	    break;
	    }
	}
    }
else 
    {
    /* A red node replaced the deleted black node. */

    /* In this case we can simply color the red node black. */
    r->color = rbTreeBlack;
    }
}

static void *rbmTreeDeleteOneNode(struct rbmTree *t, struct rbTreeNode *p,
				 int tos)
/* p points to the node to be deleted, and is currently on the top of the
 * stack.
 */
{
struct rbTreeNode **stack = t->stack;
struct rbTreeNode *r, *x, *y, *m;
rbTreeColor removeCol;
void *returnItem = NULL;
int i;

if(!p->left) 
    {
    tos--;  /* Adjust tos to remove p. */
    /* Right child replaces p. */
    if(tos == 0) 
	{
	r = t->root = p->right;
	x = y = NULL;
	}
    else 
	{
	x = stack[--tos];
	if(p == x->left) 
	    {
	    r = x->left = p->right;
	    y = x->right;
	    }
	else 
	    {
	    r = x->right = p->right;
	    y = x->left;
	    }
	}
    removeCol = p->color;
    }
else if(!p->right) 
    {
    tos--;  /* Adjust tos to remove p. */
    /* Left child replaces p. */
    if(tos == 0) 
	{
	r = t->root = p->left;
	x = y = NULL;
	}
    else 
	{
	x = stack[--tos];
	if(p == x->left) 
	    {
	    r = x->left = p->left;
	    y = x->right;
	    }
	else 
	    {
	    r = x->right = p->left;
	    y = x->left;
	    }
	}
    removeCol = p->color;
    }
else 
    {
    /* Save p's stack position. */
    i = tos-1;
    
    /* Minimum child, m, in the right subtree replaces p. */
    m = p->right;
    do 
	{
	stack[tos++] = m;
	m = m->left;
	} while(m);
    m = stack[--tos];

    /* Update either the left or right child pointers of p's parent. */
    if(i == 0) 
	{
	t->root = m;
	}
    else 
	{
	x = stack[i-1];  /* p's parent. */
	if(p == x->left) 
	    {
	    x->left = m;
	    }
	else 
	    {
	    x->right = m;
	    }
	}
    
    /* Update the tree part m is removed from, and assign m the child
     * pointers of p (only if m is not the right child of p).
     */
    stack[i] = m;  /* Node m replaces node p on the stack. */
    x = stack[--tos];
    r = m->right;
    if(tos != i) 
	{  /* x is equal to the parent of m. */
	y = x->right;
	x->left = r;
	m->right = p->right;
	}
    else 
	{ /* m was the right child of p, and x is equal to m. */
	y = p->left;
	}
    m->left = p->left;

    /* We treat node m as the node which has been removed. */
    removeCol = m->color;
    m->color = p->color;
    }

/* Get return value and reuse the space used by node p. */
returnItem = p->item;
p->right = t->freeList;
t->freeList = p;

t->n--;

/* The number of black nodes on paths to all external nodes (NULL child
 * pointers) must remain the same for all paths.  Restructuring or
 * recolouring of nodes may be necessary to enforce this.
 */
if(removeCol == rbTreeBlack) 
    {
    rbmTreeDeletionCleanup(t, x, y, r, tos);
    }
return returnItem;
}

void *rbmTreeRemove(struct rbmTree *t, void *item)
/* At the moment there's no demand for this so I'll leave it
 * unimplemented.  But ideally it should delete *any* node from t
 * whose value is completely contained by item, and should snip out
 * any partial overlap with item from existing values in a tree (which
 * could cause a node to be split into two nodes if item is a little
 * portion in the middle of some node's value!!).  Complex, but could
 * come in handy for masking out ranges. */
{
void *bogus = NULL;
errAbort("Sorry, rbmTreeRemove is not yet implemented.");
return bogus;
}


static void rbmTreeLopBranch(struct rbmTree *t, struct rbTreeNode *p)
/* Remove an entire branch without performing cleanup checks -- cleanup 
 * will be performed when this branch's parent is deleted in the usual way. */
{
if (p->left != NULL)
    rbmTreeLopBranch(t, p->left);
if (p->right != NULL)
    rbmTreeLopBranch(t, p->right);
t->itemFree(p->item);
p->left = NULL;
p->right = t->freeList;
t->freeList = p;
t->n--;
}

static void rbmTreeMergeCleanupBranch(struct rbmTree *t, struct rbTreeNode *p,
				      struct rbTreeNode *d, int tos,
				      boolean isLeftDescendent)
/* When a rbmTreeAddMerge finds a partial overlap between the newly added value 
 * and p, search for overlaps in descendents (d) of p.  If d overlaps p, 
 * then merge d's value into p's and remove d (and one of its child 
 * branches). */
{
struct rbTreeNode **stack = t->stack;
rbmTreeCompareResult cmpResult = t->compare(d->item, p->item);

stack[tos++] = d;
if (cmpResult == RBMT_LESS)
    {
    if (d->right != NULL)
	rbmTreeMergeCleanupBranch(t, p, d->right, tos, isLeftDescendent);
    }
else if (cmpResult == RBMT_GREATER)
    {
    if (d->left != NULL)
	rbmTreeMergeCleanupBranch(t, p, d->left, tos, isLeftDescendent);
    }
else
    {
    t->itemMerge(d->item, p->item);
    /* If d is (descended from) p->left and overlaps it, then d->right and 
     * all of its descendents are guaranteed to be included in the newly 
     * expanded p, so get rid of them all.  Likewise for p->right/d->left. 
     * Deletion cleanup will be performed after d is excised. */
    if (isLeftDescendent)
	{
	if (d->left != NULL)
	    rbmTreeMergeCleanupBranch(t, p, d->left, tos, isLeftDescendent);
	if (d->right != NULL)
	    {
	    rbmTreeLopBranch(t, d->right);
	    d->right = NULL;
	    }
	}
    else
	{
	if (d->right != NULL)
	    rbmTreeMergeCleanupBranch(t, p, d->right, tos, isLeftDescendent);
	if (d->left != NULL)
	    {
	    rbmTreeLopBranch(t, d->left);
	    d->left = NULL;
	    }
	}
    rbmTreeDeleteOneNode(t, d, tos);
    }
}


void *rbmTreeAdd(struct rbmTree *t, void *item)
/* Inserts an item into the merge-capable red-black tree pointed to by t,
 * according to its value compared to other items in t.  
 * If item is completely contained by a current item in t, a pointer to that 
 * item is returned.  
 * If item partially overlaps with a current item in t, item is merged into 
 * that item (and other items in the tree might also be merged in if the new 
 * composite item encompasses other existing items).  A pointer to that 
 * merged item is returned.  
 * Otherwise, a new node for item is added to the tree and NULL is returned,
 * indicating a non-merging addition.  
 */
{
struct rbTreeNode *x, *p, **attachX;
rbmTreeCompareFunction *compare = t->compare;
rbmTreeCompareResult cmpResult;
rbTreeColor col;
struct rbTreeNode **stack = NULL;
int tos;

if (compare == NULL)
    errAbort("rbmTreeAddMerge called on non-merging tree -- use rbmTreeAdd.");
tos = 0;    
if((p = t->root) != NULL) 
    {
    stack = t->stack;

    /* Repeatedly explore either the left branch or the right branch
     * depending on the value of the key, until an empty branch is chosen.
     */
    for(;;) 
        {
	stack[tos++] = p;
	cmpResult = compare(item, p->item);
	if(cmpResult == RBMT_LESS) 
	    {
	    p = p->left;
	    if(!p) 
	        {
		p = stack[--tos];
		attachX = &p->left;
		break;
		}
	    }
	else if(cmpResult == RBMT_GREATER)
	    {
	    p = p->right;
	    if(!p) 
	        {
		p = stack[--tos];
		attachX = &p->right;
		break;
		}
	    }
	else if (cmpResult == RBMT_COMPLETE)
	    {
	    return p->item;
	    }
	else 
	    {
	    /* Partial overlap ==> merge item into p->item, then look for 
	     * descendents of p whose values overlap the expanded p->item. 
	     * If any are found, merge their values into p->item and remove 
	     * them from the tree. */
	    t->itemMerge(item, p->item);
	    if (p->left)
		rbmTreeMergeCleanupBranch(t, p, p->left, tos, TRUE);
	    if (p->right)
		rbmTreeMergeCleanupBranch(t, p, p->right, tos, FALSE);
	    return p->item;
	    }
	}
    col = rbTreeRed;
    }
else 
    {
    attachX = &t->root;
    col = rbTreeBlack;
    }

/* Allocate new node and place it in tree. */
if ((x = t->freeList) != NULL)
    t->freeList = x->right;
else
    lmAllocVar(t->lm, x);
x->left = x->right = NULL;
x->item = item;
x->color = col;
*attachX = x;
t->n++;

if(tos > 0) 
    rbmTreeInsertionCleanup(t, p, x, tos);
return NULL;
}


/* Some variables to help recursively dump tree. */
static int dumpLevel;	/* Indentation level. */
static FILE *dumpFile;  /* Output file */
static void (*dumpIt)(void *item, FILE *f);  /* Item dumper. */

static void rTreeDump(struct rbTreeNode *n)
/* Recursively dump. */
{
if (n == NULL)
    return;
spaceOut(dumpFile, ++dumpLevel * 3);
fprintf(dumpFile, "%c ", (n->color ==  rbTreeRed ? 'r' : 'b'));
dumpIt(n->item, dumpFile);
fprintf(dumpFile, "\n");
rTreeDump(n->left);
rTreeDump(n->right);
--dumpLevel;
}

void rbmTreeDump(struct rbmTree *tree, FILE *f, 
	void (*dumpItem)(void *item, FILE *f))
/* Dump out rb tree to file, mostly for debugging. */
{
dumpFile = f;
dumpLevel = 0;
dumpIt = dumpItem;
fprintf(f, "rbmTreeDump\n");
rTreeDump(tree->root);
}



/* Variables to help recursively traverse tree. */
static void (*doIt)(void *item);
static void *minIt, *maxIt;
static rbmTreeCompareFunction *compareIt;

static void rTreeTraverseRange(struct rbTreeNode *n)
/* Recursively traverse tree applying doIt to items between minIt and maxIt. */
{
if (n != NULL)
   {
   rbmTreeCompareResult minCmp = compareIt(n->item, minIt);
   rbmTreeCompareResult maxCmp = compareIt(n->item, maxIt);
   if (minCmp != RBMT_LESS)
       rTreeTraverseRange(n->left);
   if (minCmp != RBMT_LESS && maxCmp != RBMT_GREATER)
       doIt(n->item);
   if (maxCmp != RBMT_GREATER)
       rTreeTraverseRange(n->right);
   }
}

void rbmTreeTraverseRange(struct rbmTree *tree, void *minItem, void *maxItem,
			  void (*doItem)(void *item))
/* Apply doItem function to all items in tree such that
 * minItem <= item <= maxItem */
{
doIt = doItem;
minIt = minItem;
maxIt = maxItem;
    rTreeTraverseRange(tree->root);
}

static void rTreeTraverse(struct rbTreeNode *n)
/* Recursively traverse full tree applying doIt. */
{
if (n != NULL)
    {
    rTreeTraverse(n->left);
    doIt(n->item);
    rTreeTraverse(n->right);
    }
}

void rbmTreeTraverse(struct rbmTree *tree, void (*doItem)(void *item))
/* Apply doItem function to all items in tree. */
{
doIt = doItem;
rTreeTraverse(tree->root);
}

struct slRef *itList;  /* List of items that rbmTreeItemsInRange returns. */

static void addRef(void *item)
/* Add item it itList. */
{
refAdd(&itList, item);
}

struct slRef *rbmTreeItemsInRange(struct rbmTree *tree, void *minItem, void *maxItem)
/* Return a sorted list of references to items in tree between range.
 * slFree this list when done. */
{
itList = NULL;
rbmTreeTraverseRange(tree, minItem, maxItem, addRef);
slReverse(&itList);
return itList;
}

struct slRef *rbmTreeItems(struct rbmTree *tree)
/* Return sorted list of items. */
{
itList = NULL;
rbmTreeTraverse(tree, addRef);
slReverse(&itList);
return itList;
}

