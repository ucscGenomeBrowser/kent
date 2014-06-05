/* This daemon just sends a heartbeat message to the hub
 * every now and again. */

#include "paraCommon.h"
#include "errAbort.h"
#include "net.h"
#include "paraHub.h"

static pthread_t heartbeatThread;
static boolean alive = TRUE;

static void *heartbeatDeamon(void *vptr)
/* Send out a beat every now and again to hub. */
{
struct paraMessage *pm;
while (alive)
    {
    sleep(MINUTE/4);
    findNow();
    pm = pmNew(0,0);
    pmSet(pm, "heartbeat");
    hubMessagePut(pm);
    }
return NULL;
}

void endHeartbeat()
/* Kill heartbeat deamon.  This won't take effect right away
 * though. */
{
alive = FALSE;
}

void startHeartbeat()
/* Start heartbeat deamon. */
{
int err = pthread_create(&heartbeatThread, NULL, heartbeatDeamon, NULL);
if (err < 0)
    errAbort("Couldn't create heartbeatThread %s", strerror(err));
}
