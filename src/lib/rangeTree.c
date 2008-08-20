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


struct range *rangeTreeAddVal(struct rbTree *tree, int start, int end, void *val, void *(*mergeVals)(void *existing, void *new) )
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

struct range rangeReadOne(FILE *f, boolean isSwapped)
/* Returns a single range from the file */
{
struct range r = {NULL, 0, 0, NULL};
r.start = readBits32(f, isSwapped);
r.end = r.start + readBits32(f, isSwapped);
return r;
}

void rangeWriteOne(struct range *r, FILE *f)
/* Write out one range structure to binary file f.
 * This only writes start and size. */
{
bits32 start = r->start;
bits32 size = r->end-r->start;
writeOne(f, start);
writeOne(f, size);
}

void rangeReadArray(FILE *f, struct rangeStartSize *r, int n, boolean isSwapped)
/* Read 'n' elements of range array (start,size) from file 'f'. */
{
mustRead(f, r, n*sizeof(*r));
if (isSwapped)
    {
    while (n)
	{
	r->start = byteSwap32(r->start);
	r->size = byteSwap32(r->size);
	--n;
	++r;
	}
    }
}

void rangeWriteArray(struct rangeStartSize *r, int n, FILE *f)
/* Write 'n' elements of range array (start,size) to file 'f'. */
{
mustWrite(f, r, n*sizeof(*r));
}

unsigned rangeArraySize(struct rangeStartSize *r, int n)
/* calculate the total size of the array */
{
unsigned size = 0;
int i;
for ( i = 0 ; i<n ; ++i )
    {
    size += r[i].size;
    }
return size;
}

void rangeReadWriteN(FILE *inF, int n, boolean isSwapped, FILE *outF)
/* Read 'n' ranges in from file 'inF' and write them to file 'outF'.
 * Reads and writes ranges one at a time. */
{
int i;
struct range r;
for (i=0 ; i<n ; ++i )
    {
    r = rangeReadOne(inF, isSwapped);
    rangeWriteOne(&r, outF);
    }
}

struct range rangeReadOneWithVal(FILE *f, boolean isSwapped, void *(*valReadOne)(FILE *f, boolean isSwapped))
/* Read one range structure from binary file f, including range val.
 * Returns start, end.
 * valWriteOne should point to a function which writes the value (not called if null).
 * Returns val if valWriteOne is not null */
{
struct range r = {NULL, 0, 0, NULL};
r.start = readBits32(f, isSwapped);
r.end = r.start + readBits32(f, isSwapped);
if (valReadOne)
    r.val = valReadOne(f, isSwapped);
return r;
}

void rangeWriteOneWithVal(struct range *r, FILE *f, void (*valWriteOne)(void *val, FILE *f))
/* Write out one range structure to binary file f.
 * Writes start and size.
 * valWriteOne should point to a function which writes the value (not called if null). */
{   
rangeWriteOne(r, f);
if (valWriteOne)
	valWriteOne(r->val, f);
}

/* Some file-globals to be used by rangeTreeWriteFile, rangeTreeWriteFileWithVal, rangeTreeSizeInFileWithVal */
static FILE *tempF = NULL;
static void (*tempFuncWriteNodesWithVal)(void *val, FILE *f) = NULL;
static int tempValSize = 0;
static int (*tempFuncRangeValSize)(void *val) = NULL;

static void rangeWriteFile(void *item)
/* Utility function to pass file handle. See rangeTreeWriteOne() below.
 * Not thread-safe. */
{
rangeWriteOne(item, tempF);
}

static void rangeWriteFileWithVal(void *item)
/* Utility function to pass file handle and func ptr. See rangeTreeWriteOneWithVal() below.
 * Not thread-safe. */
{
rangeWriteOneWithVal(item, tempF, tempFuncWriteNodesWithVal);
}

static void rangeValSize(void *val) 
/* Utility function to calculate binary file size of range val.
 * Not thread-safe. */
{
tempValSize += tempFuncRangeValSize(val);
}

void rangeTreeReadNodes(FILE *f, struct rbTree *rt, int numNodes, boolean isSwapped)
/* Reads numNodes ranges from the file and adds them to rangeTree rt.
 * Does not read range val.  */
{
struct range r;
int i;
for (i = 0 ; i<numNodes ; ++i)
    {
    r = rangeReadOne(f, isSwapped);
    rangeTreeAdd(rt, r.start, r.end);
    }
}

