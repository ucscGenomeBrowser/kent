/* binRange Stuff to handle binning - which helps us restrict 
 * our attention to the parts of database that contain info
 * about a particular window on a chromosome. This scheme
 * will work without modification for chromosome sizes up
 * to half a gigaBase.  The finest sized bin is 128k (1<<17).
 * The next coarsest is 8x as big (1<<3).  There's a hierarchy
 * of bins with the chromosome itself being the final bin.
 * Features are put in the finest bin they'll fit in. */

#include "common.h"
#include "binRange.h"

static int binOffsets[] = {512+64+8+1, 64+8+1, 8+1, 1, 0};
#define _binFirstShift 17	/* How much to shift to get to finest bin. */
#define _binNextShift 3		/* How much to shift to get to next larger bin. */



int binLevels()
/* Return number of levels to bins. */
{
return ArraySize(binOffsets);
}

int binFirstShift()
/* Return amount to shift a number to get to finest bin. */
{
return _binFirstShift;
}

int binNextShift()
/* Return amount to shift a number to get to next coarser bin. */
{
return _binNextShift;
}

int binOffset(int level)
/* Return offset for bins of a given level. */
{
assert(level >= 0 && level < ArraySize(binOffsets));
return binOffsets[level];
}

int binFromRange(int start, int end)
/* Given start,end in chromosome coordinates assign it
 * a bin.   There's a bin for each 128k segment, for each
 * 1M segment, for each 8M segment, for each 64M segment,
 * and for each chromosome (which is assumed to be less than
 * 512M.)  A range goes into the smallest bin it will fit in. */
{
int startBin = start, endBin = end-1, i;
startBin >>= _binFirstShift;
endBin >>= _binFirstShift;
for (i=0; i<ArraySize(binOffsets); ++i)
    {
    if (startBin == endBin)
        return binOffsets[i] + startBin;
    startBin >>= _binNextShift;
    endBin >>= _binNextShift;
    }
errAbort("start %d, end %d out of range in findBin (max is 512M)", start, end);
return 0;
}


struct binKeeper *binKeeperNew(int minPos, int maxPos)
/* Create new binKeeper that can cover range. */
{
int binCount;
struct binKeeper *bk;
if (minPos < 0 || maxPos < 0 || minPos > maxPos || maxPos > 512*1024*1024)
    errAbort("bad range %d,%d in binKeeperNew", minPos, maxPos);

binCount = binFromRange(maxPos-1, maxPos) + 1;
AllocVar(bk);
bk->minPos = minPos;
bk->maxPos = maxPos;
bk->binCount = binCount;
AllocArray(bk->binLists, binCount);
return bk;
}

void binKeeperFree(struct binKeeper **pBk)
/* Free up a bin keeper. */
{
struct binKeeper *bk = *pBk;
if (bk != NULL)
    {
    int i;
    for (i=0; i<bk->binCount; ++i)
	slFreeList(&bk->binLists[i]);
    freeMem(bk->binLists);
    freez(pBk);
    }
}

void binKeeperAdd(struct binKeeper *bk, int start, int end, void *val)
/* Add item to binKeeper. */ 
{
int bin;
struct binElement *el;
if (start < bk->minPos || end > bk->maxPos || start > end)
    errAbort("(%d %d) out of range (%d %d) in binKeeperAdd", 
    	start, end, bk->minPos, bk->maxPos);
bin = binFromRange(start, end);
assert(bin < bk->binCount);
AllocVar(el);
el->start = start;
el->end = end;
el->val = val;
slAddHead(&bk->binLists[bin], el);
}

int binElementCmpStart(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct binElement *a = *((struct binElement **)va);
const struct binElement *b = *((struct binElement **)vb);
return a->start - b->start;
}

struct binElement *binKeeperFind(struct binKeeper *bk, int start, int end)
/* Return a list of all items in binKeeper that intersect range.
 * Free this list with slFreeList. */
{
struct binElement *list = NULL, *newEl, *el;
int startBin = (start>>_binFirstShift), endBin = ((end-1)>>_binFirstShift);
int i,j;

for (i=0; i<ArraySize(binOffsets); ++i)
    {
    int offset = binOffsets[i];
    for (j=startBin+offset; j<=endBin+offset; ++j)
        {
	for (el=bk->binLists[j]; el != NULL; el = el->next)
	    {
	    if (rangeIntersection(el->start, el->end, start, end) > 0)
	        {
		newEl = CloneVar(el);
		slAddHead(&list, newEl);
		}
	    }
	}
    startBin >>= _binNextShift;
    endBin >>= _binNextShift;
    }
return list;
}

struct binElement *binKeeperFindSorted(struct binKeeper *bk, int start, int end)
/* Like binKeeperFind, but sort list on start coordinates. */
{
struct binElement *list = binKeeperFind(bk, start, end);
slSort(&list, binElementCmpStart);
return list;
}

struct binElement *binKeeperFindAll(struct binKeeper *bk)
/* Get all elements sorted. */
{
return binKeeperFindSorted(bk, bk->minPos, bk->maxPos);
}

struct binElement *binKeeperFindLowest(struct binKeeper *bk, int start, int end)
/* Find the lowest overlapping range. Quick even if search range large */
{
struct binElement *first = NULL, *el;
int startBin = (start>>_binFirstShift), endBin = ((end-1)>>_binFirstShift);
int i,j;

/* Search the possible range of bins at each level, looking for lowest.  Once
 * an overlaping range is found at a level, continue with next level, however
 * must search an entire bin as they are not ordered. */
for (i=0; i<ArraySize(binOffsets); ++i)
    {
    int offset = binOffsets[i];
    boolean foundOne = FALSE;
    for (j=startBin+offset; (j<=endBin+offset) && (!foundOne); ++j)
        {
	for (el=bk->binLists[j]; el != NULL; el = el->next)
	    {
            if ((rangeIntersection(el->start, el->end, start, end) > 0)
                && ((first == NULL) || (el->start < first->start)
                    || ((el->start == first->start)
                        && (el->end < first->end))))
                {
                first = el;
                foundOne = TRUE;
		}
	    }
	}
    startBin >>= _binNextShift;
    endBin >>= _binNextShift;
    }
return first;
}


void binKeeperRemove(struct binKeeper *bk, int start, int end, void *val)
/* Remove item from binKeeper. */ 
{
int bin = binFromRange(start, end);
struct binElement **pList = &bk->binLists[bin], *newList = NULL, *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    if (el->val == val && el->start == start && el->end == end)
        {
	freeMem(el);
	}
    else
        {
	slAddHead(&newList, el);
	}
    }
slReverse(&newList);
*pList = newList;
}
