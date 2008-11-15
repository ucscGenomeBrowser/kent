/* rangeTree - This module is a way of keeping track of
 * non-overlapping ranges (half-open intervals). It is
 * based on the self-balancing rbTree code.  Use it in
 * place of a bitmap when the total number of ranges
 * is significantly smaller than the number of bits would
 * be. */

#ifndef RANGETREE_H
#define RANGETREE_H

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

struct rbTree *rangeTreeNew();
/* Create a new, empty, rangeTree.  Free with rbFreeTree. */

#define rangeTreeFree(a) rbTreeFree(a)
/* Free up range tree.  */

int rangeCmp(void *va, void *vb);
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */


struct range *rangeTreeAddVal(struct rbTree *tree, int start, int end, void *val, void *(*mergeVals)(void *existingVal, void *newVal) );
/* Add range to tree, merging with existing ranges if need be. 
 * If this is a new range, set the value to this val.
 * If there are existing items for this range, and if mergeVals function is not null, 
 * apply mergeVals to the existing values and this new val, storing the result as the val
 * for this range (see rangeTreeAddValCount() and rangeTreeAddValList() below for examples). */

struct range *rangeTreeAdd(struct rbTree *tree, int start, int end);
/* Add range to tree, merging with existing ranges if need be. */

struct range *rangeTreeAddValCount(struct rbTree *tree, int start, int end);
/* Add range to tree, merging with existing ranges if need be. 
 * Set range val to count of elements in the range. Counts are pointers to 
 * ints allocated in tree localmem */

struct range *rangeTreeAddValList(struct rbTree *tree, int start, int end, void *val);
/* Add range to tree, merging with existing ranges if need be. 
 * Add val to the list of values (if any) in each range.
 * val must be valid argument to slCat (ie, be a struct with a 'next' pointer as its first member) */

boolean rangeTreeOverlaps(struct rbTree *tree, int start, int end);
/* Return TRUE if start-end overlaps anything in tree */

int rangeTreeOverlapSize(struct rbTree *tree, int start, int end);
/* Return the total size of intersection between interval
 * from start to end, and items in range tree. Sadly not
 * thread-safe.
 * On 32 bit machines be careful not to overflow
 * range of start, end or total size return value. */

int rangeTreeOverlapTotalSize(struct rbTree *tree);
/* Return the total size of all ranges in range tree.
 * Sadly not thread-safe. 
 * On 32 bit machines be careful not to overflow
 * range of start, end or total size return value. */

struct range *rangeTreeFindEnclosing(struct rbTree *tree, int start, int end);
/* Find item in range tree that encloses range between start and end 
 * if there is any such item. */

struct range *rangeTreeAllOverlapping(struct rbTree *tree, int start, int end);
/* Return list of all items in range tree that overlap interval start-end.
 * Do not free this list, it is owned by tree.  However it is only good until
 * next call to rangeTreeFindInRange or rangTreeList. Not thread safe. */

struct range *rangeTreeMaxOverlapping(struct rbTree *tree, int start, int end);
/* Return item that overlaps most with start-end. Not thread safe.  Trashes list used
 * by rangeTreeAllOverlapping. */

struct range *rangeTreeList(struct rbTree *tree);
/* Return list of all ranges in tree in order.  Not thread safe. 
 * No need to free this when done, memory is local to tree. */

struct rbTree *rangeTreeNewDetailed(struct lm *lm, struct rbTreeNode *stack[128]);
/* Allocate rangeTree on an existing local memory & stack.  This is for cases
 * where you want a lot of trees, and don't want the overhead for each one. 
 * Note, to clean these up, just do freez(&rbTree) rather than rbFreeTree(&rbTree). */

#endif /* RANGETREE_H */

