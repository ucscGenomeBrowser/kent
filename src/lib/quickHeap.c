/* quickHeap - Heap implemented as an array for speed.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial.
 *
 *    The array can be resized as it grows.
 *  By preserving the relationship that the children of n are at 2n+1 and 2n+2,
 *  and therefore the parent of n is at (n-1)/2, we can save alot of pointer
 *  manipulation and get some speed.  
 *    This routine could for instance be used at the heart of a multi-way mergesort
 *  or other situation such as a priority queue.
 *
 * Maintaining the heap is quick and easy since
 * we just use an array with the following relationships:
 * children(n) == 2n+1,2n+2. parent(n) == (n-1)/2.
 * The highest score is always at the top of the heap in 
 * position 0. When the top element is consumed and 
 * and the next element for the previously top elem is 
 * set in, we simply balance the heap to maintain the heap property
 * by swapping with the largest child until both children 
 * are smaller, or we hit the end of the array (==heapCount).
 * Also note that scores are not necessarily unique, heap members
 * may have equal score.  So A parent need not only be greater
 * than its children, it may also be equal to them.
 */

#include "common.h"
#include "quickHeap.h"



struct quickHeap *newQuickHeap(int initSize, 
   int (*compare )(const void *elem1,  const void *elem2))
/* Create a new array quick heap of initial size specified,
 * The compare function must return > 0 if elem1 > elem2, etc.*/
{
struct quickHeap *h;
if (initSize < 1)
    errAbort("invalid initial size for newQuickHeap: %d",initSize);
AllocVar(h);
h->heapCount=0;
h->heapMax = initSize;
h->heap = AllocN(void *,h->heapMax);
h->compareFunc = compare;
return h;
}


void freeQuickHeap(struct quickHeap **pH)
/* Heap needs more space, double the size of the array preserving elements */
{
struct quickHeap *h = *pH;
freez(&h->heap);
freez(pH);
}

static void expandQuickHeap(struct quickHeap *h)
/* Heap needs more space, double the size of the array preserving elements */
{
int newSize = h->heapMax * 2;
ExpandArray(h->heap, h->heapMax, newSize);
h->heapMax = newSize;
}

void addToQuickHeap(struct quickHeap *h, void *elem)
/* add to heap at end (expands array if needed), rebalance */
{
int n, p;   /* node, parent */
if (h->heapCount >= h->heapMax)
    expandQuickHeap(h);
/* preserve heap property as it grows */
n = h->heapCount;
h->heap[h->heapCount++] = elem;
p = (n-1)/2;
while (n > 0 && h->compareFunc(h->heap[p], h->heap[n]) < 0)
    {
    void *temp = h->heap[p];
    h->heap[p] = h->heap[n];
    h->heap[n] = temp;
    n = p;
    p = (n-1)/2;
    }
}


static void quickHeapBalanceWithChildren(struct quickHeap *h, int n)
/* only the value of the n-th element has changed, now rebalance */
{
int c1, c2, hc;   /* child1, child2, heapCount */
if (n < 0 || n >= h->heapCount)
    errAbort("attempt to quickHeapBalanceWithChildren with invalid value "
		"for n: %d, heapCount=%d", n, h->heapCount);
/* balance heap - swap node with biggest child until both children are 
 * either less or equal or hit end of heap (no more children) */
hc = h->heapCount;
c1 = 2*n+1;
c2 = 2*n+2;
while (TRUE)
    {
    void *temp = NULL;
    int bestc = n;
    if (c1 < hc && h->compareFunc(h->heap[c1], h->heap[bestc]) > 0)
	bestc = c1;
    if (c2 < hc && h->compareFunc(h->heap[c2], h->heap[bestc]) > 0)
	bestc = c2;
    if (bestc == n)
	break;
    temp = h->heap[bestc];
    h->heap[bestc] = h->heap[n];
    h->heap[n] = temp;
    n = bestc;
    c1 = 2*n+1;
    c2 = 2*n+2;
    }
}


void quickHeapTopChanged(struct quickHeap *h)
/* only the value of the top element has changed, now rebalance */
{
/* balance heap - swap node with biggest child until both children are 
 * either less or equal or hit end of heap (no more children) */
quickHeapBalanceWithChildren(h,0);
}

boolean quickHeapEmpty(struct quickHeap *h)
/* return TRUE if quick heap is empty, otherwise FALSE */
{
return h->heapCount < 1;
}

static void removeFromQuickHeapN(struct quickHeap *h, int n)
/* Remove element n from the heap by swapping 
 * it with the last element in the heap and rebalancing.
 * If heap is empty it will errAbort, but that should not happen. */
{
if (n < 0 || n >= h->heapCount)
    errAbort("attempt to removeFromQuickHeapN with invalid value "
		"for n: %d, heapCount=%d", n, h->heapCount);
h->heap[n] = h->heap[--h->heapCount]; /* wipes out n by moving tail to it */
h->heap[h->heapCount] = NULL; /* not needed but clean */
/* balance heap - swap node with biggest child until both children are 
 * either less or equal or hit end of heap (no more children) */
if (n < h->heapCount)  /* i.e. true for all but when n was the tail */
    quickHeapBalanceWithChildren(h,n);
}


static int findElemInQuickHeap(struct quickHeap *h, void *elem)
/* Do a linear search in heap array for elem,
 * return position n or -1 if not found */
{
int n = -1;
while (++n < h->heapCount)
    {
    if (h->heap[n] == elem)
	return n;
    }
return -1;    
}


boolean removeFromQuickHeapByElem(struct quickHeap *h, void *elem)
/* Do a linear search in heap array for elem,
 * then remove it by position n. Return TRUE
 * if found and removed, otherwise return FALSE. */
{
int n = findElemInQuickHeap(h, elem);
if (n < 0) 
    return FALSE;
removeFromQuickHeapN(h,n);
return TRUE;
}

void *peekQuickHeapTop(struct quickHeap *h)
/* return the top element or NULL if empty */
{
if (h->heapCount < 1) 
    return NULL;
return h->heap[0];    
}

void *removeQuickHeapTop(struct quickHeap *h)
/* Return elem (pointer) in heap array[0]
 * which will be NULL if heap is empty.
 * Then that top element if found is removed. */
{
void *result = h->heap[0]; 
/* this is ok, but depends on removeFromQuickHeapN NULLing as it empties */   
if (h->heapCount > 0) 
    {
    removeFromQuickHeapN(h,0);
    }
return result;
}

