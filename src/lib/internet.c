static char const rcsid[] = "$Id: internet.c,v 1.6 2003/05/05 06:45:34 kate Exp $";

/* internet - some stuff to make it easier to use
 * internet sockets and the like. */
#include "common.h"
#include "internet.h"

bits32 internetHostIp(char *hostName)
/* Get IP v4 address (in host byte order) for hostName.
 * Warn and return 0 if there's a problem. */
{
struct hostent *hostent;
bits32 ret;
hostent = gethostbyname(hostName);
if (hostent == NULL)
    {
    warn("Couldn't find host %s. h_errno %d", hostName, h_errno);
    return 0;
    }
memcpy(&ret, hostent->h_addr_list[0], sizeof(ret));
ret = ntohl(ret);
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
    if ((address->sin_addr.s_addr = htonl(internetHostIp(hostName))) == 0)
	return FALSE;
    }
return TRUE;
}

boolean internetIpToDottedQuad(bits32 ip, char dottedQuad[17])
/* Convert IP4 address in host byte order to dotted quad 
 * notation.  Warn and return FALSE if there's a 
 * problem. */
{
struct in_addr ia;
zeroBytes(dottedQuad, 17);
ZeroVar(&ia);
ia.s_addr = htonl(ip);
if (inet_ntop(AF_INET, &ia, dottedQuad, 16) == NULL)
    {
    warn("conversion problem on %x in internetIpToDottedQuad: %s", 
    	strerror(errno));
    return FALSE;
    }
return TRUE;
}

boolean internetDottedQuadToIp(char *dottedQuad, bits32 *retIp)
/* Convert dotted quad format address to IP4 address in
 * host byte order.  Warn and return FALSE if there's a 
 * problem. */
{
struct in_addr ia;
if (inet_pton(AF_INET, dottedQuad, &ia) < 0)
    {
    warn("internetDottedQuadToIp problem on %s: %s", strerror(errno));
    return FALSE;
    }
*retIp = ntohl(ia.s_addr);
return TRUE;
}
