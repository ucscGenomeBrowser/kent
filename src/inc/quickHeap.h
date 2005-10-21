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
 */

struct quickHeap 
/* A quick array heap. */
    {
    void **heap;
    int heapCount;
    int heapMax;
    int (*compareFunc)(const void *elem1, const void *elem2);
    };

struct quickHeap *newQuickHeap(int initSize, 
   int (*compare )(const void *elem1,  const void *elem2));
/* Create a new array quick heap of initial size specified,
 * The compare function must return > 0 if elem1 > elem2, etc.*/

void freeQuickHeap(struct quickHeap **pH);
/* Heap needs more space, double the size of the array preserving elements */

void addToQuickHeap(struct quickHeap *h, void *elem);
/* add to heap at end (expands array if needed), rebalance */

void quickHeapTopChanged(struct quickHeap *h);
/* only the value of the top element has changed, now rebalance */

boolean quickHeapEmpty(struct quickHeap *h);
/* return TRUE if quick heap is empty, otherwise FALSE */

boolean removeFromQuickHeapByElem(struct quickHeap *h, void *elem);
/* Do a linear search in heap array for elem,
 * then remove it by position n. Return TRUE
 * if found and removed, otherwise return FALSE. */

void *peekQuickHeapTop(struct quickHeap *h);
/* return the top element or NULL if empty */

void *removeQuickHeapTop(struct quickHeap *h);
/* Return elem (pointer) in heap array[0]
 * which will be NULL if heap is empty.
 * Then that top element if found is removed. */

