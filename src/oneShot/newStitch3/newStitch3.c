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
    struct asBranch *branch;	/* Node in tree. Used internally. */
    struct asBranch *bestPred;	/* Best predicessor - used internally. */
    int totalScore;		/* Total score of chain up to here. */
    bool hit;			/* This hit? Used by system internally. */
    };

struct asChain
/* A chain of blocks.  Used for output of asStitch. */
    {
    struct asChain *next;	/* Next in list. */
    struct asBlock *blockList;	/* List of blocks. */
    int score;			/* Total score for chain. */
    };

struct asBranch
/* A kd-tree. That is a binary tree which partitions the children
 * into higher and lower one dimension at a time.  We're just doing
 * one in two dimensions, so it alternates between q and t dimensions. */
    {
    struct asBranch *lo;      /* Pointer to children with lower coordinates. */
    struct asBranch *hi;      /* Pointer to children with higher coordinates. */
    struct asBlock *block;    /* Block pointer for leaves on tree. */
    int cutCoord;	      /* Coordinate (in some dimension) to cut on */
    int maxScore;	      /* Max score of any leaf below us. */
    };

struct asTree
/* The root of our tree.  */
    {
    struct asBranch *root;	/* Pointer to root of kd-tree. */
    struct lm *lm;		/* Branches etc. allocated from here. */
    int maxDepth;		/* Maximum depth of tree. */
    };

void indent(FILE *f, int count)
/* Print given number of spaces. */
{
while (--count >= 0)
    fprintf(f, "  ");
}

void rBranchDump(struct asBranch *branch, int depth, FILE *f)
/* Dump branches. */
{
int i;
if (branch->block)
    {
    indent(f, depth+1);
    fprintf(f, "[%d] %d (q), %d (t)\n", branch->maxScore, branch->block->qStart, branch->block->tStart);
    }
else
    {
    rBranchDump(branch->lo, depth+1, f);
    indent(f, depth+1);
    fprintf(f, "%c [%d] %d\n", ((depth&1) == 0 ? 'q' : 't'), branch->maxScore, branch->cutCoord);
    rBranchDump(branch->hi, depth+1, f);
    }
}

void asTreeDump(struct asTree *tree, FILE *f)
/* Dump out tree to file. */
{
fprintf(f, "treeDump: maxDepth=%d\n", tree->maxDepth);
rBranchDump(tree->root, 0, f);
}

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
/* Return value of median block in list on given dimension 
 * Mark blocks up to median as hit. */
{
struct dlNode *node = list->head;
struct asBlock *block = NULL;
int i;

for (i=0; i<medianIx; ++i)
    {
    block = node->val;
    block->hit = TRUE;  
    node = node->next;
    }
return (dim == 0 ? block->qStart : block->tStart);
}

