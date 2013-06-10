/* Let the user redirect where memory allocation/deallocation
 * happens.  'careful' routines help debug scrambled heaps. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef MEMALLOC_H
#define MEMALLOC_H

struct memHandler
    {
    struct memHandler *next;
    void * (*alloc)(size_t size);
    void (*free)(void *vpt);
    void * (*realloc)(void* vpt, size_t size);
    };

struct memHandler *pushMemHandler(struct memHandler *newHandler);
/* Use newHandler for memory requests until matching popMemHandler.
 * Returns previous top of memory handler stack. */

struct memHandler *popMemHandler();
/* Removes top element from memHandler stack and returns it. */

void setDefaultMemHandler();
/* Sets memHandler to the default. */

void pushCarefulMemHandler(size_t maxAlloc);
/* Push the careful (paranoid, conservative, checks everything)
 * memory handler  top of the memHandler stack and use it. */

void carefulCheckHeap();
/* Walk through allocated memory and make sure that all cookies are
 * in place. Only walks through what's been done since 
 * pushCarefulMemHandler(). */

int carefulCountBlocksAllocated();
/* How many memory items are allocated? (Since called
 * pushCarefulMemHandler(). */

size_t carefulTotalAllocated();
/* Return total bases allocated */

void setMaxAlloc(size_t s);
/* Set large allocation limit. */

void memTrackerStart();
/* Push memory handler that will track blocks allocated so that
 * they can be automatically released with memTrackerEnd().  */

void memTrackerEnd();
/* Free any remaining blocks and pop tracker memory handler. */

#endif /* MEMALLOC_H */

