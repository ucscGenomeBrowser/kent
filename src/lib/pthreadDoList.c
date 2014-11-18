/* pthreadDoList - do something to all items in list using multiple threads in parallel. 
 * See pthreadDoList.h for documentation on how to use. */

#include "common.h"
#include "pthreadWrap.h"
#include "synQueue.h"
#include "pthreadDoList.h"
#include "dlist.h"

struct pthreadWorkList
/* Manage threads to work on a list of items in parallel. */
    {
    struct pthreadWorkList *next;   /* Next in list */
    void *context;		    /* Constant context data for each item */
    PthreadListWorker *worker;   /* Routine that does work in a single thread */
    pthread_t *pthreads;		    /* Array of pthreads */
    struct synQueue *inQueue;	    /* Put all inputs here */
    };

static void pthreadWorkListFree(struct pthreadWorkList **pPwl)
/* Free up memory resources associated with work list */
{
struct pthreadWorkList *pwl = *pPwl;
if (pwl != NULL)
    {
    synQueueFree(&pwl->inQueue);
    freez(&pwl->pthreads);
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
pwl->context = context;
pwl->worker = worker;
AllocArray(pwl->pthreads, threadCount);
struct synQueue *inQueue = pwl->inQueue = synQueueNew();

/* Loop through all items in list and add them to inQueue */
struct slList *item;
for (item = workList; item != NULL; item = item->next)
    synQueuePutUnprotected(inQueue, item);

/* Start up pthreads */
int i;
for (i=0; i<threadCount; ++i)
    pthreadCreate(&pwl->pthreads[i], NULL, pthreadWorker, pwl);

/* Wait for threads to finish*/
for (i=0; i<threadCount; ++i)
    pthread_join(pwl->pthreads[i], NULL);

/* Clean up */
pthreadWorkListFree(&pwl);
}

