/* SockSuck - A process that sucks messages from a socket
 * and puts them in the hub queue. */
#include "paraCommon.h"
#include "paraHub.h"
#include "net.h"

static pthread_t sockSuckThread;

static void *sockSuckDaemon(void *vptr)
/* Shovel messages from short socket queue to our
 * larger hub message queue. */
{
struct paraMessage *pm;
struct rudp *ru = vptr;
for (;;)
    {
    AllocVar(pm);
    if (pmReceive(pm, ru))
	{
	// Listening on AF_INET6 with hybrid dual stack, ipv4 addresses are ipv4-mapped.
	struct sockaddr_in6 *sai6 = (struct sockaddr_in6 *)&pm->ipAddress;
	if (internetIpInSubnetCidr(&sai6->sin6_addr, hubSubnet))
	    {
	    hubMessagePut(pm);
	    }
	else
	    {
	    char ipStr[INET6_ADDRSTRLEN];
	    if (inet_ntop(AF_INET6, &sai6->sin6_addr, ipStr, sizeof(ipStr))) 
		{
		warn("unauthorized access by %s", ipStr);
		freez(&pm);
		}
	    }
	}
    else
	freez(&pm);
    }
return NULL;  // avoid compiler warning
}


void sockSuckStart(struct rudp *ru)
/* Start socket sucker deamon.  */
{
int err = pthread_create(&sockSuckThread, NULL, sockSuckDaemon, ru);
if (err < 0)
    errAbort("Couldn't create sockSuckThread %s", strerror(err));
}
