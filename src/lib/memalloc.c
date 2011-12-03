/* memalloc.c - Routines to allocate and deallocate dynamic memory. 
 * This lets you have a stack of memory handlers.  The default
 * memory handler is a thin shell around malloc/free.  You can
 * substitute routines that do more integrety checking with
 * pushCarefulMem(), or routines of your own devising with
 * pushMemHandler(). 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "obscure.h"
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

static void *defaultRealloc(void *vpt, size_t size)
/* Default deallocator. */
{
return realloc(vpt, size);
}

static struct memHandler defaultMemHandler = 
/* Default memory handler. */
    {
    NULL,
    defaultAlloc,
    defaultFree,
    defaultRealloc,
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

/* 128*8*1024*1024 == 1073741824 == 2^30 on 32 bit machines,size_t == 4 bytes*/
/* on 64 bit machines, size_t = 8 bytes, 2^30 * 2 * 2 * 2 * 2 = 2^34 == 16 Gb */
static size_t maxAlloc = (size_t)128*8*1024*1024*(sizeof(size_t)/4)*(sizeof(size_t)/4)*(sizeof(size_t)/4*(sizeof(size_t)/4));

void setMaxAlloc(size_t s)
/* Set large allocation limit. */
{
maxAlloc = s;
}

void *needLargeMem(size_t size)
/* This calls abort if the memory allocation fails. The memory is
 * not initialized to zero. */
{
void *pt;
if (size == 0 || size >= maxAlloc)
    errAbort("needLargeMem: trying to allocate %llu bytes (limit: %llu)",
         (unsigned long long)size, (unsigned long long)maxAlloc);
if ((pt = mhStack->alloc(size)) == NULL)
    errAbort("needLargeMem: Out of memory - request size %llu bytes, errno: %d\n",
             (unsigned long long)size, errno);
return pt;
}

void *needLargeZeroedMem(size_t size)
/* Request a large block of memory and zero it. */
{
void *v;
v = needLargeMem(size);
memset(v, 0, size);
return v;
}

void *needLargeMemResize(void* vp, size_t size)
/* Adjust memory size on a block, possibly relocating it.  If vp is NULL,
 * a new memory block is allocated.  Memory not initted. */
{
void *pt;
if (size == 0 || size >= maxAlloc)
    errAbort("needLargeMemResize: trying to allocate %llu bytes (limit: %llu)",
         (unsigned long long)size, (unsigned long long)maxAlloc);
if ((pt = mhStack->realloc(vp, size)) == NULL)
    errAbort("needLargeMemResize: Out of memory - request size %llu bytes, errno: %d\n",
             (unsigned long long)size, errno);
return pt;
}

void *needLargeZeroedMemResize(void* vp, size_t oldSize, size_t newSize)
/* Adjust memory size on a block, possibly relocating it.  If vp is NULL, a
 * new memory block is allocated.  If block is grown, new memory is zeroed. */
{
void *v = needLargeMemResize(vp, newSize);
if (newSize > oldSize)
    memset(((char*)v)+oldSize, 0, newSize-oldSize);
return v;
}

void *needHugeMem(size_t size)
/* No checking on size.  Memory not initted. */
{
void *pt;
if (size == 0)
    errAbort("needHugeMem: trying to allocate 0 bytes");
if ((pt = mhStack->alloc(size)) == NULL)
    errAbort("needHugeMem: Out of huge memory - request size %llu bytes, errno: %d\n",
             (unsigned long long)size, errno);
return pt;
}


void *needHugeZeroedMem(size_t size)
/* Request a large block of memory and zero it. */
{
void *v;
v = needHugeMem(size);
memset(v, 0, size);
return v;
}

void *needHugeMemResize(void* vp, size_t size)
/* Adjust memory size on a block, possibly relocating it.  If vp is NULL,
 * a new memory block is allocated.  No checking on size.  Memory not
 * initted. */
{
void *pt;
if ((pt = mhStack->realloc(vp, size)) == NULL)
    errAbort("needHugeMemResize: Out of memory - request resize %llu bytes, errno: %d\n",
	(unsigned long long)size, errno);
return pt;
}


void *needHugeZeroedMemResize(void* vp, size_t oldSize, size_t newSize)
/* Adjust memory size on a block, possibly relocating it.  If vp is NULL, a
 * new memory block is allocated.  No checking on size.  If block is grown,
 * new memory is zeroed. */
{
void *v;
v = needHugeMemResize(vp, newSize);
if (newSize > oldSize)
    memset(((char*)v)+oldSize, 0, newSize-oldSize);
return v;
}

#define NEEDMEM_LIMIT 500000000

void *needMem(size_t size)
/* Need mem calls abort if the memory allocation fails. The memory
 * is initialized to zero. */
{
void *pt;
if (size == 0 || size > NEEDMEM_LIMIT)
    errAbort("needMem: trying to allocate %llu bytes (limit: %llu)",
         (unsigned long long)size, (unsigned long long)NEEDMEM_LIMIT);
if ((pt = mhStack->alloc(size)) == NULL)
    errAbort("needMem: Out of memory - request size %llu bytes, errno: %d\n",
             (unsigned long long)size, errno);
memset(pt, 0, size);
return pt;
}

void *needMoreMem(void *old, size_t oldSize, size_t newSize)
/* Adjust memory size on a block, possibly relocating it.  If vp is NULL, a
 * new memory block is allocated.  No checking on size.  If block is grown,
 * new memory is zeroed. */
{
return needLargeZeroedMemResize(old, oldSize, newSize);
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
void *pt = *ppt;
*ppt = NULL;
freeMem(pt);
}

static int carefulAlignSize;    /* Alignment size for machine - 8 bytes for DEC alpha, 4 for Sparc. */
static int carefulAlignAdd;     /* Do aliSize = *(unaliSize+carefulAlignAdd)&carefulAlignMask); */

#if __WORDSIZE == 64
static bits64 carefulAlignMask;    /* to make sure requests are aligned. */
#elif __WORDSIZE == 32
static bits32 carefulAlignMask;    /* to make sure requests are aligned. */
#else
static bits32 carefulAlignMask;    /* to make sure requests are aligned. */
#endif

static struct memHandler *carefulParent;

static size_t carefulMaxToAlloc;
static size_t carefulAlloced;

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

static void carefulMemInit(size_t maxToAlloc)
/* Initialize careful memory system */
{
carefulMaxToAlloc = maxToAlloc;
cmbAllocedList = newDlList();
carefulAlignSize = sizeof(double);
if (sizeof(void *) > carefulAlignSize)
    carefulAlignSize = sizeof(void *);
if (sizeof(long) > carefulAlignSize)
    carefulAlignSize = sizeof(long);
if (sizeof(off_t) > carefulAlignSize)
    carefulAlignSize = sizeof(off_t);
if (sizeof(long long) > carefulAlignSize)
    carefulAlignSize = sizeof(long long);
carefulAlignAdd = carefulAlignSize-1;
carefulAlignMask = ~carefulAlignAdd;
}


static void *carefulAlloc(size_t size)
/* Allocate extra memory for cookies and list node, and then
 * return memory block. */
{
struct carefulMemBlock *cmb;
char *pEndCookie;
size_t newAlloced = size + carefulAlloced;
size_t aliSize;

if (newAlloced > carefulMaxToAlloc)
    {
    char maxAlloc[32];
    char allocRequest[32];
    sprintLongWithCommas(maxAlloc, (long long)carefulMaxToAlloc);
    sprintLongWithCommas(allocRequest, (long long)newAlloced);
    errAbort("carefulAlloc: Allocated too much memory - more than %s bytes (%s)",
	maxAlloc, allocRequest);
    }
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
struct carefulMemBlock *cmb = ((struct carefulMemBlock *)vpt)-1;
size_t size = cmb->size;
char *pEndCookie;

carefulAlloced -= size;
pEndCookie = (((char *)(cmb+1)) + size);
if (cmb->startCookie != cmbStartCookie)
    errAbort("Bad start cookie %x freeing %llx\n", cmb->startCookie,
             ptrToLL(vpt));
if (memcmp(pEndCookie, cmbEndCookie, sizeof(cmbEndCookie)) != 0)
    errAbort("Bad end cookie %x%x%x%x freeing %llx\n", 
        pEndCookie[0], pEndCookie[1], pEndCookie[2], pEndCookie[3],
             ptrToLL(vpt));
dlRemove((struct dlNode *)cmb);
carefulParent->free(cmb);
}


static void *carefulRealloc(void *vpt, size_t size)
/* realloc a careful memblock block. */
{
unsigned char* newBlk = carefulAlloc(size);
if (vpt != NULL)
    {
    struct carefulMemBlock *cmb = ((struct carefulMemBlock *)vpt)-1;
    memcpy(newBlk, vpt, cmb->size);
    carefulFree(vpt);
    }
return newBlk;
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
        errAbort("Bad start cookie %x checking %llx\n", cmb->startCookie,
                 ptrToLL(cmb+1));
    if (memcmp(pEndCookie, cmbEndCookie, sizeof(cmbEndCookie)) != 0)
        errAbort("Bad end cookie %x%x%x%x checking %llx\n", 
                 pEndCookie[0], pEndCookie[1], pEndCookie[2], pEndCookie[3],
                 ptrToLL(cmb+1));
    if (--maxPieces == 0)
        errAbort("Loop or more than 10000000 pieces in memory list");
    }
}

