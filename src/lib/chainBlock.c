/* chainBlock - Chain together scored blocks from an alignment
 * into scored chains.  Internally this uses a kd-tree and a
 * varient of an algorithm suggested by Webb Miller and further
 * developed by Jim Kent. */

#include "common.h"
#include "localmem.h"
#include "linefile.h"
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
    double maxScore;	      /* Max score of any leaf below us. */
    int maxQ;		      /* Maximum qEnd of any leaf below us. */
    int maxT;		      /* Maximum tEnd of any leaf below us. */
    };

struct kdLeaf
/* A leaf in our kdTree. */
    {
    struct kdLeaf *next;	/* Next in list. */
    struct cBlock *cb;	        /* Start position and score from user. */
    struct kdBranch *bestPred;	/* Best predecessor. */
    double totalScore;		/* Total score of chain up to here. */
    bool hit;			/* This hit? Used by system internally. */
    };

struct kdTree
/* The whole tree.  */
    {
    struct kdBranch *root;	/* Pointer to root of kd-tree. */
    };


static int kdLeafCmpQ(const void *va, const void *vb)
/* Compare to sort based on query start. */
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
double diff = b->totalScore - a->totalScore;
if (diff < 0) return -1;
else if (diff > 0) return 1;
else return 0;
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


static struct kdBranch *kdBuild(int nodeCount, struct dlList *lists[2], int dim,
	struct lm *lm)
/* Build up kd-tree recursively. */
{
struct kdBranch *branch;
lmAllocVar(lm, branch);
if (nodeCount == 1)
    {
    struct kdLeaf *leaf = lists[0]->head->val;
    branch->leaf = leaf;
    branch->maxQ = leaf->cb->qEnd;
    branch->maxT = leaf->cb->tEnd;
    }
else
    {
    int newCount;
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
    branch->lo = kdBuild(nodeCount - newCount, lists, nextDim, lm);
    branch->hi = kdBuild(newCount, newLists, nextDim, lm);
    branch->maxQ = max(branch->lo->maxQ, branch->hi->maxQ);
    branch->maxT = max(branch->lo->maxT, branch->hi->maxT);
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
/* Just sort qList since tList is sorted because it was
 * constructed from sorted leafList. */
dlSort(&qList, kdLeafCmpQ); 
lists[0] = &qList;
lists[1] = &tList;

/* Allocate master data structure and call recursive builder. */
lmAllocVar(lm, tree);
tree->root = kdBuild(nodeCount, lists, 0, lm);

/* Clean up and go home. */
freeMem(qNodes);
freeMem(tNodes);
return tree;
}

struct predScore
/* Predecessor and score we get merging with it. */
    {
    struct kdBranch *pred;	/* Predecessor. */
    double score;		/* Score of us plus predecessor. */
    };

static struct predScore bestPredecessor(
	struct kdLeaf *lonely,	    /* We're finding this leaf's predecessor */
	ConnectCost connectCost,    /* Cost to connect two leafs. */
	GapCost gapCost,	    /* Lower bound on gap cost. */
	void *gapData,		    /* Data to pass to Gap/Connect cost */
	int dim,		    /* Dimension level of tree splits on. */
	struct kdBranch *branch,    /* Subtree to explore */
	struct predScore bestSoFar) /* Best predecessor so far. */
/* Find the highest scoring predecessor to this leaf, and
 * thus iteratively the highest scoring subchain that ends
 * in this leaf. */
{
struct kdLeaf *leaf;
double maxScore = branch->maxScore + lonely->cb->score;

/* If best score in this branch of tree wouldn't be enough
 * don't bother exploring it. First try without calculating
 * gap score in case gap score is a little expensive to calculate. */
if (maxScore < bestSoFar.score)
    return bestSoFar;
maxScore -= gapCost(lonely->cb->qStart - branch->maxQ, 
	lonely->cb->tStart - branch->maxT, gapData);
if (maxScore < bestSoFar.score)
    return bestSoFar;


/* If it's a terminal branch, then calculate score to connect
 * with it. */
else if ((leaf = branch->leaf) != NULL)
    {
    if (leaf->cb->qStart < lonely->cb->qStart 
     && leaf->cb->tStart < lonely->cb->tStart)
	{
	double score = leaf->totalScore + lonely->cb->score - 
		connectCost(leaf->cb, lonely->cb, gapData);
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
         bestSoFar = bestPredecessor(lonely, connectCost, gapCost, gapData,
	 	newDim, branch->hi, bestSoFar);
    bestSoFar = bestPredecessor(lonely, connectCost, gapCost, gapData,
    	newDim, branch->lo, bestSoFar);
    return bestSoFar;
    }
}

static void updateScoresOnWay(struct kdBranch *branch, 
	int dim, struct kdLeaf *leaf)
/* Traverse kd-tree to find leaf.  Update all maxScores on the way
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

static void findBestPredecessors(struct kdTree *tree, struct kdLeaf *leafList, 
	ConnectCost connectCost, GapCost gapCost, void *gapData)
/* Find best predecessor for each leaf. */
{
static struct predScore noBest;
struct kdLeaf *leaf;
double bestScore = 0;

for (leaf = leafList; leaf != NULL; leaf = leaf->next)
    {
    struct predScore best;
    best = bestPredecessor(leaf, connectCost, gapCost, gapData, 0, tree->root, noBest);
    if (best.score > leaf->totalScore)
        {
	leaf->totalScore = best.score;
	leaf->bestPred = best.pred;
	}
    updateScoresOnWay(tree->root, 0, leaf);
    if (bestScore < leaf->totalScore)
        {
	bestScore = leaf->totalScore;
	}
    }
}

static double scoreBlocks(struct cBlock *blockList, ConnectCost connectCost,
	void *gapData)
/* Score list of blocks including gaps between blocks. */
{
struct cBlock *block, *lastBlock = NULL;
double score = 0;
for (block = blockList; block != NULL; block = block->next)
    {
    score += block->score;
    if (lastBlock != NULL)
	score -= connectCost(lastBlock, block, gapData);
    lastBlock = block;
    }
return score;
}

static struct chain *peelChains(char *qName, int qSize, char qStrand,
	char *tName, int tSize, struct kdLeaf *leafList, FILE *details)
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
	chain->qName = cloneString(qName);
	chain->qSize = qSize;
	chain->qStrand = qStrand;
	chain->tName = cloneString(tName);
	chain->tSize = tSize;
	chain->qEnd = leaf->cb->qEnd;
	chain->tEnd = leaf->cb->tEnd;
	if (details)
	    {
	    chain->score = leaf->totalScore;
	    chain->id = -1;
	    chainWriteHead(chain, details);
	    chain->score = 0;
	    chain->id = 0;
	    }
	slAddHead(&chainList, chain);
	for (lf = leaf; ; )
	    {
	    lf->hit = TRUE;
	    slAddHead(&chain->blockList, lf->cb);
	    chain->qStart = lf->cb->qStart;
	    chain->tStart = lf->cb->tStart;
	    if (details)
	        {
		struct cBlock *b = lf->cb;
		fprintf(details, "%d\t%f\t%d\t%d\t%d\n", b->score, lf->totalScore,
			b->tStart, b->qStart, b->qEnd - b->qStart);
		}
	    if (lf->bestPred == NULL)
	         break;
	    else
	        {
		if (details)
		    {
		    struct cBlock *b = lf->cb;
		    struct cBlock *a = lf->bestPred->leaf->cb;
		    fprintf(details, " gap %d\t%d\n", 
			b->tStart - a->tEnd, b->qStart - a->qEnd);
		    }
		}
	    lf = lf->bestPred->leaf;
	    if (lf->hit)
	        break;
	    }
	}
    }
