#include "common.h"
#include "localmem.h"
#include "dlist.h"
#include "chainBlock.h"

struct kdBranch
/* A kd-tree. That is a binary tree which partitions the children
 * into higher and lower one dimension at a time.  We're just doing
 * one in two dimensions, so it alternates between q and t dimensions. */
    {
    struct kdBranch *lo;      /* Pointer to children with lower coordinates. */
    struct kdBranch *hi;      /* Pointer to children with higher coordinates. */
    struct kdLeaf *leaf;      /* Extra info for leaves on tree. */
    int cutCoord;	      /* Coordinate (in some dimension) to cut on */
    int maxScore;	      /* Max score of any leaf below us. */
    };

struct kdLeaf
/* A leaf in our kdTree. */
    {
    struct kdLeaf *next;	/* Next in list. */
    struct chainBlock *cb;	/* Start position and score from user. */
    struct kdBranch *bestPred;	/* Best predecessor. */
    int totalScore;		/* Total score of chain up to here. */
    bool hit;			/* This hit? Used by system internally. */
    };

struct kdTree
/* The whole tree.  */
    {
    struct kdBranch *root;	/* Pointer to root of kd-tree. */
    };

void chainFree(struct chain **pChain)
/* Free up a chain. */
{
struct chain *chain = *pChain;
if (chain != NULL)
    {
    slFreeList(&chain->blockList);
    freez(pChain);
    }
}

void chainFreeList(struct chain **pList)
/* Free a list of dynamically allocated chain's */
{
struct chain *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    chainFree(&el);
    }
*pList = NULL;
}

static void indent(FILE *f, int count)
/* Print given number of spaces. */
{
while (--count >= 0)
    fprintf(f, "  ");
}

static void rBranchDump(struct kdBranch *branch, int depth, FILE *f)
/* Dump branches. */
{
int i;
if (branch->leaf)
    {
    struct chainBlock *cb = branch->leaf->cb;
    indent(f, depth+1);
    fprintf(f, "[%d] %d (q), %d (t)\n", 
    	branch->maxScore, cb->qStart, cb->tStart);
    }
else
    {
    rBranchDump(branch->lo, depth+1, f);
    indent(f, depth+1);
    fprintf(f, "%c [%d] %d\n", ((depth&1) == 0 ? 'q' : 't'), 
    	branch->maxScore, branch->cutCoord);
    rBranchDump(branch->hi, depth+1, f);
    }
}

static void kdTreeDump(struct kdTree *tree, FILE *f)
/* Dump out tree to file. */
{
fprintf(f, "treeDump\n");
rBranchDump(tree->root, 0, f);
}

static int kdLeafCmpQ(const void *va, const void *vb)
/* Compare to sort based on target start. */
{
const struct kdLeaf *a = *((struct kdLeaf **)va);
const struct kdLeaf *b = *((struct kdLeaf **)vb);
return a->cb->qStart - b->cb->qStart;
}

static int kdLeafCmpT(const void *va, const void *vb)
/* Compare to sort based on target start. */
{
const struct kdLeaf *a = *((struct kdLeaf **)va);
const struct kdLeaf *b = *((struct kdLeaf **)vb);
return a->cb->tStart - b->cb->tStart;
}

static int kdLeafCmpTotal(const void *va, const void *vb)
/* Compare to sort based on total score. */
{
const struct kdLeaf *a = *((struct kdLeaf **)va);
const struct kdLeaf *b = *((struct kdLeaf **)vb);
return b->totalScore - a->totalScore;
}

static int chainCmpScore(const void *va, const void *vb)
/* Compare to sort based on total score. */
{
const struct chain *a = *((struct chain **)va);
const struct chain *b = *((struct chain **)vb);
return b->score - a->score;
}


static int medianVal(struct dlList *list, int medianIx, int dim)
/* Return value of median block in list on given dimension 
 * Mark blocks up to median as hit. */
{
struct dlNode *node = list->head;
struct kdLeaf *leaf = NULL;
int i;

for (i=0; i<medianIx; ++i)
    {
    leaf = node->val;
    leaf->hit = TRUE;  
    node = node->next;
    }
return (dim == 0 ? leaf->cb->qStart : leaf->cb->tStart);
}

static int splitList(struct dlList *oldList, struct dlList *newList)
/* Peel off members of oldList that are not hit onto new list. */
{
struct dlNode *node, *next;
struct kdLeaf *leaf;
int newCount = 0;
dlListInit(newList);
for (node = oldList->head; !dlEnd(node); node = next)
    {
    next = node->next;
    leaf = node->val;
    if (!leaf->hit)
	{
	dlRemove(node);
	dlAddTail(newList, node);
	++newCount;
	}
    }
return newCount;
}