int carefulCountBlocksAllocated()
/* How many memory items are allocated? */
{
return dlCount(cmbAllocedList);
}

long carefulTotalAllocated()
/* Return total bases allocated */
{
return carefulAlloced;
}

static struct memHandler carefulMemHandler = 
/* Default memory handler. */
    {
    NULL,
    carefulAlloc,
    carefulFree,
    carefulRealloc,
    };

void pushCarefulMemHandler(size_t maxAlloc)
/* Push the careful (paranoid, conservative, checks everything)
 * memory handler  top of the memHandler stack and use it. */
{
carefulMemInit(maxAlloc);
carefulParent = pushMemHandler(&carefulMemHandler);
}

struct memTracker
/* A structure to keep track of memory. */
    {
    struct memTracker *next;	 /* Next in list. */
    struct dlList *list;	 /* List of allocated blocks. */
    struct memHandler *parent;   /* Underlying memory handler. */
    struct memHandler *handler;  /* Memory handler. */
    };

static struct memTracker *memTracker = NULL;	/* Head in memTracker list. */

static void *memTrackerAlloc(size_t size)
/* Allocate extra memory for cookies and list node, and then
 * return memory block. */
{
struct dlNode *node;

size += sizeof (*node);
node = memTracker->parent->alloc(size);
if (node == NULL)
    return node;
dlAddTail(memTracker->list, node);
return (void*)(node+1);
}

