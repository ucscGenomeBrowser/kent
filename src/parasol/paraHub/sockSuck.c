/* SockSuck - A process that sucks messages from a socket
 * and puts them in the hub queue. */
#include "paraCommon.h"
#include "paraHub.h"

static pthread_t sockSuckThread;
unsigned char localHost[4] = {127,0,0,1};   /* Address for local host in network order */

boolean ipAddressOk(in_addr_t packed, unsigned char *spec)
/* Return TRUE if packed IP address matches spec. */
{
unsigned char unpacked[4];
internetUnpackIp(packed, unpacked);
return internetIpInSubnet(unpacked, spec);
}

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
	if (ipAddressOk(pm->ipAddress.sin_addr.s_addr, hubSubnet) || 
	    ipAddressOk(pm->ipAddress.sin_addr.s_addr, localHost))
	    {
	    hubMessagePut(pm);
	    }
	else
	    {
	    char dottedQuad[17];
	    internetIpToDottedQuad(ntohl(pm->ipAddress.sin_addr.s_addr), dottedQuad);
	    warn("unauthorized access by %s", dottedQuad);
	    freez(&pm);
	    }
	}
    else
	freez(&pm);
    }
}


void sockSuckStart(struct rudp *ru)
/* Start socket sucker deamon.  */
{
int err = pthread_create(&sockSuckThread, NULL, sockSuckDaemon, ru);
if (err < 0)
    errAbort("Couldn't create sockSuckThread %s", strerror(err));
}
