/* rangeTree - This module is a way of keeping track of
 * non-overlapping ranges (half-open intervals). It is
 * based on the self-balancing rbTree code.  Use it in
 * place of a bitmap when the total number of ranges
 * is significantly smaller than the number of bits would
 * be. 
 * Beware the several static/global variables which can be
 * changed by various function calls. */

#include "common.h"
#include "limits.h"
#include "localmem.h"
#include "rbTree.h"
#include "rangeTree.h"

static char const rcsid[] = "$Id: rangeTree.c,v 1.21 2008/11/15 07:07:30 markd Exp $";

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


static void *sumInt(void *a, void *b)
/* Local function used by rangeTreeAddValCount, which sums two ints a and b, 
 * referenced by void pointers, returning the result in a */
{
int *i = a, *j = b;
*i += *j;
return a;
}


struct range *rangeTreeAddVal(struct rbTree *tree, int start, int end, void *val, void *(*mergeVals)(void *existingVal, void *newVal) )
/* Add range to tree, merging with existing ranges if need be. 
 * If this is a new range, set the value to this val.
 * If there are existing items for this range, and if mergeVals function is not null, 
 * apply mergeVals to the existing values and this new val, storing the result as the val
 * for this range (see rangeTreeAddValCount() and rangeTreeAddValList() below for examples). */
{
struct range *r, *existing;
r = lmAlloc(tree->lm, sizeof(*r)); /* alloc new zeroed range */
r->start = start;
r->end = end;
r->val = val;
while ((existing = rbTreeRemove(tree, r)) != NULL)
    {
    r->start = min(r->start, existing->start);
    r->end = max(r->end, existing->end);
    if (mergeVals)
	r->val = mergeVals(existing->val, r->val);
    }
rbTreeAdd(tree, r);
return r;
}


struct range *rangeTreeAdd(struct rbTree *tree, int start, int end)
/* Add range to tree, merging with existing ranges if need be. */
{
    return rangeTreeAddVal(tree, start, end, NULL, NULL);
}


struct range *rangeTreeAddValCount(struct rbTree *tree, int start, int end)
/* Add range to tree, merging with existing ranges if need be. 
 * Set range val to count of elements in the range. Counts are pointers to 
 * ints allocated in tree localmem */
{
    int *a = lmAlloc(tree->lm, sizeof(*a)); /* keep the count in localmem */
    *a = 1;
    return rangeTreeAddVal(tree, start, end, (void *)a, sumInt);
}


struct range *rangeTreeAddValList(struct rbTree *tree, int start, int end, void *val)
/* Add range to tree, merging with existing ranges if need be. 
 * Add val to the list of values (if any) in each range.
 * val must be valid argument to slCat (ie, be a struct with a 'next' pointer as its first member) */
{
    return rangeTreeAddVal(tree, start, end, val, slCat);
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
    {
    r->next = NULL; /* this can be set by previous calls to the List functions */
    return r;
    }
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
slReverse(&rangeList);
return rangeList;
}


struct range *rangeTreeMaxOverlapping(struct rbTree *tree, int start, int end)
/* Return item that overlaps most with start-end. Not thread safe.  Trashes list used
 * by rangeTreeAllOverlapping. */
{
struct range *range, *best = NULL;
int bestOverlap = 0; 
for (range  = rangeTreeAllOverlapping(tree, start, end); range != NULL; range = range->next)
    {
    int overlap = rangeIntersection(range->start, range->end, start, end);
    if (overlap > bestOverlap)
        {
	bestOverlap = overlap;
	best = range;
	}
    }
if (best)
    best->next = NULL; /* could be set by calls to List functions */
return best;
}

/* A couple of variables used to calculate total overlap. */
static int totalOverlap;
static int overlapStart, overlapEnd;

static void addOverlap(void *v)
/* Callback to add item to range list. */
{
struct range *r = v;
totalOverlap += positiveRangeIntersection(r->start, r->end, 
	overlapStart, overlapEnd);
}

int rangeTreeOverlapSize(struct rbTree *tree, int start, int end)
/* Return the total size of intersection between interval
 * from start to end, and items in range tree. Sadly not
 * thread-safe. 
 * On 32 bit machines be careful not to overflow
 * range of start, end or total size return value. */
{
struct range tempR;
tempR.start = overlapStart = start;
tempR.end = overlapEnd = end;
totalOverlap = 0;
rbTreeTraverseRange(tree, &tempR, &tempR, addOverlap);
return totalOverlap;
}

int rangeTreeOverlapTotalSize(struct rbTree *tree)
/* Return the total size of all ranges in range tree.
 * Sadly not thread-safe. 
 * On 32 bit machines be careful not to overflow
 * range of start, end or total size return value. */
{
return rangeTreeOverlapSize(tree, INT_MIN, INT_MAX);
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

