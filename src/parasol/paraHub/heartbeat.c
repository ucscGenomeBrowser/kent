/* This daemon just sends a heartbeat message to the hub
 * every now and again. */

#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "common.h"
#include "paraLib.h"
#include "net.h"
#include "paraHub.h"

int heartPid;	/* Process id of heartbeat. */

void heartbeatDeamon()
/* Send out a beat every now and again to hub. */
{
int hubFd;
int sigSize = strlen(paraSig);
char portName[16];
sprintf(portName, "%d", paraPort);
for (;;)
    {
    sleep(10);
    hubFd = netConnect("localhost", portName);
    if (hubFd > 0)
        {
	write(hubFd, paraSig, sigSize);
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
