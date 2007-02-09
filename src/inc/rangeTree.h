/* rangeTree - This module is a way of keeping track of
 * non-overlapping ranges (half-open intervals). It is
 * based on the self-balancing rbTree code.  Use it in
 * place of a bitmap when the total number of ranges
 * is significantly smaller than the number of bits would
 * be. */

#ifndef RBTREE_H
#include "rbTree.h"
#endif

struct range
/* An interval in a list of intervals. */
    {
    struct range *next;
    int start,end;	/* Zero based half open interval. */
    void *val;		/* Some value associated with range. */
    };

int rangeCmp(void *va, void *vb);
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */

void rangeTreeAdd(struct rbTree *tree, int start, int end);
/* Add range to tree, merging with existing ranges if need be. */

struct range *rangeTreeFindEnclosing(struct rbTree *tree, int start, int end);
/* Find item in range tree that encloses range between start and end 
 * if there is any such item. */

struct range *rangeTreeList(struct rbTree *tree);
/* Return list of all ranges in tree in order.  Not thread safe. 
 * No need to free this when done, memory is local to tree. */

struct rbTree *rangeTreeNew();
/* Create a new, empty, rangeTree. */