void rangeTreeWriteNodes(struct rbTree *tree, FILE *f)
/* Write out one rangeTree structure to binary file f. 
 * Note this does not include the name, which is stored only in index. 
 * Ranges are written in start sequence (depth-first tree traversal).
 * Writes start and size but not val. 
 * Not thread-safe. */
{
tempF = f;
rbTreeTraverse(tree, rangeWriteFile);
}

static void *assignVal(void *existing, void *new)
/* Local helper function to assign range val in case of reading rangeTree 
 * with guaranteed non-overlapping ranges on disk (no merging of values would occur).
 * Simply assignes new value and ignores null existing value.
 * Produces error if existing value is non-null. */
{
if (existing != NULL)
    errAbort("assignVal found existing val (%p) where NULL expected\n", existing);
return new;
}

void rangeTreeReadNodesWithVal(FILE *f, struct rbTree *rt, int numNodes, boolean isSwapped, void *(*valReadOne)(FILE *f, boolean isSwapped))
/* Reads numNodes ranges from the file and adds them to rangeTree rt.
 * If rt contains no nodes, and since rangeTree was saved to disk 
 * implying its ranges are already non-overlapping, it is safe 
 * to use a mergeVal function which simply assigns the stored value 
 * to the range since the existing val must always be null.
 * Produces an error if the rt contains nodes already. */
{
if (rt->n > 0)
    errAbort("Range tree already contains %d ranges. Use rangeTreeReadOneWithValMerge().\n", rt->n);
rangeTreeReadNodesWithValMerge(f, rt, numNodes, isSwapped, valReadOne, assignVal);
}

void rangeTreeReadNodesWithValMerge(FILE *f, struct rbTree *rt, int numNodes, boolean isSwapped, void *(*valReadOne)(FILE *f, boolean isSwapped), void *(*mergeVals)(void *existing, void *new) )
/* Reads numNodes ranges from the file and adds them to rangeTree rt.
 * Reads range values using valReadOne function, if function is non-null.
 * Input rangeTree rt could already have nodes, so mergeVals is called 
 * to merge values read from disk to values in the tree.  */
{
struct range r;
int i;
for (i = 0 ; i<numNodes ; ++i)
    {
    r = rangeReadOneWithVal(f, isSwapped, valReadOne);
    rangeTreeAddVal(rt, r.start, r.end, r.val, mergeVals);
    }
}

void rangeTreeWriteNodesWithVal(struct rbTree *tree, FILE *f, void (*valWriteOne)(void *val, FILE *f))
/* Write out one rangeTree structure to binary file f. 
 * Note this does not include the name, which is stored only in index. 
 * Ranges are written in start sequence (depth-first tree traversal).
 * Writes start and size.
 * valWriteOne should be a function which writes the range val. Not called if null. 
 * Not thread-safe. */
{
tempF = f;
tempFuncWriteNodesWithVal = valWriteOne;
rbTreeTraverse(tree, rangeWriteFileWithVal);
}

int rangeTreeSizeInFile(struct rbTree *tree)
/* Returns size of rangeTree written in binary file format.
 * Includes start and size. 
 * Does not include val. */
{
return sizeof(bits32)*2*tree->n;
}

int rangeTreeSizeInFileWithVal(struct rbTree *tree, int (*rangeValSizeInFile)(void *val))
/* Returns size of rangeTree written in binary file format.
 * Includes start, size, and val. 
 * rangeValSizeInFile should refer to a function which calculates the size of the val 
 * in a binary file. Not called if null. */
{
tempValSize = 0;
tempFuncRangeValSize = rangeValSizeInFile;
rbTreeTraverse(tree, rangeValSize);
return rangeTreeSizeInFile(tree) + tempValSize;
}

