/* internet - some stuff to make it easier to use
 * internet sockets and the like. */
#include "common.h"
#include "internet.h"

static char const rcsid[] = "$Id: internet.c,v 1.11 2010/03/11 17:54:35 angie Exp $";

boolean internetIsDottedQuad(char *s)
/* Returns TRUE if it looks like s is a dotted quad. */
{
int i;
if (!isdigit(s[0]))
    return FALSE;
for (i=0; i<3; ++i)
    {
    s = strchr(s, '.');
    if (s == NULL)
        return FALSE;
    s += 1;
    if (!isdigit(s[0]))
        return FALSE;
    }
return TRUE;
}

bits32 internetHostIp(char *hostName)
/* Get IP v4 address (in host byte order) for hostName.
 * Warn and return 0 if there's a problem. */
{
bits32 ret;
if (internetIsDottedQuad(hostName))
    {
    internetDottedQuadToIp(hostName, &ret);
    }
else
    {
    /* getaddrinfo is thread-safe and widely supported */
    struct addrinfo hints, *res;
    struct in_addr addr;
    int err;

    zeroBytes(&hints, sizeof(hints));
    hints.ai_family = AF_INET;

    if ((err = getaddrinfo(hostName, NULL, &hints, &res)) != 0) 
	{
	warn("getaddrinfo() error on hostName=%s: %s\n", hostName, gai_strerror(err));
	return 0;
	}

    addr = ((struct sockaddr_in *)(res->ai_addr))->sin_addr;

    ret = ntohl((uint32_t)addr.s_addr);

    freeaddrinfo(res);

    }
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
#ifndef __CYGWIN32__
struct in_addr ia;
zeroBytes(dottedQuad, 17);
ZeroVar(&ia);
ia.s_addr = htonl(ip);
if (inet_ntop(AF_INET, &ia, dottedQuad, 16) == NULL)
    {
    warn("conversion problem on 0x%x in internetIpToDottedQuad: %s", 
    	ip, strerror(errno));
    return FALSE;
    }
return TRUE;
#else
warn("Sorry, internetIpToDottedQuad not supported in Windows.");
return FALSE;
#endif
}

boolean internetDottedQuadToIp(char *dottedQuad, bits32 *retIp)
/* Convert dotted quad format address to IP4 address in
 * host byte order.  Warn and return FALSE if there's a 
 * problem. */
{
#ifndef __CYGWIN32__
struct in_addr ia;
if (inet_pton(AF_INET, dottedQuad, &ia) < 0)
    {
    warn("internetDottedQuadToIp problem on %s: %s", dottedQuad, strerror(errno));
    return FALSE;
    }
*retIp = ntohl(ia.s_addr);
return TRUE;
#else
warn("Sorry, internetDottedQuadToIp not supported in Windows.");
return FALSE;
#endif
}

void internetParseDottedQuad(char *dottedQuad, unsigned char quad[4])
/* Parse dotted quads into quad */
{
char *s = dottedQuad;
int i;
if (!internetIsDottedQuad(s))
    errAbort("%s is not a dotted quad", s);
for (i=0; i<4; ++i)
    {
    quad[i] = atoi(s);
    s = strchr(s, '.') + 1;
    }
}

void internetUnpackIp(bits32 packed, unsigned char unpacked[4])
/* Convert from 32 bit to 4-byte format with most significant
 * byte first. */
{
int i;
for (i=3; i>=0; --i)
    {
    unpacked[i] = (packed&0xff);
    packed >>= 8;
    }
}

boolean internetIpInSubnet(unsigned char unpackedIp[4], unsigned char subnet[4])
/* Return true if unpacked IP address is in subnet. */
{
int i;
for (i=0; i<4; ++i)
    {
    unsigned char c = subnet[i];
    if (c == 255)
        return TRUE;
    if (c != unpackedIp[i])
        return FALSE;
    }
return TRUE;
}

