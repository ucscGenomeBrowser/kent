/* internet - some stuff to make it easier to use
 * internet sockets and the like. */
#include "common.h"
#include "internet.h"

boolean internetFillInAddress(char *hostName, int port, struct sockaddr_in *address)
/* Fill in address. Return FALSE if can't.  */
{
struct hostent *hostent;
ZeroVar(address);
address->sin_family = AF_INET;
address->sin_port = htons(port);
if (hostName == NULL)
    address->sin_addr.s_addr = INADDR_ANY;
else
    {
    hostent = gethostbyname(hostName);
    if (hostent == NULL)
	{
	warn("Couldn't find host %s. h_errno %d (%s)", hostName, h_errno, hstrerror(h_errno));
	return FALSE;
	}
    memcpy(&address->sin_addr.s_addr, hostent->h_addr_list[0], sizeof(address->sin_addr.s_addr));
    }
return TRUE;
}
