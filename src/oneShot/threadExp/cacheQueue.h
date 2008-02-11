/* cacheQueue - a cache of synchronized message queues.
 * ParaFlow uses these to implement the thread synchronization
 * when parallel processing. */

#ifndef CACHEQUEUE_H
#define CACHEQUEUE_H

struct synQueue *cacheQueueAlloc();
/* Get a synQueue from cache */
    
void cacheQueueFree(struct synQueue **pQueue);
/* Free up queue back to hash. The queue must be empty. */


#endif /* CACHEQUEUE_H */

