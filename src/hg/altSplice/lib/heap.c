#include "common.h"
#include "math.h"
#include "heap.h"


int _heapParent(int i)
/* Return the parent index of node at heap index i. */
{
return i/2;
}

int _heapLeft(int i)
/* Return the left child index of node at heap index i. */
{
return i << 1;
}

int _heapRight(int i)
/* Return the right child index of node at heap index i. */
{
return (i << 1) + 1;
}

void _heapExchange(struct heap *h,int i,int j)
/* Exchange i and j in heap. */
{
struct heapEl *tmp = h->array[i];
assert(i<h->count && j<h->count);
h->array[i] = h->array[j];
h->array[j] = tmp;
}

void heapMinHeapify(struct heap *h, int index)
/* Investigates parents to try and produce heap properties. */
{
int l = _heapLeft(index);
int r = _heapRight(index);
int smallest = 0;
if(l < h->count && (h->cmpItems(h->array[l]->val, h->array[index]->val) < 0))
    smallest = l;
else
    smallest = index;
if(r < h->count && (h->cmpItems(h->array[r]->val, h->array[smallest]->val) < 0))
    smallest = r;
if(smallest != index)
    {
    _heapExchange(h, index, smallest);
    heapMinHeapify(h, smallest);
    }
}

void heapMinInsert(struct heap *h, void *val)
/* Insert val into the heap. */
{
struct heapEl *el = NULL;
int index = 0;
assert(h);
index = h->count;
AllocVar(el);
if(index +1 >= h->capacity)
    {
    ExpandArray(h->array, h->capacity, h->capacity * 2);
    h->capacity = h->capacity *2;
    }
el->val = val;
h->array[index] = el;
h->count++;
while(index > 0 && (h->cmpItems(h->array[_heapParent(index)]->val, h->array[index]->val)  >  0))
    {
    _heapExchange(h, _heapParent(index), index);
    index = _heapParent(index);
    }
}

boolean heapEmpty(struct heap *h)
/* Return FALSE if heap has elements, TRUE if empty. */
{
return (h->count <= 0);
}

void *heapExtractMin(struct heap *h)
/* Extract the smallest element from the heap. */
{
struct heapEl *el = NULL;
void *val = NULL;
if(heapEmpty(h))
    errAbort("heap.c::heapExtractMin() - No more elements to removes, heap is empty.");
el = h->array[0];
h->array[0] = h->array[h->count - 1];
heapMinHeapify(h, 0);
val = el->val;
h->count--;
freez(&el);
return val;
}

struct heap *newHeap(int powerOfTowSize, int (*cmpItems)(const void *a, const void *b))
/* Construct a new heap with initial capacity powerOfTowSize and comparator
   function cmpItems. */
{
struct heap *h = NULL;
AllocVar(h);
assert(cmpItems);
if(powerOfTowSize == 0)
    powerOfTowSize = 12;
h->capacity = (1<<powerOfTowSize);
h->array = needMem(h->capacity * sizeof(struct heapEl *));
h->cmpItems = cmpItems;
return h;
}

void heapFree(struct heap **pHeap)
/* Free a heap. */
{
struct heap *h = *pHeap;
int i;
if(h == NULL)
    return;
for(i=0; i<h->count; i++)
    freez(&h->array[i]);
freez(&h->array);
freez(pHeap);
}

void heapTraverseVals(struct heap *heap, void (*func)(void *val))
/* Apply func to every element of heap with heapEl->val as parameter. */
{
int i;
for (i=0; i<heap->count; ++i)
    {
    func(heap->array[i]->val);
    }
}