unsigned rangeTreeFileIntersection(struct rangeStartSize *r1, struct rangeStartSize *r2, int n1, int n2, struct rangeStartSize **pRange, int *n, boolean saveMem)
/* Create intersection of array of 'n1' ranges (start,size) in r1 with 
 * 'n2' ranges (start,size) in r2, saving them in array r and returning 
 * the number of merged ranges in n.
 * Note that the ranges are as stored on disk (start,size), 
 * not as in the rangeTree (start,end).
 * Free array with freez(&r)
 * Returns total size of ranges in r. If no ranges, returns NULL in r. */
{
int r1end, r2end, i = 0;
unsigned size = 0;
struct rangeStartSize *r = NULL;
int rSize = max(n1,n2); /* max size needed is larger of two */
AllocArray(r, rSize);
/* loop around merging while we have ranges in both lists */
while ( n1 > 0 && n2 > 0)
    {
    r1end = r1->start+r1->size;
    r2end = r2->start+r2->size;
    /* check which should be merged or output */
    if ( r1end <= r2->start ) /* r1 is upstream, get next one */
	{
	--n1;
	++r1;
	}
    else if (r2end <= r1->start) /* r2 is upstream, get next one */
	{
	--n2;
	++r2;
	}
    else 
    /* Overlap: take the max(start) and min(end) as the intersection */
    /* Overlap: keep the one with rightmost end, and extend its start position
     * to the left if need be. Note that the next element from this rangeTree 
     * will not overlap. 
     * Read the next element from the other rangeTree as it may overlap. 
     * If they both end on the same base, output the largest of the two
     * and read in the next from each rangeTree. */
	{ 
	if (r1end > r2end) /* 1 is rightmost, get highest start, fix size, and read next r2 */
	    {
	    r[i].start = max(r1->start,r2->start);
	    r[i].size = r2end - r[i].start;
	    size += r[i].size;
	    ++i;
	    --n2;
	    ++r2;
	    }
	else if (r2end > r1end) /* 2 is rightmost, set lowest start, fix size, and read next r1 */
	    {
	    r[i].start = max(r1->start,r2->start);
	    r[i].size = r1end - r[i].start;
	    size += r[i].size;
	    ++i;
	    --n1;
	    ++r1;
	    }
	else /* 1 and 2 both end on same base, write out smallest one, and read both */
	    {
	    r[i].start = max(r1->start,r2->start);
            r[i].size = r1end - r[i].start;
            size += r[i].size;
            ++i;
	    --n1;
	    ++r1;
	    --n2;
	    ++r2;
	    }
	}
    }
*n = i;
if (*n == 0) /* if no results, free this memory */
    freez(&r);
else if (saveMem && *n < rSize/2) /* we seriously overestimated mem reqts so free it up */
    {
    struct rangeStartSize *r0 = CloneArray(r, *n);
    freez(&r);
    r = r0;
    }
*pRange = r;
return size;
}

unsigned rangeTreeFileUnion(struct rangeStartSize *r1, struct rangeStartSize *r2, int n1, int n2, struct rangeStartSize **pRange, int *n, boolean saveMem)
/* Create union of array of 'n1' ranges (start,size) in r1 with 
 * 'n2' ranges in r2, saving them in array r and returning 
 * the number of merged ragnes in n. 
 * Note that the ranges are as stored on disk (start,size)
 * not as in the rangeTree (start,end).
 * Free array with freez(&r)
 * Returns total size of ranges in r. */
{
int r1end, r2end;
unsigned size = 0;
struct rangeStartSize *r = NULL;
int i = 0;
int rSize = n1 + n2; /* worst case these dont overlap at all */
AllocArray(r, rSize);
/* loop around merging while we have ranges in both lists */
while ( n1 > 0 && n2 > 0)
    {
    r1end = r1->start+r1->size;
    r2end = r2->start+r2->size;
    /* check which should be merged or output */
    if ( r1end <= r2->start ) /* r1 is upstream, write it and get next one */
	{
	r[i].start = r1->start;
	r[i].size = r1->size;
	size += r[i].size;
	++i;
	--n1;
	++r1;
	}
    else if (r2end <= r1->start) /* r2 is upstream, write it and get next one */
	{
	r[i].start = r2->start;
	r[i].size = r2->size;
	size += r[i].size;
	++i;
	--n2;
	++r2;
	}
    else 
    /* Overlap: keep the one with rightmost end, and extend its start position
     * to the left if need be. Note that the next element from this rangeTree 
     * will not overlap. 
     * Read the next element from the other rangeTree as it may overlap. 
     * If they both end on the same base, output the largest of the two
     * and read in the next from each rangeTree. */
	{ 
	if (r1end > r2end) /* 1 is rightmost, set lowest start, fix size, and read next r2 */
	    {
	    if (r2->start < r1->start)
		{
		r1->start = r2->start;
		r1->size = r1end - r1->start;
		}
	    --n2;
	    ++r2;
	    }
	else if (r2end > r1end) /* 2 is rightmost, set lowest start, fix size, and read next r1 */
	    {
	    if (r1->start < r2->start)
		{
		r2->start = r1->start;
		r2->size = r2end - r2->start;
		}
	    --n1;
	    ++r1;
	    }
	else /* 1 and 2 both end on same base, write out left-most one */
	    {
	    if (r1->start <= r2->start)
		{
		r[i].start = r1->start;
		r[i].size = r1->size;
		size += r[i].size;
		}
	    else
		{
		r[i].start = r2->start;
		r[i].size = r2->size;
		size += r[i].size;
		}
	    ++i;
	    --n1;
	    ++r1;
	    --n2;
	    ++r2;
	    }
	}
    }
/* write out any remaining nodes that are either in n1 or n2 */
while (n1)
    {
    r[i].start = r1->start;
    r[i].size = r1->size;
    size += r[i].size;
    ++i;
    --n1;
    ++r1;
    }
while (n2)
    {
    r[i].start = r2->start;
    r[i].size = r2->size;
    size += r[i].size;
    ++i;
    --n2;
    ++r2;
    }
if (i == 0) /* if no results, free this memory */
    freez(&r);
else if (saveMem && i < rSize/2) /* we seriously overestimated mem reqts so free it up */
    {
    struct rangeStartSize *r0 = CloneArray(r, i);
    freez(&r);
    r = r0;
    }
*n = i;
*pRange = r;
return size;
}


