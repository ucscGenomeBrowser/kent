/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* memalloc.c - Routines to allocate and deallocate dynamic memory. 
 * This lets you have a stack of memory handlers.  The default
 * memory handler is a thin shell around malloc/free.  You can
 * substitute routines that do more integrety checking with
 * pushCarefulMem(), or routines of your own devising with
 * pushMemHandler(). */
#include "common.h"
#include "memalloc.h"
#include "dlist.h"

static void *defaultAlloc(size_t size)
/* Default allocator. */
{
return malloc(size);
}

static void defaultFree(void *vpt)
/* Default deallocator. */
{
free(vpt);
}

static struct memHandler defaultMemHandler = 
/* Default memory handler. */
    {
    NULL,
    defaultAlloc,
    defaultFree,
    };

static struct memHandler *mhStack = &defaultMemHandler;

struct memHandler *pushMemHandler(struct memHandler *newHandler)
/* Use newHandler for memory requests until matching popMemHandler.
 * Returns previous top of memory handler stack. */
{
struct memHandler *oldHandler = mhStack;
slAddHead(&mhStack, newHandler);
return oldHandler;
}


struct memHandler *popMemHandler()
/* Removes top element from memHandler stack and returns it. */
{
struct memHandler *oldHandler = mhStack;
if (mhStack == &defaultMemHandler)
    errAbort("Too many popMemHandlers()");
mhStack = mhStack->next;
return oldHandler;
}


void setDefaultMemHandler()
/* Sets memHandler to the default. */
{
mhStack = &defaultMemHandler;
}


void *needLargeMem(size_t size)
/* This calls abort if the memory allocation fails. The memory is
 * not initialized to zero. */
{
void *pt;
if (size == 0 || size >= 128*4*1024*1024)
    {
    warn("Program error: trying to allocate %d bytes in needLargeMem", size);
    assert(FALSE);
    }
if ((pt = mhStack->alloc(size)) == NULL)
    errAbort("Out of memory - request size %d bytes\n", size);
return pt;
}

void *needLargeZeroedMem(long size)
/* Request a large block of memory and zero it. */
{
void *v;
v = needLargeMem(size);
memset(v, 0, size);
return v;
}

void *needHugeMem(size_t size)
/* No checking on size.  Memory not initted. */
{
void *pt;
if (size == 0)
    {
    warn("Program error: trying to allocate 0 bytes in needHugeMem");
    assert(FALSE);
    }
if ((pt = mhStack->alloc(size)) == NULL)
    errAbort("Out of memory - request size %d bytes\n", size);
return pt;
}


void *needHugeZeroedMem(long size)
/* Request a large block of memory and zero it. */
{
void *v;
v = needHugeMem(size);
memset(v, 0, size);
return v;
}


void *needMem(size_t size)
/* Need mem calls abort if the memory allocation fails. The memory
 * is initialized to zero. */
{
void *pt;
if (size == 0 || size > 100000000)
    {
    warn("Program error: trying to allocate %d bytes in needMem", size);
    assert(FALSE);
    }
if ((pt = mhStack->alloc(size)) == NULL)
    errAbort("Out of memory - request size %d bytes\n", size);
memset(pt, 0, size);
return pt;
}

void *needMoreMem(void *old, size_t copySize, size_t newSize)
/* Allocate a new buffer, copy old buffer to it, free old buffer. */
{
void *newBuf = needLargeMem(newSize);
if (copySize > newSize)
    internalErr();
memcpy(newBuf, old, copySize);
memset(newBuf+copySize, 0, newSize - copySize);
freeMem(old);
return newBuf;
}

void *wantMem(size_t size)
/* Want mem just calls malloc - no zeroing of memory, no
 * aborting if request fails. */
{
return mhStack->alloc(size);
}

void freeMem(void *pt)
/* Free memory will check for null before freeing. */
{
if (pt != NULL)
    mhStack->free(pt);
}

void freez(void *vpt)
/* Pass address of pointer.  Will free pointer and set it 
 * to NULL. */
{
void **ppt = (void **)vpt;
freeMem(*ppt);
*ppt = NULL;
}

static int carefulAlignSize;    /* Alignment size for machine - 8 bytes for DEC alpha, 4 for Sparc. */
static int carefulAlignAdd;     /* Do aliSize = *(unaliSize+carefulAlignAdd)&carefulAlignMask); */
static bits32 carefulAlignMask;    /* to make sure requests are aligned. */

static struct memHandler *carefulParent;

