/* newStitch3 - Another stitching experiment - with kd-trees.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "dlist.h"
#include "dystring.h"
#include "axt.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "newStitch3 - Another stitching experiment - with kd-trees.\n"
  "usage:\n"
  "   newStitch3 in.axt out.s3\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct asBlock
/* A block in an alignment.  Used for input as
 * well as output for asStitch. */
    {
    struct asBlock *next;	/* Next in list. */
    int qStart, qEnd;		/* Range covered in query. */
    int tStart, tEnd;		/* Range covered in target. */
    int score;			/* Alignment score for this block. */
    void *ali;			/* Alignment in some format. */
    bool hit;			/* This hit? (I wish this were somewhere else) */
    };

struct asChain
/* A chain of blocks.  Used for output of asStitch. */
    {
    struct asChain *next;	/* Next in list. */
    struct asBlock *blockList;	/* List of blocks. */
    };

struct asBranch
/* A kd-tree.  That is a binary tree which partitions the children
 * into higher and lower one dimension at a time.  We're just doing
 * one in two dimensions, so it alternates between x and y dimensions,
 * or, for alignments between q and t. */
    {
    struct asBranch *low;       /* Pointer to children with lower coordinates. */
    struct asBranch *high;      /* Pointer to children with higher coordinates. */
    struct asBlock *block;	/* Block pointer for leaves on tree. */
    int cutCoord;		/* Coordinate (in some dimension) to cut on */
    int maxScore;		/* Max score of any leaf below us. */
    };

struct asTree
/* The root of our tree.  */
    {
    struct asBranch *root;	/* Pointer to root of kd-tree. */
    struct lm *lm;		/* Branches etc. allocated from here. */
    };

int asBlockCmpQ(const void *va, const void *vb)
/* Compare to sort based on target start. */
{
const struct asBlock *a = *((struct asBlock **)va);
const struct asBlock *b = *((struct asBlock **)vb);
return a->qStart - b->qStart;
}

int asBlockCmpT(const void *va, const void *vb)
/* Compare to sort based on target start. */
{
const struct asBlock *a = *((struct asBlock **)va);
const struct asBlock *b = *((struct asBlock **)vb);
return a->tStart - b->tStart;
}

int medianVal(struct dlList *list, int medianIx, int dim)
/* Return value of median block in list on given dimension */
{
struct dlNode *node = list->head;
struct asBlock *block = NULL;
int i;

/* Figure out median value in this dimension */
for (i=0; i<medianIx; ++i)
    {
    node = node->next;
    block = node->val;
    block->hit = TRUE;
    }
return (dim == 0 ? block->qStart : block->tStart);
}

int splitList(struct dlList *oldList, struct dlList *newList, int cutCoord)
/* Peel off members of oldList that are greater than cutCoord
 * in given dimension onto new list.  Return count on new list. */
{
struct dlNode *node, *next;
struct asBlock *block;
int newCount = 0;
dlListInit(newList);
for (node = oldList->head; !dlEnd(node); node = next)
    {
    next = node->next;
    block = node->val;
    if (block->hit)
	{
	dlRemove(node);
	dlAddTail(newList, node);
	++newCount;
	}
    }
return newCount;
}

void clearHits(struct dlList *list)
/* Clear hit flags of all blocks on list. */
{
struct dlNode *node;
for (node = list->head; !dlEnd(node); node = node->next)
    {
    struct asBlock *block = node->val;
    block->hit = FALSE;
    }
}

static struct lm *lm;
/* Local memory pool for branches. */

struct asBranch *kdBuild(int nodeCount, struct dlList *lists[2], int dim)
/* Build up kd-tree recursively. */
{
struct asBranch *branch;

lmAllocVar(lm, branch);
if (nodeCount <= 1)
    {
    branch->block = lists[0]->head->val;
    }
else
    {
    int cutCoord, newCount;
    struct dlList *newLists[2];
    struct dlList newQ, newT;
    int nextDim = 1-dim;

    /* Subdivide lists along median.  */
    newLists[0] = &newQ;
    newLists[1] = &newT;
    clearHits(lists[dim]);
    cutCoord = medianVal(lists[dim], nodeCount/2, dim);
    newCount = splitList(lists[0], newLists[0], cutCoord);
    splitList(lists[1], newLists[1], cutCoord);

    /* Recurse on each side. */
    branch->low = kdBuild(nodeCount - newCount, lists, nextDim);
    branch->high = kdBuild(newCount, newLists, nextDim);
    }
return branch;
}


