/* Let the user redirect where memory allocation/deallocation
 * happens.  'careful' routines help debug scrambled heaps. */

#ifndef MEMALLOC_H
#define MEMALLOC_H

struct memHandler
    {
    struct memHandler *next;
    void * (*alloc)(size_t size);
    void (*free)(void *vpt);
    };

struct memHandler *pushMemHandler(struct memHandler *newHandler);
/* Use newHandler for memory requests until matching popMemHandler.
 * Returns previous top of memory handler stack. */

struct memHandler *popMemHandler();
/* Removes top element from memHandler stack and returns it. */

void setDefaultMemHandler();
/* Sets memHandler to the default. */

void pushCarefulMemHandler(long maxAlloc);
/* Push the careful (paranoid, conservative, checks everything)
 * memory handler  top of the memHandler stack and use it. */

void carefulCheckHeap();
/* Walk through allocated memory and make sure that all cookies are
 * in place. Only walks through what's been done since 
 * pushCarefulMemHandler(). */

int carefulCountBlocksAllocated();
/* How many memory items are allocated? (Since called
 * pushCarefulMemHandler(). */

void setMaxAlloc(size_t s);
/* Set large allocation limit. */

#endif /* MEMALLOC_H */

