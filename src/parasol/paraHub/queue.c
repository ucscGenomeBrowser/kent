/* queue - routines to manage paraHub central message queue. 
 * The queue is just a doubly-linked list with some protection
 * so that multiple threads don't trip on it.  */

#include "paraCommon.h"
#include "synQueue.h"

static struct synQueue *sq;

void hubMessageQueueInit()
/* Setup stuff for hub message queue.  Must be called once
 * at the beginning before spawning threads. */
{
if (sq != NULL)
    errAbort("You can't call hubMessageQueueInit twice");
sq = synQueueNew();
}

struct paraMessage *hubMessageGet()  
/* Get message from central queue, waiting if necessary for one to appear. 
 * Do a paraMessageFree when you're done with this message. */
{
return synQueueGet(sq);
}

void hubMessagePut(struct paraMessage *pm)
/* Add message to central queue.  Message must be dynamically allocated. */
{
return synQueuePut(sq, pm);
}
     
