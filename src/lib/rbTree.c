/* rbTree - rbTreeRed-rbTreeBlack Tree - a type of binary tree which 
 * automatically keeps relatively balanced during
 * inserts and deletions.
 *   original author: Shane Saunders
 *   adapted into local conventions: Jim Kent
 */

#include "common.h"
#include "localmem.h"
#include "rbTree.h"



static struct rbTreeNode *restructure(struct rbTree *t, int tos, 
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

struct rbTree *rbTreeNewDetailed(int (*compare)(void *, void *), struct lm *lm, 
	struct rbTreeNode *stack[128])
/* Allocate rbTree on an existing local memory & stack.  This is for cases
 * where you want a lot of trees, and don't want the overhead for each one. 
 * Note, to clean these up, just do freez(&rbTree) rather than rbFreeTree(&rbTree). */
{
struct rbTree *t;
AllocVar(t);
t->root = NULL;
t->compare = compare;
t->lm = lm;
t->stack = stack;	
t->n = 0;
return t;
}

struct rbTree *rbTreeNew(int (*compare)(void *, void *))
/* rbTreeNew() - Allocates space for a red-black tree and returns a pointer
 * to it.  The function compare compares they keys of two items, and returns a
 * negative, zero, or positive integer depending on whether the first item is
 * less than, equal to, or greater than the second.  */
{
/* The stack keeps us from having to keep explicit
 * parent, grandparent, greatgrandparent variables.
 * It needs to be big enough for the maximum depth
 * of tree.  Since the whole point of rb trees is
 * that they are self-balancing, this is not all
 * that deep, just 2*log2(N).  Therefore a stack of
 * 128 is good for up to 2^64 items in stack, which
 * should keep us for the next couple of decades... */
struct lm *lm = lmInit(0);
struct rbTreeNode **stack = lmAlloc(lm, 128 * sizeof(stack[0]));	
return rbTreeNewDetailed(compare, lm, stack);
}


void rbTreeFree(struct rbTree **pTree)
/* rbTreeFree() - Frees space used by the red-black tree pointed to by t. */
{
struct rbTree *tree = *pTree;
if (tree != NULL)
    {
    lmCleanup(&tree->lm);
    freez(pTree);
    }
}

void rbTreeFreeList(struct rbTree **pList)
/* Free up a list of rbTrees. */
{
struct rbTree *tree, *next;
for (tree = *pList; tree != NULL; tree = next)
    {
    next = tree->next;
    rbTreeFree(&tree);
    }
}

void *rbTreeAdd(struct rbTree *t, void *item)
/* rbTreeAdd() - Inserts an item into the red-black tree pointed to by t,
 * according the the value its key.  The key of an item in the red-black
 * tree must be unique among items in the tree.  If an item with the same key
 * already exists in the tree, a pointer to that item is returned.  Otherwise,
 * NULL is returned, indicating insertion was successful.
 */
{
struct rbTreeNode *x, *p, *q, *m, **attachX;
int (* compare)(void *, void *);
int cmpResult;
rbTreeColor col;
struct rbTreeNode **stack = NULL;
int tos;

tos = 0;    
if((p = t->root) != NULL) 
    {
    compare = t->compare;
    stack = t->stack;

    /* Repeatedly explore either the left branch or the right branch
     * depending on the value of the key, until an empty branch is chosen.
     */
    for(;;) 
        {
	stack[tos++] = p;
	cmpResult = compare(item, p->item);
	if(cmpResult < 0) 
	    {
	    p = p->left;
	    if(!p) 
	        {
		p = stack[--tos];
		attachX = &p->left;
		break;
		}
	    }
	else if(cmpResult > 0) 
	    {
	    p = p->right;
	    if(!p) 
	        {
		p = stack[--tos];
		attachX = &p->right;
		break;
		}
	    }
	else 
	    {
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

/* Restructuring or recolouring will be needed if node x and its parent, p,
 * are both red.
 */
if(tos > 0) 
    {
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

return NULL;
}


void *rbTreeFind(struct rbTree *t, void *item)
/* rbTreeFind() - Find an item in the red-black tree with the same key as the
 * item pointed to by `item'.  Returns a pointer to the item found, or NULL
 * if no item was found.
 */
{
struct rbTreeNode *p, *nextP;
int (*compare)(void *, void *) = t->compare;
int cmpResult;
    
/* Repeatedly explore either the left or right branch, depending on the
 * value of the key, until the correct item is found.  */
for (p = t->root; p != NULL; p = nextP)
    {
    cmpResult = compare(item, p->item);
    if(cmpResult < 0) 
	nextP = p->left;
    else if(cmpResult > 0) 
	nextP = p->right;
    else 
	return p->item;
    }
return NULL;
}


void *rbTreeRemove(struct rbTree *t, void *item)
/* rbTreeRemove() - Delete an item in the red-black tree with the same key as
 * the item pointed to by `item'.  Returns a pointer to the deleted item,
 * and NULL if no item was found.
 */
{
struct rbTreeNode *p, *r, *x, *y, *z, *b, *newY;
struct rbTreeNode *m;
rbTreeColor removeCol;
void *returnItem;
int (* compare)(void *, void *);
int cmpResult;
struct rbTreeNode **stack;
int i, tos;


/* Attempt to locate the item to be deleted. */
if((p = t->root)) 
    {
    compare = t->compare;
    stack = t->stack;
    tos = 0;
    
    for(;;) 
	{
	stack[tos++] = p;
	cmpResult = compare(item, p->item);
	if(cmpResult < 0) 
	    p = p->left;
	else if(cmpResult > 0) 
	    p = p->right;
	else 
	    /* Item found. */
	    break;
	if(!p) return NULL;
	}
    }
else 
    return NULL;

/* p points to the node to be deleted, and is currently on the top of the
 * stack.
 */
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

/* The pointers x, y, and r point to nodes which may be involved in
 * restructuring and recolouring.
 *  x - the parent of the removed node.
 *  y - the sibling of the removed node.
 *  r - the node which replaced the removed node.
 * From the above code, the next entry off the stack will be the parent of
 * node x.
 */

/* The number of black nodes on paths to all external nodes (NULL child
 * pointers) must remain the same for all paths.  Restructuring or
 * recolouring of nodes may be necessary to enforce this.
 */
if(removeCol == rbTreeBlack) 
    {
    /* Removal of a black node requires some adjustment. */
    
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
return returnItem;
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

void rbTreeDump(struct rbTree *tree, FILE *f, 
	void (*dumpItem)(void *item, FILE *f))
/* Dump out rb tree to file, mostly for debugging. */
{
dumpFile = f;
dumpLevel = 0;
dumpIt = dumpItem;
fprintf(f, "rbTreeDump\n");
rTreeDump(tree->root);
}



/* Variables to help recursively traverse tree. */
static void (*doIt)(void *item);
static void *minIt, *maxIt;
static int (*compareIt)(void *, void *);

static void rTreeTraverseRange(struct rbTreeNode *n)
/* Recursively traverse tree in range applying doIt. */
{
if (n != NULL)
   {
   int minCmp = compareIt(n->item, minIt);
   int maxCmp = compareIt(n->item, maxIt);
   if (minCmp >= 0)
       rTreeTraverseRange(n->left);
   if (minCmp >= 0 && maxCmp <= 0)
       doIt(n->item);
   if (maxCmp <= 0)
       rTreeTraverseRange(n->right);
   }
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


void rbTreeTraverseRange(struct rbTree *tree, void *minItem, void *maxItem,
	void (*doItem)(void *item))
/* Apply doItem function to all items in tree such that
 * minItem <= item <= maxItem */
{
doIt = doItem;
minIt = minItem;
maxIt = maxItem;
compareIt = tree->compare;
rTreeTraverseRange(tree->root);
}

void rbTreeTraverse(struct rbTree *tree, void (*doItem)(void *item))
/* Apply doItem function to all items in tree */
{
doIt = doItem;
rTreeTraverse(tree->root);
}

struct rTreeContext
/* Context for traversing a tree when you want to be fully thread safe and reentrant. */
    {
    void *context;	/* Some context carried from user and passed to doIt. */
    void (*doItem)(void *item, void *context);
    };

static void rTreeTraverseWithContext(struct rbTreeNode *n, struct rTreeContext *context)
/* Traverse tree with a little context so don't need little static variables that
 * prevent reentrancy of callback functions. */
{
if (n != NULL)
    {
    rTreeTraverseWithContext(n->left, context);
    context->doItem(n->item, context->context);
    rTreeTraverseWithContext(n->right, context);
    }
}

void rbTreeTraverseWithContext(struct rbTree *tree, 
	void (*doItem)(void *item, void *context), void *context)
/* Traverse tree calling doItem on every item with context pointer passed through to doItem.
 * This often avoids having to declare global or static variables for the doItem callback to use. */
{
struct rTreeContext ctx;
ctx.context = context;
ctx.doItem = doItem;
rTreeTraverseWithContext(tree->root, &ctx);
}

struct slRef *itList;  /* List of items that rbTreeItemsInRange returns. */

static void addRef(void *item)
/* Add item it itList. */
{
refAdd(&itList, item);
}

struct slRef *rbTreeItemsInRange(struct rbTree *tree, void *minItem, void *maxItem)
/* Return a sorted list of references to items in tree between range.
 * slFreeList this list when done. */
{
itList = NULL;
rbTreeTraverseRange(tree, minItem, maxItem, addRef);
slReverse(&itList);
return itList;
}

static void addRefWithContext(void *item, void *context)
/* Add item it itList. */
{
struct slRef **pList = context;
refAdd(pList, item);
}


struct slRef *rbTreeItems(struct rbTree *tree)
/* Return sorted list of items.  slFreeList this when done.*/
{
struct slRef *list = NULL;
rbTreeTraverseWithContext(tree, addRefWithContext, &list);
slReverse(&list);
return list;
}

int rbTreeCmpString(void *a, void *b)
/* Set up rbTree so as to work on strings. */
{
return strcmp(a, b);
}

int rbTreeCmpWord(void *a, void *b)	
/* Set up rbTree so as to work on case-insensitive strings. */
{
return differentWord(a,b);
}
