/* This daemon just sends a heartbeat message to the hub
 * every now and again. */

#include <signal.h>
#include "common.h"
#include "errabort.h"
#include "paraLib.h"
#include "net.h"
#include "paraHub.h"

static int heartPid;	/* Process id of heartbeat. */

static void heartbeatDeamon()
/* Send out a beat every now and again to hub. */
{
int hubFd;
for (;;)
    {
    sleep(30);
    hubFd = hubConnect();
    if (hubFd > 0)
        {
	netSendLongString(hubFd, "heartbeat");
	close(hubFd);
	}
    }
}

void endHeartbeat()
/* Kill heartbeat deamon. */
{
if (heartPid != 0)
    {
    kill(heartPid, SIGTERM);
    heartPid = 0;
    }
}

void startHeartbeat()
/* Start heartbeat deamon. */
{
int pid = fork();
if (pid < 0)
    errAbort("Couldn't start heartbeat.");
if (pid == 0)
    heartbeatDeamon();
else
    heartPid = pid;
}
