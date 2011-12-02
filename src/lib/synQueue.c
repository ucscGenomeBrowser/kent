/* synQueue - a sychronized message queue for messages between
 * threads. */

#include "common.h"
#include "dlist.h"
#include "pthreadWrap.h"
#include "synQueue.h"


struct synQueue
/* A synchronized queue for messages between threads. */
    {
    struct synQueue *next;	/* Next in list of queues. */
    struct dlList *queue;	/* The queue itself. */
    pthread_mutex_t mutex;	/* Mutex to prevent simultanious access. */
    pthread_cond_t cond;	/* Conditional to allow waiting until non-empty. */
    };

struct synQueue *synQueueNew()
/* Make a new, empty, synQueue. */
{
struct synQueue *sq;
AllocVar(sq);
pthreadMutexInit(&sq->mutex);
pthreadCondInit(&sq->cond);
sq->queue = dlListNew();
return sq;
}

void synQueueFree(struct synQueue **pSq)
/* Free up synQueue.  Be sure no other threads are using
 * it first though! This will not free any dynamic memory
 * in the messages.  Use synQueueFreeAndVals for that. */
{
struct synQueue *sq = *pSq;
if (sq == NULL)
    return;
dlListFree(&sq->queue);
pthreadCondDestroy(&sq->cond);
pthreadMutexDestroy(&sq->mutex);
freez(pSq);
}

void synQueueFreeAndVals(struct synQueue **pSq)
/* Free up synQueue.  Be sure no other threads are using
 * it first though! This will freeMem all the messages */
{
struct synQueue *sq = *pSq;
if (sq == NULL)
    return;
dlListFreeAndVals(&sq->queue);
pthreadCondDestroy(&sq->cond);
pthreadMutexDestroy(&sq->mutex);
freez(pSq);
}

void synQueuePut(struct synQueue *sq, void *message)
/* Add message to end of queue. */
{
pthreadMutexLock(&sq->mutex);
dlAddValTail(sq->queue, message);
pthreadCondSignal(&sq->cond);
pthreadMutexUnlock(&sq->mutex);
}

void *synQueueGet(struct synQueue *sq)
/* Get message off start of queue.  Wait until there is
 * a message if queue is empty. */
{
void *message;
struct dlNode *node;
pthreadMutexLock(&sq->mutex);
while (dlEmpty(sq->queue))
    pthreadCondWait(&sq->cond, &sq->mutex);
node = dlPopHead(sq->queue);
pthreadMutexUnlock(&sq->mutex);
message = node->val;
freeMem(node);
return message;
}

void *synQueueGrab(struct synQueue *sq)
/* Get message off start of queue.  Return NULL immediately 
 * if queue is empty. */
{
void *message = NULL;
struct dlNode *node;
pthreadMutexLock(&sq->mutex);
node = dlPopHead(sq->queue);
pthreadMutexUnlock(&sq->mutex);
if (node != NULL)
    {
    message = node->val;
    freeMem(node);
    }
return message;
}

int synQueueSize(struct synQueue *sq)
/* Return number of messages currently on queue. */
{
int size;
pthreadMutexLock(&sq->mutex);
size = dlCount(sq->queue);
pthreadMutexUnlock(&sq->mutex);
return size;
}