static void memTrackerFree(void *vpt)
/* Check cookies and free. */
{
struct dlNode *node = vpt;
node -= 1;
dlRemove(node);
memTracker->parent->free(node);
}

static void *memTrackerRealloc(void *vpt, size_t size)
/* Resize a memory block from memTrackerAlloc. */
{
if (vpt == NULL)
    return memTrackerAlloc(size);
else
    {
    struct dlNode *node = ((struct dlNode *)vpt)-1;
    size += sizeof(*node);
    dlRemove(node);
    node = memTracker->parent->realloc(node, size);
    if (node == NULL)
        return node;
    dlAddTail(memTracker->list, node);
    return (void*)(node+1);
    }
}

void memTrackerStart()
/* Push memory handler that will track blocks allocated so that
 * they can be automatically released with memTrackerEnd().  You
 * can have memTrackerStart one after the other, but memTrackerStart/End
 * need to nest. */
{
struct memTracker *mt;

if (memTracker != NULL)
     errAbort("multiple memTrackerStart calls");
AllocVar(mt);
AllocVar(mt->handler);
mt->handler->alloc = memTrackerAlloc;
mt->handler->free = memTrackerFree;
mt->handler->realloc = memTrackerRealloc;
mt->list = dlListNew();
mt->parent = pushMemHandler(mt->handler);
memTracker = mt;
}

void memTrackerEnd()
/* Free any remaining blocks and pop tracker memory handler. */
{
struct memTracker *mt = memTracker;
if (mt == NULL)
    errAbort("memTrackerEnd without memTrackerStart");
memTracker = NULL;
popMemHandler();
dlListFree(&mt->list);
freeMem(mt->handler);
freeMem(mt);
}