static long carefulMaxToAlloc;
static long carefulAlloced;

struct carefulMemBlock
/* Keep one of these for each outstanding memory block.   It's a doubly linked list. */
    {
    struct carefulMemBlock *next;
    struct carefulMemBlock *prev;
    int size;
    int startCookie;
    };

int cmbStartCookie = 0x78753421;

char cmbEndCookie[4] = {0x44, 0x33, 0x7F, 0x42};

struct dlList *cmbAllocedList;

static void carefulMemInit(long maxToAlloc)
/* Initialize careful memory system */
{
carefulMaxToAlloc = maxToAlloc;
cmbAllocedList = newDlList();
carefulAlignSize = sizeof(double);
if (sizeof(void *) > carefulAlignSize)
    carefulAlignSize = sizeof(void *);
if (sizeof(long) > carefulAlignSize)
    carefulAlignSize = sizeof(long);
carefulAlignAdd = carefulAlignSize-1;
carefulAlignMask = ~carefulAlignAdd;
}


static void *carefulAlloc(size_t size)
/* Allocate extra memory for cookies and list node, and then
 * return memory block. */
{
struct carefulMemBlock *cmb;
char *pEndCookie;
long newAlloced = size + carefulAlloced;
size_t aliSize;

if (newAlloced > carefulMaxToAlloc)
    errAbort("Allocated too much memory - more than %ld bytes", carefulMaxToAlloc);
carefulAlloced = newAlloced;
aliSize = ((size + sizeof(*cmb) + 4 + carefulAlignAdd)&carefulAlignMask);
cmb = carefulParent->alloc(aliSize);
cmb->size = size;
cmb->startCookie = cmbStartCookie;
pEndCookie = (char *)(cmb+1);
pEndCookie += size;
memcpy(pEndCookie, cmbEndCookie, sizeof(cmbEndCookie));
dlAddHead(cmbAllocedList, (struct dlNode *)cmb);
return (void *)(cmb+1);
}

static void carefulFree(void *vpt)
/* Check cookies and free. */
{
struct carefulMemBlock *cmb = vpt;
char *pEndCookie;
size_t size;

cmb -= 1;
size = cmb->size;
carefulAlloced -= size;
pEndCookie = (((char *)(cmb+1)) + size);
if (cmb->startCookie != cmbStartCookie)
    errAbort("Bad start cookie %x freeing %x\n", cmb->startCookie, vpt);
if (memcmp(pEndCookie, cmbEndCookie, sizeof(cmbEndCookie)) != 0)
    errAbort("Bad end cookie %x%x%x%x freeing %x\n", 
        pEndCookie[0], pEndCookie[1], pEndCookie[2], pEndCookie[3], vpt);
dlRemove((struct dlNode *)cmb);
carefulParent->free(cmb);
}

void carefulCheckHeap()
/* Walk through allocated memory and make sure that all cookies are
 * in place. */
{
int maxPieces = 10000000;    /* Assume no more than this many pieces allocated. */
struct carefulMemBlock *cmb;
char *pEndCookie;
size_t size;

if (carefulParent == NULL)
    return;

for (cmb = (struct carefulMemBlock *)(cmbAllocedList->head); cmb->next != NULL; cmb = cmb->next)
    {
    size = cmb->size;
    pEndCookie = (((char *)(cmb+1)) + size);
    if (cmb->startCookie != cmbStartCookie)
        errAbort("Bad start cookie %x checking %x\n", cmb->startCookie, cmb+1);
    if (memcmp(pEndCookie, cmbEndCookie, sizeof(cmbEndCookie)) != 0)
        {
        errAbort("Bad end cookie %x%x%x%x checking %x\n", 
            pEndCookie[0], pEndCookie[1], pEndCookie[2], pEndCookie[3], cmb+1);
        }
    if (--maxPieces == 0)
        errAbort("Loop or more than 10000000 pieces in memory list");
    }
}

int carefulCountBlocksAllocated()
/* How many memory items are allocated? */
{
return dlCount(cmbAllocedList);
}

static struct memHandler carefulMemHandler = 
/* Default memory handler. */
    {
    NULL,
    carefulAlloc,
    carefulFree,
    };

void pushCarefulMemHandler(long maxAlloc)
/* Push the careful (paranoid, conservative, checks everything)
 * memory handler  top of the memHandler stack and use it. */
{
carefulMemInit(maxAlloc);
carefulParent = pushMemHandler(&carefulMemHandler);
}
