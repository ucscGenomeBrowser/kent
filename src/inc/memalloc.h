/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
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

#endif /* MEMALLOC_H */

