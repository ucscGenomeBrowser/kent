/* internet - some stuff to make it easier to use
 * internet sockets and the like. */
#include "common.h"
#include "internet.h"

bits32 internetHostIp(char *hostName)
/* Get IP v4 address (in network byte order) for hostName.
 * Warn and return 0 if there's a problem. */
{
struct hostent *hostent;
bits32 ret;
hostent = gethostbyname(hostName);
if (hostent == NULL)
    {
    warn("Couldn't find host %s. h_errno %d (%s)", hostName, h_errno, hstrerror(h_errno));
    return 0;
    }
memcpy(&ret, hostent->h_addr_list[0], sizeof(ret));
return ret;
}

boolean internetFillInAddress(char *hostName, int port, struct sockaddr_in *address)
/* Fill in address. Return FALSE if can't.  */
{
ZeroVar(address);
address->sin_family = AF_INET;
address->sin_port = htons(port);
if (hostName == NULL)
    address->sin_addr.s_addr = INADDR_ANY;
else
    {
    if ((address->sin_addr.s_addr = internetHostIp(hostName)) == 0)
	return FALSE;
    }
return TRUE;
}