struct asTree *asTreeMake(struct asBlock *blockList)
/* Make a kd-tree containing blockList. */
{
struct asBlock *block;
int nodeCount = slCount(blockList);
struct asTree *tree;
struct dlList qList, tList,*lists[2];
struct dlNode *qNodes, *tNodes;
int i;

/* Build lists sorted in each dimension. This
 * will help us quickly find medians while constructing
 * the kd-tree. Block list is sorted initially on t.*/
dlListInit(&qList);
dlListInit(&tList);
AllocArray(qNodes, nodeCount);
AllocArray(tNodes, nodeCount);
for (i=0 , block=blockList; block != NULL; block = block->next, ++i)
    {
    qNodes[i].val = tNodes[i].val = block;
    dlAddTail(&qList, &qNodes[i]);
    dlAddTail(&tList, &tNodes[i]);
    }
dlSort(&qList, asBlockCmpQ);
lists[0] = &qList;
lists[1] = &tList;

/* Allocate master data structure and call recursive builder. */
AllocVar(tree);
tree->lm = lm = lmInit(0);
tree->root = kdBuild(nodeCount, lists, 0);
lm = NULL;

/* Clean up and go home. */
freeMem(qNodes);
freeMem(tNodes);
return tree;
}

void asTreeFree(struct asTree **pTree)
/* Free up tree */
{
struct asTree *tree = *pTree;
if (tree != NULL)
    {
    lmCleanup(&tree->lm);
    freez(pTree);
    }
}

struct seqPair
/* Pair of sequences. */
    {
    struct seqPair *next;
    char *name;	/* Allocated in hash */
    struct axt *axtList; /* List of alignments. */
    };

void chainPair(struct seqPair *sp, FILE *f)
/* Make chains for all alignments in sp. */
{
long startTime, dt;
struct asNode *n, *nList = NULL;
struct asTree *tree;
struct axt *axt, *first, *last;
struct asBlock *blockList = NULL, *block;

/* Make block list */
for (axt = sp->axtList; axt != NULL; axt = axt->next)
    {
    AllocVar(block);
    block->qStart = axt->qStart;
    block->qEnd = axt->qEnd;
    block->tStart = axt->tStart;
    block->tEnd = axt->tEnd;
    block->score = axt->score;
    block->ali = axt;
    slAddHead(&blockList, block);
    }
slSort(&blockList, asBlockCmpT);

/* Make up tree and time it for debugging. */
startTime = clock1000();
tree = asTreeMake(blockList);
dt = clock1000() - startTime;
uglyf("Made tree in %f seconds\n", dt*0.001);

asTreeFree(&tree);

slFreeList(&blockList);
}


void newStitch3(char *axtFile, char *output)
/* newStitch3 - Another stitching experiment - with kd-trees.. */
{
struct hash *pairHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct dyString *dy = newDyString(512);
struct lineFile *lf = lineFileOpen(axtFile, TRUE);
struct axt *axt;
struct seqPair *spList = NULL, *sp;
FILE *f = mustOpen(output, "w");

/* Read input file and divide alignments into various parts. */
while ((axt = axtRead(lf)) != NULL)
    {
    if (axt->score < 500)
        {
	axtFree(&axt);
	continue;
	}
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", axt->qName, axt->qStrand, axt->tName);
    sp = hashFindVal(pairHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pairHash, dy->string, sp, &sp->name);
	}
    slAddHead(&sp->axtList, axt);
    }
for (sp = spList; sp != NULL; sp = sp->next)
    {
    slReverse(&sp->axtList);
    chainPair(sp, f);
    }
dyStringFree(&dy);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
newStitch3(argv[1], argv[2]);
return 0;
}
