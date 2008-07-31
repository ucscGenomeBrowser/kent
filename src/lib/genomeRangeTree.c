/* genomeRangeTree - This module is a way of keeping track of
 * non-overlapping ranges (half-open intervals) across a whole
 * genome (multiple chromosomes or scaffolds). 
 * It is a hash table container mapping chrom to rangeTree.
 * Most of the work is performed by rangeTree, this container
 * enables local memory and stack to be shared by many rangeTrees
 * so it should be able to handle genomes with a very large 
 * number of scaffolds. See rangeTree for more information. */

#include "common.h"
#include "localmem.h"
#include "rbTree.h"
#include "hash.h"
#include "rangeTree.h"
#include "genomeRangeTree.h"


struct rbTree *genomeRangeTreeFindRangeTree(struct genomeRangeTree *tree, char *chrom)
/* Find the rangeTree for this chromosome, if any. Returns NULL if chrom not found.
 * Free with genomeRangeTreeFree. */
{
return (struct rbTree *)hashFindVal(tree->hash, chrom);
}

struct rbTree *genomeRangeTreeFindOrAddRangeTree(struct genomeRangeTree *tree, char *chrom)
/* Find the rangeTree for this chromosome, or add new chrom and empty rangeTree if not found.
 * Free with genomeRangeTreeFree. */
{
struct hashEl *hel;
hel = hashStore(tree->hash, chrom);
if (hel->val == NULL) /* need to add a new rangeTree */
    hel->val = (void *)rangeTreeNewDetailed(tree->lm, tree->stack);
return (struct rbTree *)hel->val;
}

struct range *genomeRangeTreeAdd(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Add range to tree, merging with existing ranges if need be. 
 * Adds new rangeTree if chrom not found. */
{
return rangeTreeAdd(genomeRangeTreeFindOrAddRangeTree(tree,chrom), start, end);
}

boolean genomeRangeTreeOverlaps(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Return TRUE if start-end overlaps anything in tree */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeOverlaps(t, start, end) : FALSE;
}

struct range *genomeRangeTreeFindEnclosing(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Find item in range tree that encloses range between start and end 
 * if there is any such item. */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeFindEnclosing(t, start, end) : NULL;
}

struct range *genomeRangeTreeAllOverlapping(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Return list of all items in range tree that overlap interval start-end.
 * Do not free this list, it is owned by tree.  However it is only good until
 * next call to rangeTreeFindInRange or rangeTreeList. Not thread safe. */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeAllOverlapping(t, start, end) : NULL;
}

struct range *genomeRangeTreeMaxOverlapping(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Return item that overlaps most with start-end. Not thread safe.  Trashes list used
 * by rangeTreeAllOverlapping. */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeMaxOverlapping(t, start, end) : NULL;
}

int genomeRangeTreeOverlapSize(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Return the total size of intersection between interval
 * from start to end, and items in range tree. Sadly not
 * thread-safe. */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeOverlapSize(t, start, end) : 0;
}

struct range *genomeRangeTreeList(struct genomeRangeTree *tree, char *chrom)
/* Return list of all ranges in single rangeTree in order.  Not thread safe. 
 * No need to free this when done, memory is local to tree. */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeList(t) : NULL;
}

struct genomeRangeTree *genomeRangeTreeNew()
/* Create a new, empty, genomeRangeTree. Uses the default hash size.
 * Free with genomeRangeTreeFree. */
{
return genomeRangeTreeNewSize(0);
}

struct genomeRangeTree *genomeRangeTreeNewSize(int hashPowerOfTwoSize)
/* Create a new, empty, genomeRangeTree. 
 * Free with genomeRangeTreeFree. */
{
struct genomeRangeTree *t;
AllocVar(t); 
t->hash = hashNew(hashPowerOfTwoSize);
t->lm = lmInit(0);
return t;
}

void genomeRangeTreeFree(struct genomeRangeTree **tree)
/* Free up genomeRangeTree.  */
{
lmCleanup(&((*tree)->lm));  /* clean up all the memory for all nodes for all trees */
freeHash(&((*tree)->hash)); /* free the hash table including names (trees are freed by lmCleanup) */
freez(tree);                /* free this */
}

