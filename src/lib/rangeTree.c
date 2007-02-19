/* rangeTree - This module is a way of keeping track of
 * non-overlapping ranges (half-open intervals). It is
 * based on the self-balancing rbTree code.  Use it in
 * place of a bitmap when the total number of ranges
 * is significantly smaller than the number of bits would
 * be. */

#include "common.h"
#include "localmem.h"
#include "rbTree.h"
#include "rangeTree.h"

int rangeCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct range *a = va;
struct range *b = vb;
if (a->end <= b->start)
    return -1;
else if (b->end <= a->start)
    return 1;
else
    return 0;
}

struct range *rangeTreeAdd(struct rbTree *tree, int start, int end)
/* Add range to tree, merging with existing ranges if need be. */
{
struct range tempR, *existing;
tempR.start = start;
tempR.end = end;
tempR.val = NULL;
while ((existing = rbTreeRemove(tree, &tempR)) != NULL)
     {
     tempR.start = min(tempR.start, existing->start);
     tempR.end = max(tempR.end, existing->end);
     }
struct range *r = lmCloneMem(tree->lm, &tempR, sizeof(tempR));
rbTreeAdd(tree, r);
return r;
}

boolean rangeTreeOverlaps(struct rbTree *tree, int start, int end)
/* Return TRUE if start-end overlaps anything in tree */
{
struct range tempR;
tempR.start = start;
tempR.end = end;
tempR.val = NULL;
return rbTreeFind(tree, &tempR) != NULL;
}

static struct range *rangeList;

static void rangeListAdd(void *v)
/* Callback to add item to range list. */
{
struct range *r = v;
slAddHead(&rangeList, r);
}

struct range *rangeTreeList(struct rbTree *tree)
/* Return list of all ranges in tree in order.  Not thread safe. 
 * No need to free this when done, memory is local to tree. */
{
rangeList = NULL;
rbTreeTraverse(tree, rangeListAdd);
slReverse(&rangeList);
return rangeList;
}

struct range *rangeTreeFindEnclosing(struct rbTree *tree, int start, int end)
/* Find item in range tree that encloses range between start and end 
 * if there is any such item. */
{
struct range tempR, *r;
tempR.start = start;
tempR.end = end;
r = rbTreeFind(tree, &tempR);
if (r != NULL && r->start <= start && r->end >= end)
    return r;
return NULL;
}

struct range *rangeTreeAllOverlapping(struct rbTree *tree, int start, int end)
/* Return list of all items in range tree that overlap interval start-end.
 * Do not free this list, it is owned by tree.  However it is only good until
 * next call to rangeTreeFindInRange or rangTreeList. Not thread safe. */
{
struct range tempR;
tempR.start = start;
tempR.end = end;
rangeList = NULL;
rbTreeTraverseRange(tree, &tempR, &tempR, rangeListAdd);
return rangeList;
}

struct rbTree *rangeTreeNew()
/* Create a new, empty, rangeTree. */
{
return rbTreeNew(rangeCmp);
}

struct rbTree *rangeTreeNewDetailed(struct lm *lm, struct rbTreeNode *stack[128])
/* Allocate rangeTree on an existing local memory & stack.  This is for cases
 * where you want a lot of trees, and don't want the overhead for each one. 
 * Note, to clean these up, just do freez(&rbTree) rather than rbFreeTree(&rbTree). */
{
return rbTreeNewDetailed(rangeCmp, lm, stack);
}