unsigned rangeTreeFileUnionToFile(struct rangeStartSize *r1, struct rangeStartSize *r2, int n1, int n2, FILE *of, int *n)
/* Create union of array of 'n1' ranges (start,size) in r1 with 
 * 'n2' ranges in r2, writing them to output file 'of' and returning 
 * the number of merged ranges written in 'n'. 
 * Note that the ranges are as stored on disk (start,size)
 * not as in the rangeTree (start,end).
 * Writes the ranges one-by-one.
 * Returns total size of ranges in merged file. */
{
int r1end, r2end;
unsigned size = 0;
*n = 0;
/* loop around merging while we have ranges in both lists */
while ( n1 > 0 && n2 > 0)
    {
    r1end = r1->start+r1->size;
    r2end = r2->start+r2->size;
    /* check which should be merged or output */
    if ( r1end <= r2->start ) /* r1 is upstream, write it and get next one */
	{
	if (of)
	    mustWrite(of, r1, sizeof(*r1));
	size += r1->size;
	++(*n);
	--n1;
	++r1;
	}
    else if (r2end <= r1->start) /* r2 is upstream, write it and get next one */
	{
	if (of)
	    mustWrite(of, r2, sizeof(*r2));
	size += r2->size;
	++(*n);
	--n2;
	++r2;
	}
    else 
    /* Overlap: keep the one with rightmost end, and extend its start position
     * to the left if need be. Note that the next element from this rangeTree 
     * will not overlap. 
     * Read the next element from the other rangeTree as it may overlap. 
     * If they both end on the same base, output the largest of the two
     * and read in the next from each rangeTree. */
	{ 
	if (r1end > r2end) /* 1 is rightmost, set lowest start, fix size, and read next r2 */
	    {
	    if (r2->start < r1->start)
		{
		r1->start = r2->start;
		r1->size = r1end - r1->start;
		}
	    --n2;
	    ++r2;
	    }
	else if (r2end > r1end) /* 2 is rightmost, set lowest start, fix size, and read next r1 */
	    {
	    if (r1->start < r2->start)
		{
		r2->start = r1->start;
		r2->size = r2end - r2->start;
		}
	    --n1;
	    ++r1;
	    }
	else /* 1 and 2 both end on same base, write out left-most one */
	    {
	    if (r1->start <= r2->start)
		{
		if (of)
		    mustWrite(of, r1, sizeof(*r1));
		size += r1->size;
		}
	    else
		{
		if (of)
		    mustWrite(of, r2, sizeof(*r2));
		size += r2->size;
		}
	    ++(*n);
	    --n1;
	    ++r1;
	    --n2;
	    ++r2;
	    }
	}
    }
/* write out any remaining nodes that are either in n1 or n2 */
while (n1)
    {
    if (of)
	mustWrite(of, r1, sizeof(*r1));
    size += r1->size;
    ++(*n);
    --n1;
    ++r1;
    }
while (n2)
    {
    if (of)
	mustWrite(of, r2, sizeof(*r2));
    size += r2->size;
    ++(*n);
    --n2;
    ++r2;
    }
return size;
}


