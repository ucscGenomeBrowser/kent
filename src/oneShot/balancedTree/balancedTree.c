/* rbTree - Red/Black trees.  These are self-balancing
 * binary trees.  The implementation here is based on
 * study of the Algorithms book by Sedgewich and the
 * source code at
 *  http://www.cosc.canterbury.ac.nz/~tad/alg/dict/rbtree.c
 * Many thanks to Tadao Takaoka for putting this source
 * on the net. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "balancedTree - Balanced trees with fast insert/deletion\n"
  "usage:\n"
  "   balancedTree now\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct rbNode
/* A node on a rb (relatively balanced, or red/black) tree. */
    {
    struct rbNode *left, *right; /* Left and right children if any */
    boolean red;		  /* True if a 'Red' node. */
    int val;			  /* Value stored at this node. */
    };

struct rbTree
/* A relatively balanced (red/black) tree. */
    {
    struct rbTree *next;	/* Next tree in list. */
    struct rbNode root;		/* Tree grows off of root's right child */
    struct lm *lm;		/* Memory pool for tree. */
    struct rbNode *freeList;	/* List of free nodes. */
    };

struct rbTree *rbTreeNew()
/* Allocate a new balanced tree. */
{
struct rbTree *tree;
AllocVar(tree);
tree->lm = lmInit(0);
tree->root.val = -BIGNUM;
return tree;
}

void rbTreeFree(struct rbTree **pTree)
/* Free a balanced tree. */
{
struct rbTree *tree = *pTree;
if (tree != NULL)
   {
   lmCleanup(&tree->lm);
   freez(pTree);
   }
}

struct rbNode *rbTreeNeedNode(struct rbTree *tree)
/* Allocate node, recycling from freeList if possible. */
{
struct rbNode *n;
if ((n = tree->freeList) != NULL)
    {
    tree->freeList = n->right;
    }
else
    lmAllocVar(tree->lm, n);
return n;
}

static FILE *dumpFile;
static int dumpLevel;

static void spaceOut(FILE *f, int count)
/* Put out some spaces to file. */
{
while (--count >= 0)
    fputc(' ', f);
}

static void rTreeDump(struct rbNode *n, char type)
/* Recursively dump. */
{
if (n == NULL)
    return;
spaceOut(dumpFile, ++dumpLevel);
fprintf(dumpFile, "%c %d (%s)\n", type, n->val, (n->isRed ? "red" : "black"));
rTreeDump(n->left, 'l');
rTreeDump(n->right, 'r');
--dumpLevel;
}


void rbTreeDump(struct rbTree *tree, FILE *f)
/* Dump out balanced tree. */
{
dumpFile = f;
dumpLevel = 0;
fprintf(f, "rbTreeDump\n");
rTreeDump(&tree->root, 'r');
}

struct rbNode *rbTreeInsert(struct rbTree *tree, int val)
/* Insert new value into balanced tree. */
{
struct rbNode *p, *n;
for (p = &tree->root; ; p = n)
    {
    if (val <= p->val)
        {
	p->leftCount += 1;
	if ((n = p->left) == NULL)
	    {
	    n = rbTreeNeedNode(tree);
	    n->val = val;
	    p->left = n;
	    return n;
	    }
	}
    else
        {
	p->rightCount += 1;
	if ((n = p->right) == NULL)
	    {
	    n = rbTreeNeedNode(tree);
	    n->val = val;
	    p->right = n;
	    return n;
	    }
	}
    }
}

int testList[] = {1, 5, 9, 7, 4, 3, 8, 2, 6, 0};

void balancedTree(char *XXX)
/* balancedTree - Balanced trees with fast insert/deletion. */
{
struct rbTree *tree = rbTreeNew();
int i;
for (i=0; i<ArraySize(testList); ++i)
    {
    rbTreeInsert(tree, testList[i]);
    }
rbTreeDump(tree, uglyOut);
rbTreeFree(&tree);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
balancedTree(argv[1]);
return 0;
}
