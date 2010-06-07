/* A heap/priority queue. */

struct heapEl
/* An element in the heap. */
{
    struct heapEl *next;  /* next in list. */
    void *val;          /* item in heap. */
};

struct heap
/* Heap structure. */
{
    int count;              /* Number of items in queue. */
    int capacity;           /* Capacity of heap. */
    struct heapEl **array;     /* Array of heap nodes. */
    int (*cmpItems)(const void *a, const void *b);     /* Comparator function to determine ordering of heap. */
};

boolean heapEmpty(struct heap *h);
/* Return FALSE if heap has elements, TRUE if empty. */

void heapMinInsert(struct heap *h, void *val);
/* Insert val into the heap. */

void *heapExtractMin(struct heap *h);
/* Extract the smallest element from the heap. */

struct heap *newHeap(int powerOfTowSize, int (*cmpItems)(const void *a, const void *b));
/* Construct a new heap with initial capacity powerOfTowSize and comparator
   function cmpItems. */

void heapFree(struct heap **pHeap);
/* Free a heap. */

void heapTraverseVals(struct heap *heap, void (*func)(void *val));
/* Apply func to every element of heap with heapEl->val as parameter. */;
