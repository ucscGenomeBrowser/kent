/* SockSuck - A process that sucks messages from a socket
 * and puts them in the hub queue. */
#include "paraCommon.h"
#include "paraHub.h"

static pthread_t sockSuckThread;
unsigned char localHost[4] = {127,0,0,1};	   /* Address for local host */

void unpackIp(in_addr_t packed, unsigned char unpacked[4])
/* Unpack IP address into 4 bytes. */
{
/* We cannot use sizeof(unpacked) because an array on a stack 
 * is just a pointer, and so pointer-size is all that sizeof returns 
 * for unpacked. We'll use the size of the source instead. */
memcpy(unpacked, &packed, sizeof(packed));
}

boolean ipAddressOk(in_addr_t packed, unsigned char *spec)
/* Return TRUE if packed IP address matches spec. */
{
unsigned char unpacked[4], c;
int i;
unpackIp(packed, unpacked);
for (i=0; i<sizeof(unpacked); ++i)
    {
    c = spec[i];
    if (c == 255)
        break;
    if (c != unpacked[i])
        return FALSE;
    }
return TRUE;
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
	/* FIXME: I think there may be some byte order issues here. 
	 * that are hidden under the Pentiums we are using. */
	if (ipAddressOk(pm->ipAddress.sin_addr.s_addr, hubSubnet) || 
	    ipAddressOk(pm->ipAddress.sin_addr.s_addr, localHost))
	    {
	    hubMessagePut(pm);
	    }
	else
	    {
	    unsigned char ip[4];
	    unpackIp(pm->ipAddress.sin_addr.s_addr, ip);
	    warn("unauthorized access by %d.%d.%d.%d\n", 
                 ip[0], ip[1], ip[2], ip[3]);
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
