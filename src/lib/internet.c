/* internet - some stuff to make it easier to use
 * internet sockets and the like. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "internet.h"


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

bits32 internetPackIp(unsigned char unpacked[4])
/* Convert from 4-byte format with most significant
 * byte first to native 32-bit format. */
{
int i;
bits32 packed = 0;
for (i=0; i<=3; ++i)
    {
    packed <<= 8;
    packed |= unpacked[i];
    }
return packed;
}

void internetCidrRange(struct cidr *cidr, bits32 *pStartIp, bits32 *pEndIp)
/* get range of CIDR formatted subnet as start and end IPs. */
{
bits32 packedIp = cidr->ip;
int bits = cidr->subnetLength;
int r = 32 - bits;
bits32 start = packedIp & (((unsigned int) 0xFFFFFFFF) << r);
bits32 end;
// shr or shl 32 of a 32-bit value 
//  does nothing at all rather than turning it to zeroes.
if (bits == 32)
    end = packedIp;
else
    end = packedIp | (((unsigned int) 0xFFFFFFFF) >> bits);

*pStartIp = start;
*pEndIp = end;

}

boolean internetIpInSubnetCidr(unsigned char unpackedIp[4], struct cidr *cidr)
/* Return true if unpacked IP address is in subnet cidr. */
{
bits32 subnetIp = cidr->ip;
//printf("packed32 bits=%u %08x\n", subnetIp, subnetIp);   // DEBUG REMOVE
int r = 32 - cidr->subnetLength;
bits32 caremask = subnetIp & (((unsigned int) 0xFFFFFFFF) << r);

bits32 packedIp = internetPackIp(unpackedIp);

if ((subnetIp & caremask) == (packedIp & caremask))
    return TRUE;

return FALSE;
}

static void notGoodSubnetCidr(char *sns)
/* Complain about subnet format. */
{
errAbort("'%s' is not a properly formatted subnet.  Subnets must consist of\n"
         "one to four dot-separated numbers between 0 and 255 \n"
	 "optionally followed by an slash and subnet bit length integer between 1 and 32.\n"
	 "A trailing dot on the subnet IP address is not allowed.", sns);
}

struct cidr *internetParseSubnetCidr(char *cidr)
/* parse input CIDR format IP for range or subnet */
{
if (!cidr)
    return NULL;  
char *s = cloneString(cidr);
char *c = strchr(s, '/');
char *ip = s;
int bits = -1;
if (c) // has slash
    {
    *c++ = 0;
    bits = atoi(c);
    }
if (ip[strlen(ip)-1] == '.')  // trailing dot not allowed in subnet ip
    notGoodSubnetCidr(cidr);
char *snsCopy = cloneString(ip);
char expIp[17];
char *words[5];
int wordCount, i;
expIp[0] = 0;
wordCount = chopByChar(snsCopy, '.', words, ArraySize(words));
if (wordCount > 4 || wordCount < 1)
    notGoodSubnetCidr(cidr);
for (i=0; i<4; ++i)
    {
    int x = 0; // does not matter what these bits are, it is just filler.
    char *s = "0";
    if (i<wordCount)
	{
	s = words[i];
	if (!isdigit(s[0]))
	    notGoodSubnetCidr(cidr);
	x = atoi(s);
	if (x > 255)
	    notGoodSubnetCidr(cidr);
	}
    safecat(expIp, sizeof expIp, s);
    if (i < 3)
	safecat(expIp, sizeof expIp, ".");
    }
if (bits == -1)
    bits = 8 * wordCount;
if ((bits > 32) || (bits < 1))
    notGoodSubnetCidr(cidr);
ip = expIp;

unsigned char quadIp[4];
internetParseDottedQuad(ip, quadIp);
bits32 packedIp = 0;
packedIp = internetPackIp(quadIp);
struct cidr *result;
AllocVar(result);
result->ip = packedIp;
result->subnetLength = bits;
return result;
}

