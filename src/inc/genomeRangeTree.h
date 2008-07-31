/* genomeRangeTree - This module is a way of keeping track of
 * non-overlapping ranges (half-open intervals) across a whole
 * genome (multiple chromosomes or scaffolds). 
 * It is a hash table container mapping chrom to rangeTree.
 * Most of the work is performed by rangeTree, this container
 * enables local memory and stack to be shared by many rangeTrees
 * so it should be able to handle genomes with a very large 
 * number of scaffolds. See rangeTree for more information. */

#ifndef GENOMERANGETREE_H
#define GENOMERANGETREE_H

#ifndef HASH_H
#include "hash.h"
#endif

#ifndef RANGETREE_H
#include "rangeTree.h"
#endif

struct genomeRangeTree
/* A structure that allows efficient random access of ranges of bases covered
 * by a particular feature genome-wide.  Implemented as a range tree for each
 * scaffold or chromosome in a genome. */
    {
    struct genomeRangeTree *next;  /* Next genomeRangeTree in list if any. */
    struct hash *hash;             /* Hash of rangeTrees keyed by chromosome/scaffold name. */
    struct rbTreeNode *stack[128]; /* Stack for binary search. */
    struct lm *lm;                 /* Local memory pool for tree nodes and ranges. */
    };

struct genomeRangeTree *genomeRangeTreeNew();
/* Create a new, empty, genomeRangeTree. Uses the default hash size.
 * Free with genomeRangeTreeFree. */

struct genomeRangeTree *genomeRangeTreeNewSize(int hashPowerOfTwoSize);
/* Create a new, empty, genomeRangeTree. 
 * Free with genomeRangeTreeFree. */

void genomeRangeTreeFree(struct genomeRangeTree **tree);
/* Free up genomeRangeTree. */

struct rbTree *genomeRangeTreeFindRangeTree(struct genomeRangeTree *tree, char *chrom);
/* Find the rangeTree for this chromosome, if any. Returns NULL if chrom not found.
 * Free with genomeRangeTreeFree. */

struct rbTree *genomeRangeTreeFindOrAddRangeTree(struct genomeRangeTree *tree, char *chrom);
/* Find the rangeTree for this chromosome, or add new chrom and empty rangeTree if not found.
 * Free with genomeRangeTreeFree. */

struct range *genomeRangeTreeAdd(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Add range to tree, merging with existing ranges if need be. 
 * Adds new rangeTree if chrom not found. */

boolean genomeRangeTreeOverlaps(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Return TRUE if start-end overlaps anything in tree. */

int genomeRangeTreeOverlapSize(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Return the total size of intersection between interval
 * from start to end, and items in range tree. Sadly not
 * thread-safe. */

struct range *genomeRangeTreeFindEnclosing(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Find item in range tree that encloses range between start and end 
 * if there is any such item. */

struct range *genomeRangeTreeAllOverlapping(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Return list of all items in range tree that overlap interval start-end.
 * Do not free this list, it is owned by tree.  However it is only good until
 * next call to rangeTreeFindInRange or rangeTreeList. Not thread safe. */

struct range *genomeRangeTreeMaxOverlapping(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Return item that overlaps most with start-end. Not thread safe.  Trashes list used
 * by rangeTreeAllOverlapping. */

struct range *genomeRangeTreeList(struct genomeRangeTree *tree, char *chrom);
/* Return list of all ranges in single rangeTree in order.  Not thread safe. 
 * No need to free this when done, memory is local to tree. */

#endif /* GENOMERANGETREE_H */

