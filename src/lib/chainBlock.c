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
    int maxScore;	      /* Max score of any leaf below us. */
    int maxQ;		      /* Maximum qEnd of any leaf below us. */
    int maxT;		      /* Maximum tEnd of any leaf below us. */
    };

struct kdLeaf
/* A leaf in our kdTree. */
    {
    struct kdLeaf *next;	/* Next in list. */
    struct boxIn *cb;	        /* Start position and score from user. */
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
    freeMem(chain->qName);
    freeMem(chain->tName);
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

int chainCmpScore(const void *va, const void *vb)
/* Compare to sort based on total score. */
{
const struct chain *a = *((struct chain **)va);
const struct chain *b = *((struct chain **)vb);
double diff = b->score - a->score;
if (diff < 0)
   return -1;
else if (diff > 0)
   return 1;
else
   return 0;
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
dlSort(&qList, kdLeafCmpQ); /* tList sorted since leafList is sorted. */
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
    int score;			/* Score of us plus predecessor. */
    };

static struct predScore bestPredecessor(
	struct kdLeaf *lonely,	    /* We're finding this leaf's predecessor */
	ConnectCost connectCost,    /* Cost to connect two leafs. */
	GapCost gapCost,	    /* Lower bound on gap cost. */
	int dim,		    /* Dimension level of tree splits on. */
	struct kdBranch *branch,    /* Subtree to explore */
	struct predScore bestSoFar) /* Best predecessor so far. */
{
struct kdLeaf *leaf;
int maxScore = branch->maxScore + lonely->cb->score;

/* If best score in this branch of tree wouldn't be enough
 * don't bother exploring it. First try without calculating
 * gap score in case gap score is a little expensive to calculate. */
if (maxScore < bestSoFar.score)
    return bestSoFar;
#ifdef SOON
#endif /* SOON */
maxScore -= gapCost(lonely->cb->qStart - branch->maxQ, 
	lonely->cb->tStart - branch->maxT);
if (maxScore < bestSoFar.score)
    return bestSoFar;


/* If it's a terminal branch, then calculate score to connect
 * with it. */
else if ((leaf = branch->leaf) != NULL)
    {
    if (leaf->cb->qStart < lonely->cb->qStart 
     && leaf->cb->tStart < lonely->cb->tStart)
	{
	int score = leaf->totalScore + lonely->cb->score - 
		connectCost(leaf->cb, lonely->cb);
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
         bestSoFar = bestPredecessor(lonely, connectCost, gapCost, newDim, 
	 	branch->hi, bestSoFar);
    bestSoFar = bestPredecessor(lonely, connectCost, gapCost, newDim, 
    	branch->lo, bestSoFar);
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

static void findBestPredecessors(struct kdTree *tree, struct kdLeaf *leafList, 
	ConnectCost connectCost, GapCost gapCost)
/* Find best predecessor for each block. */
{
static struct predScore noBest;
struct kdLeaf *leaf;
struct kdLeaf *bestLeaf = NULL;
int bestScore = 0;

for (leaf = leafList; leaf != NULL; leaf = leaf->next)
    {
    struct predScore best;
    best = bestPredecessor(leaf, connectCost, gapCost, 0, tree->root, noBest);
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

static double scoreBlocks(struct boxIn *blockList, ConnectCost connectCost)
/* Score list of blocks including gaps between blocks. */
{
struct boxIn *block, *lastBlock = NULL;
double score = 0;
for (block = blockList; block != NULL; block = block->next)
    {
    score += block->score;
    if (lastBlock != NULL)
	score -= connectCost(lastBlock, block);
    lastBlock = block;
    }
return score;
}

static struct chain *peelChains(char *qName, int qSize, char qStrand,
	char *tName, int tSize, struct kdLeaf *leafList)
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
	slAddHead(&chainList, chain);
	for (lf = leaf; ; )
	    {
	    lf->hit = TRUE;
	    slAddHead(&chain->blockList, lf->cb);
	    chain->qStart = lf->cb->qStart;
	    chain->tStart = lf->cb->tStart;
	    if (lf->bestPred == NULL)
	         break;
	    lf = lf->bestPred->leaf;
	    if (lf->hit)
	        break;
	    }
	}
    }
slReverse(&chainList);
return chainList;
}

struct chain *chainBlocks(char *qName, int qSize, char qStrand,
	char *tName, int tSize, struct boxIn **pBlockList, 
	ConnectCost connectCost, GapCost gapCost)
/* Create list of chains from list of blocks.  The blockList will get
 * eaten up as the blocks are moved from the list to the chain. 
 * The chain returned is sorted by score. 
 *
 * Note that the connectCost needs to adjust for possibly partially 
 * overlapping blocks, and that these need to be taken out of the
 * resulting chains in general.  See src/hg/axtChain for an example
 * of coping with this. */
{
struct kdTree *tree;
struct kdLeaf *leafList = NULL, *leaf;
struct boxIn *block;
struct chain *chainList = NULL, *chain;
struct lm *lm;

/* Empty lists will be problematic later, so deal with them here. */
if (*pBlockList == NULL)
   return NULL;

/* Make a leaf for each block. */
lm = lmInit(0);  /* Memory for tree, branches and leaves. */
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
findBestPredecessors(tree, leafList, connectCost, gapCost);
slSort(&leafList, kdLeafCmpTotal);
chainList = peelChains(qName, qSize, qStrand, tName, tSize, leafList);

/* Rescore chains (since some truncated) */
for (chain = chainList; chain != NULL; chain = chain->next)
    chain->score = scoreBlocks(chain->blockList, connectCost);
slSort(&chainList,  chainCmpScore);

/* Clean up and go home. */
lmCleanup(&lm);
*pBlockList = NULL;
return chainList;
}

static int nextId = 1;

void chainIdSet(int id)
/* Set next chain id. */
{
nextId = id;
}

void chainIdReset()
/* Reset chain id. */
{
chainIdSet(1);
}

void chainIdNext(struct chain *chain)
/* Add an id to a chain if it doesn't have one already */
{
chain->id = nextId++;
}

void chainWrite(struct chain *chain, FILE *f)
/* Write out chain to file. */
{
struct boxIn *b, *nextB;
static int id = 1;

if (chain->id == 0)
    chainIdNext(chain);
fprintf(f, "chain %1.0f %s %d + %d %d %s %d %c %d %d %d\n", chain->score,
    chain->tName, chain->tSize, chain->tStart, chain->tEnd,
    chain->qName, chain->qSize, chain->qStrand, chain->qStart, chain->qEnd,
    chain->id);
for (b = chain->blockList; b != NULL; b = nextB)
    {
    nextB = b->next;
    fprintf(f, "%d", b->qEnd - b->qStart);
    if (nextB != NULL)
	fprintf(f, "\t%d\t%d", 
		nextB->tStart - b->tEnd, nextB->qStart - b->qEnd);
    fputc('\n', f);
    }
fputc('\n', f);
}

struct chain *chainRead(struct lineFile *lf)
/* Read next chain from file.  Return NULL at EOF. 
 * Note that chain block scores are not filled in by
 * this. */
{
char *row[13];
int wordCount;
struct chain *chain;
int q,t;
static int id = 0;


wordCount = lineFileChop(lf, row);
if (wordCount == 0)
    return NULL;
if (wordCount < 12)
    errAbort("Expecting at least 12 words line %d of %s", 
    	lf->lineIx, lf->fileName);
if (!sameString(row[0], "chain"))
    errAbort("Expecting 'chain' line %d of %s", lf->lineIx, lf->fileName);
AllocVar(chain);
chain->score = atof(row[1]);
chain->tName = cloneString(row[2]);
chain->tSize = lineFileNeedNum(lf, row, 3);
if (wordCount >= 13)
    chain->id = lineFileNeedNum(lf, row, 12);
else
    chainIdNext(chain);

/* skip tStrand for now, always implicitly + */
chain->tStart = lineFileNeedNum(lf, row, 5);
chain->tEnd = lineFileNeedNum(lf, row, 6);
chain->qName = cloneString(row[7]);
chain->qSize = lineFileNeedNum(lf, row, 8);
chain->qStrand = row[9][0];
chain->qStart = lineFileNeedNum(lf, row, 10);
chain->qEnd = lineFileNeedNum(lf, row, 11);
if (chain->qStart >= chain->qEnd || chain->tStart >= chain->tEnd)
    errAbort("End before start line %d of %s", lf->lineIx, lf->fileName);
if (chain->qStart < 0 || chain->tStart < 0)
    errAbort("Start before zero line %d of %s", lf->lineIx, lf->fileName);
if (chain->qEnd > chain->qSize || chain->tEnd > chain->tSize)
    errAbort("Past end of sequence line %d of %s", lf->lineIx, lf->fileName);

/* Now read in block list. */
q = chain->qStart;
t = chain->tStart;
for (;;)
    {
    int wordCount = lineFileChop(lf, row);
    int size = lineFileNeedNum(lf, row, 0);
    struct boxIn *b;
    AllocVar(b);
    slAddHead(&chain->blockList, b);
    b->qStart = q;
    b->tStart = t;
    q += size;
    t += size;
    b->qEnd = q;
    b->tEnd = t;
    if (wordCount == 1)
        break;
    else if (wordCount < 3)
        errAbort("Expecting 1 or 3 words line %d of %s\n", 
		lf->lineIx, lf->fileName);
    t += lineFileNeedNum(lf, row, 1);
    q += lineFileNeedNum(lf, row, 2);
    }
if (q != chain->qEnd)
    errAbort("q end mismatch %d vs %d line %d of %s\n", 
    	q, chain->qEnd, lf->lineIx, lf->fileName);
if (t != chain->tEnd)
    errAbort("t end mismatch %d vs %d line %d of %s\n", 
    	t, chain->tEnd, lf->lineIx, lf->fileName);
slReverse(&chain->blockList);
return chain;
}

