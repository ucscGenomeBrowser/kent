/* SockSuck - A process that sucks messages from a socket
 * and puts them in the hub queue. */
#include "paraCommon.h"
#include "paraHub.h"

static pthread_t sockSuckThread;

static void *sockSuckDeamon(void *vptr)
/* Shovel messages from short socket queue to our
 * larger hub message queue. */
{
struct paraMessage *pm;
struct rudp *ru = vptr;
for (;;)
    {
    AllocVar(pm);
    if (pmReceive(pm, ru))
	hubMessagePut(pm);
    else
	freez(&pm);
    }
}


void sockSuckStart(struct rudp *ru)
/* Start socket sucker deamon.  */
{
int err = pthread_create(&sockSuckThread, NULL, sockSuckDeamon, ru);
if (err < 0)
    errAbort("Couldn't create sockSuckThread %s", strerror(err));
}
