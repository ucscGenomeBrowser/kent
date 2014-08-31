/* pthreadDoList - do something to all items in list using multiple threads in parallel. 
 * See pthreadDoList.h for documentation on how to use. */

#include "common.h"
#include "pthreadWrap.h"
#include "synQueue.h"
#include "pthreadDoList.h"

struct pthreadWorkList
/* Manage threads to work on a list of items in parallel. */
    {
    struct pthreadWorkList *next;   /* Next in list */
    int threadCount;		    /* Number of threads to use */
    struct slList *itemList;	    /* Input list - not allocated here. */
    void *context;		    /* Constant context data for each item */
    PthreadListWorker *worker;   /* Routine that does work in a single thread */
    pthread_t *pthreads;		    /* Array of pthreads */
    struct synQueue *inQueue;	    /* Put all inputs here */
    int itemCount;		    /* Number of items in itemList */
    int toDoCount;		    /* Number of items left to process. */
    pthread_mutex_t finishMutex;    /* Mutex to prevent simultanious access to finish. */
    pthread_cond_t finishCond;	    /* Conditional to allow waiting until non-empty. */
    };

static void pthreadWorkListFree(struct pthreadWorkList **pPwl)
/* Free up memory resources associated with work list */
{
struct pthreadWorkList *pwl = *pPwl;
if (pwl != NULL)
    {
    synQueueFree(&pwl->inQueue);
    freez(&pwl->pthreads);
    pthreadCondDestroy(&pwl->finishCond);
    pthreadMutexDestroy(&pwl->finishMutex);
    freez(pPwl);
    }
}

static void *pthreadWorker(void *vWorkList)
/* The worker-bee for a workList thread.  This grabs next input, processes it, and
 * puts result on output queue.  It returns when no more input. */
{
struct pthreadWorkList *pwl = vWorkList;
void *item;
while ((item = synQueueGrab(pwl->inQueue)) != NULL)
    {
    /* Do work on the item. */
    pwl->worker(item, pwl->context);

    /* Decrement toDoCount, a protected variable, and wake up manager. */
    pthreadMutexLock(&pwl->finishMutex);
    pwl->toDoCount -= 1;
    pthreadCondSignal(&pwl->finishCond);
    pthreadMutexUnlock(&pwl->finishMutex);
    }
return NULL;
}

void pthreadDoList(int threadCount, void *workList,  PthreadListWorker *worker, void *context)
/* Work through list with threadCount workers each in own thread. 
 * The worker will be called in parallel with an item from work list and the context
 *       worker(item, context)
 * The context is constant across all threads and items. */
{
if (workList == NULL)
    return;
if (threadCount < 1 || threadCount > 256)
    errAbort("pthreadDoList - threadCount %d, but must be between 1 and 256", threadCount);

/* Allocate basic structure */
struct pthreadWorkList *pwl;
AllocVar(pwl);
pwl->threadCount = threadCount;
pwl->itemList = workList;
pwl->context = context;
pwl->worker = worker;
AllocArray(pwl->pthreads, threadCount);
pwl->inQueue = synQueueNew();

/* Loop through all items in list and add them to inQueue */
struct slList *item;
int count = 0;
for (item = pwl->itemList; item != NULL; item = item->next)
    {
    synQueuePut(pwl->inQueue, item);
    ++count;
    }

/* Initialize item counting stuff */
pwl->itemCount = pwl->toDoCount = count;
pthreadMutexInit(&pwl->finishMutex);
pthreadCondInit(&pwl->finishCond);

/* Start up pthreads */
int i;
for (i=0; i<threadCount; ++i)
    pthreadCreate(&pwl->pthreads[i], NULL, pthreadWorker, pwl);

/* Wait until nothing more to do */
pthreadMutexLock(&pwl->finishMutex);
while (pwl->toDoCount > 0)
    pthreadCondWait(&pwl->finishCond, &pwl->finishMutex);
pthreadMutexUnlock(&pwl->finishMutex);

/* Wait for threads to finish*/
for (i=0; i<threadCount; ++i)
    {
    pthread_join(pwl->pthreads[i], NULL);
    }

/* Clean up */
pthreadWorkListFree(&pwl);
}