slReverse(&chainList);
return chainList;
}

struct chain *chainBlocks(
	char *qName, int qSize, char qStrand,	/* Info on query sequence */
	char *tName, int tSize, 		/* Info on target. */
	struct cBlock **pBlockList, 		/* Unordered ungapped alignments. */
	ConnectCost connectCost, 		/* Calculate cost to connect nodes. */
	GapCost gapCost, 			/* Cost for non-overlapping nodes. */
	void *gapData, 				/* Passed through to connect/gapCosts */
	FILE *details)				/* NULL except for debugging */
/* Create list of chains from list of blocks.  The blockList will get
 * eaten up as the blocks are moved from the list to the chain. 
 * The list of chains returned is sorted by score. 
 *
 * The details FILE may be NULL, and is where additional information
 * about the chaining is put.
 *
 * Note that the connectCost needs to adjust for possibly partially 
 * overlapping blocks, and that these need to be taken out of the
 * resulting chains in general.  This can get fairly complex.  Also
 * the chains will need some cleanup at the end.  Use the chainConnect
 * module to help with this.  See hg/mouseStuff/axtChain for example usage. */
{
struct kdTree *tree;
struct kdLeaf *leafList = NULL, *leaf;
struct cBlock *block;
struct chain *chainList = NULL, *chain;
struct lm *lm;

/* Empty lists will be problematic later, so deal with them here. */
if (*pBlockList == NULL)
   return NULL;

/* Make a leaf for each block. */
lm = lmInit(0);  /* Memory for tree, branches and leaves. */
for (block = *pBlockList; block != NULL; block = block->next)
    {
    /* Watch out for 0-length blocks in input: */
    if (block->tStart == block->tEnd)
	continue;
    lmAllocVar(lm, leaf);
    leaf->cb = block;
    leaf->totalScore = block->score;
    slAddHead(&leafList, leaf);
    }

/* Figure out chains. */
slSort(&leafList, kdLeafCmpT);
tree = kdTreeMake(leafList, lm);
findBestPredecessors(tree, leafList, connectCost, gapCost, gapData);
slSort(&leafList, kdLeafCmpTotal);
chainList = peelChains(qName, qSize, qStrand, tName, tSize, leafList, details);

/* Rescore chains (since some truncated) */
for (chain = chainList; chain != NULL; chain = chain->next)
    chain->score = scoreBlocks(chain->blockList, connectCost, gapData);
slSort(&chainList,  chainCmpScore);

/* Clean up and go home. */
lmCleanup(&lm);
*pBlockList = NULL;
return chainList;
}