static void clearHits(struct dlList *list)
/* Clear hit flags of all blocks on list. */
{
struct dlNode *node;
for (node = list->head; !dlEnd(node); node = node->next)
    {
    struct kdLeaf *leaf = node->val;
    leaf->hit = FALSE;
    }
}

static void dumpList(char *name, struct dlList *list)
/* Dump list of blocks. */
{
struct dlNode *node;
uglyf("%s\n", name);
for (node = list->head; !dlEnd(node); node = node->next)
    {
    struct kdLeaf *leaf = node->val;
    uglyf("  %d,%d  %d,%d\n", leaf->cb->qStart,leaf->cb->qEnd, leaf->cb->tStart,leaf->cb->tEnd);
    }
}

static struct lm *rlm; /* Local memory pool for branches etc.  Here just
                       * to avoid passing it through recursion to kdBuild. */

static struct kdBranch *kdBuild(int nodeCount, struct dlList *lists[2], int dim)
/* Build up kd-tree recursively. */
{
struct kdBranch *branch;

// uglyf("kdBuild nodeCount %d\n", nodeCount);
// dumpList(" qList", lists[0]);
// dumpList(" tList", lists[1]);
assert(nodeCount > 0);
lmAllocVar(rlm, branch);
if (nodeCount == 1)
    {
    struct kdLeaf *leaf = lists[0]->head->val;
    branch->leaf = leaf;
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
return branch;
}


static struct kdTree *kdTreeMake(struct kdLeaf *leafList, struct lm *lm)
/* Make a kd-tree containing leafList. */
{
struct kdLeaf *leaf;
int nodeCount = slCount(leafList);
struct kdTree *tree;
struct dlList qList, tList,*lists[2];
struct dlNode *qNodes, *tNodes;
int i;

/* Build lists sorted in each dimension. This
 * will let us quickly find medians while constructing
 * the kd-tree. */
dlListInit(&qList);
dlListInit(&tList);
AllocArray(qNodes, nodeCount);
AllocArray(tNodes, nodeCount);
for (i=0 , leaf=leafList; leaf != NULL; leaf = leaf->next, ++i)
    {
    qNodes[i].val = tNodes[i].val = leaf;
    dlAddTail(&qList, &qNodes[i]);
    dlAddTail(&tList, &tNodes[i]);
    }
dlSort(&qList, kdLeafCmpQ); /* tList sorted since leafList is sorted. */
lists[0] = &qList;
lists[1] = &tList;

/* Allocate master data structure and call recursive builder. */
lmAllocVar(lm, tree);
rlm = lm;
tree->root = kdBuild(nodeCount, lists, 0);

/* Clean up and go home. */
freeMem(qNodes);
freeMem(tNodes);
return tree;
}

struct predScore
/* Predecessor and score we get merging with it. */
    {
    struct kdBranch *pred;	/* Predecessor. */
    int score;			/* Score of us plus predecessor. */
    };

/* These next two are parameters for bestPredecessor that
 * do not change during recursion. */
static ConnectCost cCost;	/* Connection cost used by bestPredecessor. */
static struct kdLeaf *lonely;   /* Leaf who's predecessor we look for. */

static struct predScore bestPredecessor(
	int dim,		    /* Dimension level of tree splits on. */
	struct kdBranch *branch,    /* Subtree to explore */
	struct predScore bestSoFar) /* Best predecessor so far. */
{
struct kdLeaf *leaf;

/* If best score in this branch of tree wouldn't be enough
 * (even without gaps) don't bother exploring it. */
if (branch->maxScore + lonely->cb->score < bestSoFar.score)
    return bestSoFar;

/* If it's a terminal branch, then calculate score to connect
 * with it. */
else if ((leaf = branch->leaf) != NULL)
    {
    if (leaf->cb->qStart < lonely->cb->qStart 
     && leaf->cb->tStart < lonely->cb->tStart)
	{
	int score = leaf->totalScore + lonely->cb->score - 
		cCost(leaf->cb, lonely->cb);
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
    int dimCoord = (dim == 0 ? lonely->cb->qStart : lonely->cb->tStart);
    
    /* Explore hi branch first as it is more likely to have high
     * scores.  However only explore it if it can have things starting
     * before us. */
    if (dimCoord > branch->cutCoord)
         bestSoFar = bestPredecessor(newDim, branch->hi, bestSoFar);
    bestSoFar = bestPredecessor(newDim, branch->lo, bestSoFar);
    return bestSoFar;
    }
}

static void updateScoresOnWay(struct kdBranch *branch, int dim, struct kdLeaf *leaf)
/* Traverse kd-tree to find block.  Update all maxScores on the way
 * to reflect leaf->totalScore. */
{
int newDim = 1-dim;
int dimCoord = (dim == 0 ? leaf->cb->qStart : leaf->cb->tStart);
if (branch->maxScore < leaf->totalScore) branch->maxScore = leaf->totalScore;
if (branch->leaf == NULL)
    {
    if (dimCoord <= branch->cutCoord)
	updateScoresOnWay(branch->lo, newDim, leaf);
    if (dimCoord >= branch->cutCoord)
	updateScoresOnWay(branch->hi, newDim, leaf);
    }
}


static void connectLeaves(struct kdTree *tree, struct kdLeaf *leafList, 
	ConnectCost connectCost)
/* Find highest scoring chains in tree. */
{
static struct predScore noBest;
struct kdLeaf *leaf;
struct kdLeaf *bestLeaf = NULL;
int bestScore = 0;

for (leaf = leafList; leaf != NULL; leaf = leaf->next)
    {
    struct predScore best;
    
    lonely = leaf;
    cCost = connectCost;
    best = bestPredecessor(0, tree->root, noBest);
    if (best.score > leaf->totalScore)
        {
	leaf->totalScore = best.score;
	leaf->bestPred = best.pred;
	}
    updateScoresOnWay(tree->root, 0, leaf);
    if (bestScore < leaf->totalScore)
        {
	bestScore = leaf->totalScore;
	bestLeaf = leaf;
	}
    }
}

static int scoreBlocks(struct chainBlock *blockList, ConnectCost connectCost)
/* Score list of blocks including gaps between blocks. */
{
struct chainBlock *block, *lastBlock = NULL;
int score = 0;
for (block = blockList; block != NULL; block = block->next)
    {
    score += block->score;
    if (lastBlock != NULL)
	score -= connectCost(lastBlock, block);
    lastBlock = block;
    }
return score;
}

static struct chain *peelChains(struct kdLeaf *leafList)
/* Peel off all chains from tree implied by
 * best predecessors. */
{
struct kdLeaf *leaf;
struct chain *chainList = NULL, *chain;
for (leaf = leafList; leaf != NULL; leaf = leaf->next)
    leaf->hit = FALSE;
for (leaf = leafList; leaf != NULL; leaf = leaf->next)
    {
    if (!leaf->hit)
        {
	struct kdLeaf *lf;
	AllocVar(chain);
	slAddHead(&chainList, chain);
	for (lf = leaf; ; )
	    {
	    lf->hit = TRUE;
	    slAddHead(&chain->blockList, lf->cb);
	    if (lf->bestPred == NULL)
	         break;
	    lf = lf->bestPred->leaf;
	    if (lf->hit)
	        break;
	    }
	}
    }
return chainList;
}

struct chain *chainBlocks(struct chainBlock **pBlockList, ConnectCost connectCost)
/* Create list of chains from list of blocks.  The blockList will get
 * eaten up as the blocks are moved from the list to the chain. */
{
struct kdTree *tree;
struct kdLeaf *leafList = NULL, *leaf;
struct chainBlock *block;
struct chain *chainList = NULL, *chain;
struct lm *lm = lmInit(0);  /* Memory for tree, branches and leaves. */

/* Make a leaf for each block. */
for (block = *pBlockList; block != NULL; block = block->next)
    {
    lmAllocVar(lm, leaf);
    leaf->cb = block;
    leaf->totalScore = block->score;
    slAddHead(&leafList, leaf);
    }

/* Figure out chains. */
slSort(&leafList, kdLeafCmpT);
tree = kdTreeMake(leafList, lm);
connectLeaves(tree, leafList, connectCost);
slSort(&leafList, kdLeafCmpTotal);
chainList = peelChains(leafList);

/* Rescore chains (since some truncated) */
for (chain = chainList; chain != NULL; chain = chain->next)
    chain->score = scoreBlocks(chain->blockList, connectCost);
slSort(&chainList,  chainCmpScore);

/* Clean up and go home. */
lmCleanup(&lm);
*pBlockList = NULL;
return chainList;
}

