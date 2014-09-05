/* synQueue - a sychronized message queue for messages between
 * threads. */

#ifndef SYNQUEUE_H
#define SYNQUEUE_H

struct synQueue *synQueueNew();
/* Make a new, empty, synQueue. */

void synQueueFree(struct synQueue **pSq);
/* Free up synQueue.  Be sure no other threads are using
 * it first though! This will not free any dynamic memory
 * in the messages.  Use synQueueFreeAndVals for that. */

void synQueueFreeAndVals(struct synQueue **pSq);
/* Free up synQueue.  Be sure no other threads are using
 * it first though! This will freeMem all the messages */

void synQueuePut(struct synQueue *sq, void *message);
/* Add message to end of queue. */

void synQueuePutUnprotected(struct synQueue *sq, void *message);
/* Add message to end of queue without protecting against multithreading
 * contention - used before pthreads are launched perhaps. */

void *synQueueGet(struct synQueue *sq);
/* Get message off start of queue.  Wait until there is
 * a message if queue is empty. */

void *synQueueGrab(struct synQueue *sq);
/* Get message off start of queue.  Return NULL immediately 
 * if queue is empty. */

int synQueueSize(struct synQueue *sq);
/* Return number of messages currently on queue. */

#endif /* SYNQUEUE_H */

