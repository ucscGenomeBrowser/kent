/* queue - routines to manage paraHub central message queue. 
 * The queue is just a doubly-linked list with some protection
 * so that multiple threads don't trip on it.  */

#include "paraCommon.h"
#include "dlist.h"
#include "paraHub.h"

void mutexLock(pthread_mutex_t *mutex)
/* Lock a mutex or die trying. */
{
int err = pthread_mutex_lock(mutex);
if (err != 0)
    errAbort("Couldn't lock mutex: %s", strerror(err));
}

void mutexUnlock(pthread_mutex_t *mutex)
/* Lock a mutex or die trying. */
{
int err = pthread_mutex_unlock(mutex);
if (err != 0)
    errAbort("Couldn't lock mutex: %s", strerror(err));
}

void condSignal(pthread_cond_t *cond)
/* Set conditional signal */
{
int err = pthread_cond_signal(cond);
if (err != 0)
    errAbort("Couldn't condSignal: %s", strerror(err));
}

void condWait(pthread_cond_t *cond, pthread_mutex_t *mutex)
/* Wait for conditional signal. */
{
int err = pthread_cond_wait(cond, mutex);
if (err != 0)
    errAbort("Couldn't contWait: %s", strerror(err));
}

static struct dlList *hubQueue;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER;

void hubMessageQueueInit()
/* Setup stuff for hub message queue.  Must be called once
 * at the beginning before spawning threads. */
{
if (hubQueue != NULL)
    errAbort("You can't call hubMessageQueueInit twice");
hubQueue = dlListNew();
}

struct paraMessage *hubMessageGet()  
/* Get message from central queue, waiting if necessary for one to appear. 
 * Do a paraMessageFree when you're done with this message. */
{
struct dlNode *node;
struct paraMessage *pm;
mutexLock(&mutex);
while (dlEmpty(hubQueue))
    condWait(&ready, &mutex);
node = dlPopHead(hubQueue);
mutexUnlock(&mutex);
pm = node->val;
freeMem(node);
return pm;
}

void hubMessagePut(struct paraMessage *pm)
/* Add message to central queue.  Message must be dynamically allocated. */
{
mutexLock(&mutex);
dlAddValTail(hubQueue, pm);
condSignal(&ready);
mutexUnlock(&mutex);
}
     