int splitList(struct dlList *oldList, struct dlList *newList)
/* Peel off members of oldList that are not hit onto new list. */
{
struct dlNode *node, *next;
struct asBlock *block;
int newCount = 0;
dlListInit(newList);
for (node = oldList->head; !dlEnd(node); node = next)
    {
    next = node->next;
    block = node->val;
    if (!block->hit)
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

static struct lm *lm; /* Local memory pool for branches. */
static int depth;     /* Current recursion depth. */
static int maxDepth;  /* Maximum recursion depth. */


void dumpList(char *name, struct dlList *list)
/* Dump list of blocks. */
{
struct dlNode *node;
uglyf("%s\n", name);
for (node = list->head; !dlEnd(node); node = node->next)
    {
    struct asBlock *block = node->val;
    uglyf("  %d,%d  %d,%d\n", block->qStart,block->qEnd, block->tStart,block->tEnd);
    }
}

struct asBranch *kdBuild(int nodeCount, struct dlList *lists[2], int dim)
/* Build up kd-tree recursively. */
{
struct asBranch *branch;

if (++depth > maxDepth)
    maxDepth = depth;
// uglyf("kdBuild depth %d, nodeCount %d\n", depth, nodeCount);
// dumpList(" qList", lists[0]);
// dumpList(" tList", lists[1]);
lmAllocVar(lm, branch);
if (nodeCount <= 1)
    {
    struct asBlock *block = lists[0]->head->val;
    branch->block = block;
    block->branch = branch;
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
    clearHits(lists[0]);
    branch->cutCoord = medianVal(lists[dim], nodeCount/2, dim);
    newCount = splitList(lists[0], newLists[0]);
    splitList(lists[1], newLists[1]);

    /* Recurse on each side. */
    branch->lo = kdBuild(nodeCount - newCount, lists, nextDim);
    branch->hi = kdBuild(newCount, newLists, nextDim);
    }
--depth;
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
 * the kd-tree. */
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
dlSort(&qList, asBlockCmpQ); /* tList sorted since blockList is sorted. */
lists[0] = &qList;
lists[1] = &tList;

/* Allocate master data structure and call recursive builder. */
AllocVar(tree);
maxDepth = depth = 0;
tree->lm = lm = lmInit(0);
tree->root = kdBuild(nodeCount, lists, 0);
tree->maxDepth = maxDepth;
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

int defaultGapPenalty(int qSize, int tSize)
/* A logarithmic gap penalty scaled to axt defaults. */
{
int total = qSize + tSize;
if (total <= 0)
    return 0;
return 400 * pow(total, 1.0/3.3);
}

int gap(struct asBlock *a, struct asBlock *b)
/* Return gap score from a to b */
{
int dq = b->qStart - a->qEnd;
int dt = b->tStart - a->tEnd;
if (dq < 0) dq = 0;
if (dt < 0) dt = 0;
return defaultGapPenalty(dq, dt);
}


struct predScore
/* Predecessor and score we get merging with it. */
    {
    struct asBranch *pred;	/* Predecessor. */
    int score;			/* Score of us plus predecessor. */
    };

struct predScore bestPredecessor(
	struct asBlock *curBlock,   /* Block who's predecessor we seek */
	int dim,		    /* Dimension level of tree splits on. */
	struct asBranch *branch,    /* Subtree to explore */
	struct predScore bestSoFar) /* Best predecessor so far. */
{
struct asBlock *leaf;

/* If best score in this branch of tree wouldn't be enough
 * (even without gaps) don't bother exploring it. */
if (branch->maxScore + curBlock->score < bestSoFar.score)
    return bestSoFar;

/* If it's a leaf branch, then calculate score to connect
 * with it. */
else if ((leaf = branch->block) != NULL)
    {
    if (leaf->qStart < curBlock->qStart && leaf->tStart < curBlock->tStart)
	{
	int score = leaf->totalScore + curBlock->score - gap(leaf, curBlock);
	if (score > bestSoFar.score)
	   {
	   bestSoFar.score = score;
	   bestSoFar.pred = branch;
	   }
	}
    return bestSoFar;
    }

/* Otherwise explore sub-trees that could harbor predecessors. */
else
    {
    int newDim = 1-dim;
    int dimCoord = (dim == 0 ? curBlock->qStart : curBlock->tStart);
    
    /* Explore hi branch first as it is more likely to have high
     * scores.  However only explore it if it can have things starting
     * before us. */
    if (dimCoord > branch->cutCoord)
         bestSoFar = bestPredecessor(curBlock, newDim, branch->hi, bestSoFar);
    bestSoFar = bestPredecessor(curBlock, newDim, branch->lo, bestSoFar);
    return bestSoFar;
    }
}

void updateScoresOnWay(struct asBranch *branch, int dim, struct asBlock *block)
/* Traverse kd-tree to find block.  Update all maxScores on the way
 * to reflect block->totalScore. */
{
int newDim = 1-dim;
int dimCoord = (dim == 0 ? block->qStart : block->tStart);
if (branch->maxScore < block->totalScore) branch->maxScore = block->totalScore;
if (branch->block == NULL)
    {
    if (dimCoord <= branch->cutCoord)
	updateScoresOnWay(branch->lo, newDim, block);
    if (dimCoord >= branch->cutCoord)
	updateScoresOnWay(branch->hi, newDim, block);
    }
}


struct asChain *asChainFind(struct asTree *tree, struct asBlock *blockList)
/* Find highest scoring chain in tree. */
{
static struct predScore noBest;
struct asBlock *block;
struct asBlock *bestBlock = NULL;
int bestScore = 0;
struct asChain *chain;

for (block = blockList; block != NULL; block = block->next)
    {
    struct predScore best = bestPredecessor(block, 0, tree->root, noBest);
    if (best.score > block->totalScore)
        {
	block->totalScore = best.score;
	block->bestPred = best.pred;
	}
    updateScoresOnWay(tree->root, 0, block);
    if (bestScore < block->totalScore)
        {
	bestScore = block->totalScore;
	bestBlock = block;
	}
    }
AllocVar(chain);
chain->score = bestScore;
for (block = bestBlock; ; )
    {
    struct asBranch *pred;
    slAddHead(&chain->blockList, block);
    pred = block->bestPred;
    if (pred == NULL)
        break;
    block = pred->block;
    }
return chain;
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
struct asChain *chain;

/* Make block list */
for (axt = sp->axtList; axt != NULL; axt = axt->next)
    {
    AllocVar(block);
    block->qStart = axt->qStart;
    block->qEnd = axt->qEnd;
    block->tStart = axt->tStart;
    block->tEnd = axt->tEnd;
    block->score = block->totalScore = axt->score;
    block->ali = axt;
    slAddHead(&blockList, block);
    }
slSort(&blockList, asBlockCmpT);

/* Make up tree and time it for debugging. */
startTime = clock1000();
tree = asTreeMake(blockList);
// asTreeDump(tree, uglyOut);
dt = clock1000() - startTime;
uglyf("Made tree in %5.3f seconds\n", dt*0.001);

/* Get longest chain from tree and print it out. */
chain = asChainFind(tree, blockList);
if (chain != NULL)
    {
    struct asBlock *first = chain->blockList;
    struct asBlock *last = slLastEl(first);
    fprintf(f, "%s Chain %d, score %d, %d %d, %d %d:\n", 
	sp->name, slCount(chain->blockList), chain->score,
	first->qStart, last->qEnd, first->tStart, last->tEnd);
    for (block = chain->blockList; block != NULL; block = block->next)
        {
	struct axt *axt = block->ali;
	fprintf(f, " %s%c%s %d %d, %d %d, %d\n",
		axt->qName, axt->qStrand, axt->tName, 
		axt->qStart, axt->qEnd, axt->tStart, axt->tEnd,
		axt->score);
	}
    }
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
