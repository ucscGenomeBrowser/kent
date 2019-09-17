/* internet - some stuff to make it easier to use
 * internet sockets and the like. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "internet.h"

boolean isIpv4Address(char *ipStr)
/* Return TRUE if string is a properly formatted IPV4 string */
{
/* valid IPv4 text address? */
struct in_addr addr;
return (inet_pton(AF_INET, ipStr, &addr) == 1);
}

boolean isIpv6Address(char *ipStr)
/* Return TRUE if string is a properly formatted IPV6 string */
{
/* valid IPv6 text address? */
struct in6_addr addr;
return (inet_pton(AF_INET6, ipStr, &addr) == 1);
}

char *familyToString(sa_family_t family)
/* Convert family to printable string */
{
char *familyStr;
switch (family)
    {
    case AF_INET:
	familyStr = "IPv4";
	break;
    case AF_INET6:
	familyStr = "IPv6";
	break;
    default:
	familyStr = "unknown";
	break;            
    }
return familyStr;
}

boolean internetGetAddrInfo6n4(char *server, char *servPort, struct addrinfo **pAddress)
/* Fill in address. Return FALSE if can't. Returns info in addrinfo. Free address with freeaddrinfo(). */
{
int rc;

struct addrinfo hints;
ZeroVar(&hints);
hints.ai_flags    = AI_NUMERICSERV;
hints.ai_family   = AF_UNSPEC;
hints.ai_socktype = SOCK_STREAM;

if (server == NULL)  // USED by functions for listen/accept server
    {
    hints.ai_flags = AI_PASSIVE; // use my IP address
    }
else
    {

    /********************************************************************/
    /* Check if we were provided the address of the server using        */
    /* inet_pton() to convert the text form of the address to binary    */
    /* form. If it is numeric then we want to prevent getaddrinfo()     */
    /* from doing any name resolution.                                  */
    /********************************************************************/
    if (isIpv4Address(server))     /* valid IPv4 text address? */
	{
	hints.ai_family = AF_INET;
	hints.ai_flags |= AI_NUMERICHOST;
	}
    else
	{
	if (isIpv6Address(server)) /* valid IPv6 text address? */
	    {
	    hints.ai_family = AF_INET6;
	    hints.ai_flags |= AI_NUMERICHOST;
	    }
	}
    }
/********************************************************************/
/* Get the address information for the server using getaddrinfo().  */
/********************************************************************/
rc = getaddrinfo(server, servPort, &hints, pAddress);
if (rc != 0)
    {
    if (rc == EAI_SYSTEM)
	perror("getaddrinfo() failed");
    warn("Host %s not found --> %s\n", server, gai_strerror(rc));
    return FALSE;
    }

return TRUE;
}

boolean internetFillInAddress6n4(char *hostStr, char *portStr, 
    sa_family_t family, int socktype, struct sockaddr_storage *sai,
    boolean ipOnly)
/* Fill in address. hostStr and portStr are strings that contain ipv4 or ipv6 addresses and port numbers. 
 * hostStr can also be a name, but it aborts if ipOnly specified. Returns TRUE if result found. */
{
int rc;
boolean result = FALSE;

struct addrinfo hints;
ZeroVar(&hints);
hints.ai_flags |= AI_NUMERICSERV;
hints.ai_family   = family;  // AF_INET, AF_INET6, AF_UNSPEC
hints.ai_socktype = socktype;// SOCK_DGRAM, SOCK_STREAM

struct addrinfo *resList = NULL;

/********************************************************************/
/* Check if we were provided the address of the server using        */
/* inet_pton() to convert the text form of the address to binary    */
/* form. If it is numeric then we want to prevent getaddrinfo()     */
/* from doing any name resolution.                                  */
/********************************************************************/
if (hostStr == NULL)  // USED by functions for listen/accept server
    {
    hints.ai_flags = AI_PASSIVE; // use my IP address
    }
else
    {
    if (isIpv4Address(hostStr))     /* valid IPv4 text address? */
	{
	hints.ai_family = AF_INET;
	hints.ai_flags |= AI_NUMERICHOST;
	}
    else
	{
	if (isIpv6Address(hostStr)) /* valid IPv6 text address? */
	    {
	    hints.ai_family = AF_INET6;
	    hints.ai_flags |= AI_NUMERICHOST;
	    }
	else
	    {
	    if (ipOnly)
		{
		errAbort("hostStr=[%s] not an ipv6 or ipv4 address", hostStr);
		}
	    }
	}
    }
/********************************************************************/
/* Get the address information for the server using getaddrinfo().  */
/********************************************************************/
// host or port can be null but not both.
rc = getaddrinfo(hostStr, portStr, &hints, &resList);
if (rc != 0)
    {
    if (rc == EAI_SYSTEM)
	perror("getaddrinfo() failed");
    errAbort("Host %s not found --> %s\n", hostStr, gai_strerror(rc));
    }

if (resList)
    {
    memcpy(sai, resList->ai_addr, resList->ai_addrlen);
    result = TRUE;
    }
freeaddrinfo(resList);
return result;
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

void ip6AddrCopy(struct in6_addr *src, struct in6_addr *dst)
/* copy 16 byte ipv6 address from source to destination */
{
memcpy(dst, src, sizeof (struct in6_addr));
}


void ip6AddrToHexStr(struct in6_addr *ip, char *s, int size)
/* show 16 byte ipv6 address as hex string. size >= 33.*/
{
safef(s, size, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x" 
	, ip->s6_addr[0]
	, ip->s6_addr[1]
	, ip->s6_addr[2]
	, ip->s6_addr[3]
	, ip->s6_addr[4]
	, ip->s6_addr[5]
	, ip->s6_addr[6]
	, ip->s6_addr[7]
	, ip->s6_addr[8]
	, ip->s6_addr[9]
	, ip->s6_addr[10]
	, ip->s6_addr[11]
	, ip->s6_addr[12]
	, ip->s6_addr[13]
	, ip->s6_addr[14]
	, ip->s6_addr[15]
    );
}

void ip6AddrShowAsHex(struct in6_addr *ip)
/* show 16 byte ipv6 address as hex */
{
int i;
for (i=0; i < sizeof(struct in6_addr); ++i)
    {
    printf("%02x", ip->s6_addr[i]);
    }
printf("\n");
}

void ip6AddrFlipBits(struct in6_addr *ip)
/* flip all bits in ipv6 addr by XOR. */
{
int i;
for (i=0; i < sizeof(struct in6_addr); ++i)
    {
    ip->s6_addr[i] = ip->s6_addr[i] ^ 0xFF;  // xor
    }
}

void ip6AddrAndBits(struct in6_addr *ip1, struct in6_addr *ip2, struct in6_addr *result)
/* result = ip1 AND ip2 bits together. */
{
int i;
for (i=0; i < sizeof(struct in6_addr); ++i)
    {
    result->s6_addr[i] = ip1->s6_addr[i] & ip2->s6_addr[i]; // and
    }
}

void ip6AddrOrBits(struct in6_addr *ip1, struct in6_addr *ip2, struct in6_addr *result)
/* result = ip1 OR ip2 bits together. */
{
int i;
for (i=0; i < sizeof(struct in6_addr); ++i)
    {
    result->s6_addr[i] = ip1->s6_addr[i] | ip2->s6_addr[i]; // or
    }
}


int ip6AddrCmpBits(struct in6_addr *ip1, struct in6_addr *ip2)
/* Return signed value subracting ip2 bits from ip1 bits. 
 * Returns 0 if equal, positive if ip2 < ip1 and negative if ip1 < ip2. */
{
int i;
for (i=0; i < sizeof(struct in6_addr); ++i)
    {
    int diff = (int)ip1->s6_addr[i] - (int)ip2->s6_addr[i];
    if (diff != 0)
	return diff;
    }
return 0;
}



void ip6AddrMaskLeft(struct in6_addr *ip, int bits)
/* create mask of bits all 1 on left. */
{
if ((bits > 128) || (bits < 0))
    errAbort("bad bits %d in ip6AddrMaskLeft", bits);
int i;
for (i=0; i < sizeof(struct in6_addr); ++i)
    {
    int start = i * 8;
    int end = start + 8;
    if (bits >= end)
	ip->s6_addr[i] = 0xFF;
    else if (bits <= start)
	ip->s6_addr[i] = 0x00;
    else
	{
	int shift = (8 - (bits - start));
	ip->s6_addr[i] = (((unsigned int) 0xFF) << shift) & 0xFF;
	}
    }
}

void ip6AddrMaskRight(struct in6_addr *ip, int bits)
/* create mask of bits all 1 on right. */
{
if ((bits > 128) || (bits < 0))
    errAbort("bad bits %d in ip6AddrMaskRight", bits);
ip6AddrMaskLeft(ip, 128-bits);
ip6AddrFlipBits(ip);
}

boolean internetIpStringToIp6(char *ipStr, struct in6_addr *retIp)
/* Convert IPv6 string to IPv6 binary address in network byte order.
 * Warn and return FALSE if there's a problem. */
{
#ifndef __CYGWIN32__
struct in6_addr ia;
if (inet_pton(AF_INET6, ipStr, &ia) < 0)
    {
    warn("internetIpStringToIp6 problem on %s: %s", ipStr, strerror(errno));
    return FALSE;
    }
ip6AddrCopy(&ia, retIp);
return TRUE;
#else
warn("Sorry, internetIpStringToIp6 not supported in Windows.");
return FALSE;
#endif
}

void internetParseDottedQuad(char *dottedQuad, unsigned char quad[4])
/* Parse dotted quads into quad */
{
char *s = dottedQuad;
int i;
if (!isIpv4Address(s))
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
/* Convert unsigned char network order to bits32 host order */
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

void internetCidrRange(struct cidr *cidr, struct in6_addr *pStartIp, struct in6_addr *pEndIp)
/* get range of CIDR formatted subnet as start and end IPs. Use network byte order. */
{

struct in6_addr careMask;
ip6AddrMaskLeft(&careMask, cidr->subnetLength);

struct in6_addr start;
ip6AddrAndBits(&cidr->ipv6, &careMask, &start);

struct in6_addr flippedMask;
ip6AddrCopy(&careMask, &flippedMask);
ip6AddrFlipBits(&flippedMask);

struct in6_addr end;
ip6AddrOrBits(&cidr->ipv6, &flippedMask, &end);

*pStartIp = start;
*pEndIp = end;

}

boolean internetIpInSubnetCidr(struct in6_addr *clientIp, struct cidr *cidrList)
/* Return true if clientIp address is in one of the subnets of cidrList. Use network byte order. */
{
// Multiple subnets supports connections on both IPv4 and IPv6 CIDR subnets.
struct cidr *cidr; 
for (cidr=cidrList; cidr; cidr=cidr->next)
    {
    struct in6_addr careMask6;
    ip6AddrMaskLeft(&careMask6, cidr->subnetLength);

    struct in6_addr subMasked, clientIpMasked;
    ip6AddrAndBits(&cidr->ipv6, &careMask6, &subMasked);
    ip6AddrAndBits(clientIp, &careMask6, &clientIpMasked);
    if (ip6AddrCmpBits(&subMasked, &clientIpMasked) == 0) // they are equal
	return TRUE;
    }
return FALSE;
}

static void notGoodSubnetCidr(char *sns)
/* Complain about subnet format. */
{
errAbort("'%s' is not a properly formatted subnet.\n"
         "Subnets in IPv4 must consist of one to four dot-separated numbers between 0 and 255 \n"
	 "optionally followed by a CIDR slash and subnet bit length integer between 1 and 32, and trailing dot is not allowed.\n"
	 "Subnets in IPv6 must consist of an IPv6 address followed by a CIDR slash and 1 and 128 for IPv6 addresses.\n"
	 "Multiple subnets may be provided in a comma-separated list.", sns);
}

struct cidr *internetParseOneSubnetCidr(char *cidr)
/* parse one input CIDR format IP for range or subnet */
{
char *s = cloneString(cidr);
char *c = strchr(s, '/');
char *ip = s;
int bits = -1;
int bitsMax = 32;
if (c) // has slash
    {
    *c++ = 0;
    bits = atoi(c);
    }
boolean isIpv6 = FALSE;
if (strchr(s, ':'))
    {
    isIpv6 = TRUE;
    bitsMax = 128;
    }
if (!isIpv6)
    {
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
    ip = expIp;
    }
if ((bits > bitsMax) || (bits < 1))
    notGoodSubnetCidr(cidr);

char fullStr[INET6_ADDRSTRLEN];
safef(fullStr, sizeof fullStr, "%s", ip);

if (!isIpv6)
    {
    // prepend ::ffff: to the address for ipv4-mapped address
    safef(fullStr, sizeof fullStr, "%s%s", IPV4MAPPED_PREFIX, ip);
    bits += 96;
    }
struct cidr *result;
AllocVar(result);
result->subnetLength = bits;
if (!internetIpStringToIp6(fullStr, &result->ipv6))
    errAbort("internetIpStringToIp6 failed for %s", ip);
return result;
}

struct cidr *internetParseSubnetCidr(char *cidr)
/* parse input CIDR format IP for range or subnet */
{
if (!cidr)
    return NULL;  
struct cidr *list = NULL;

char *words[10];

// subnets are a comma-separated list of IPV4 or IPV6 CIDR subnets.
char *s = cloneString(cidr);
int wordCount = chopByChar(s, ',', words, ArraySize(words));
if (wordCount < 1)
    notGoodSubnetCidr(cidr);
int i;
for (i=0; i<wordCount; ++i)
    {
    s = words[i];
    struct cidr *cidrOne = internetParseOneSubnetCidr(words[i]);
    slAddHead(&list, cidrOne);
    }
slReverse(&list);
return list;
}

socklen_t getSockSize6n4(struct sockaddr_storage *sai)
/* figure out the size of the structure from the socket type */
{
if (sai->ss_family == AF_INET6)   //ipv6
    return sizeof (struct sockaddr_in6);
else if (sai->ss_family == AF_INET)  // ipv4
   return sizeof (struct sockaddr_in);
else
   errAbort("unknown ss_family %d in getSockSize6n4", sai->ss_family);
return -1;  // make the compiler happy.
}

void trimIpv4MappingPrefix(char *ipStr)
/* trim off the "::ffff:" ipv4-mapped prefix of the ipv6 address */
{
if (!ipStr)
    errAbort("unexpected NULL ipStr in trimIpv4-mappingPrefix");
if (startsWith(IPV4MAPPED_PREFIX, ipStr)) // strip off ipv6 ipv4-mapping
    {
    int size = strlen(ipStr);	    
    int prefixSize = strlen(IPV4MAPPED_PREFIX);
    memmove(ipStr, ipStr + prefixSize, size - prefixSize + 1); // dest and src can overlap.
    }
}

void getAddrAsString6n4(struct sockaddr_storage *sai, char *ipStr, int ipStrSize)
/* convert ip to string based on the socket type */
{
if (sai->ss_family == AF_INET6)   //ipv6
    {
    struct sockaddr_in6 *sai6 = (struct sockaddr_in6 *)sai;
    if (inet_ntop(AF_INET6, &sai6->sin6_addr, ipStr, ipStrSize) < 0)
        {
        errAbort("ntop failed on ip\n");
        }
    trimIpv4MappingPrefix(ipStr);
    }
else if (sai->ss_family == AF_INET)  // ipv4
    {
    struct sockaddr_in *sai4 = (struct sockaddr_in *)sai;
    if (inet_ntop(AF_INET, &sai4->sin_addr, ipStr, ipStrSize) < 0)
        {
        errAbort("ntop failed on ip\n");
        }
    }
else
   errAbort("unknown sai->sa_family=%d in getAddrAsString6n4", sai->ss_family);
}


void getAddrAndPortAsString6n4(struct sockaddr_storage *sai, char *ipStr, int ipStrSize, char *portStr, int portStrSize)
/* convert ip and port to strings based on the socket type */
{
if (sai->ss_family == AF_INET6)   //ipv6
    {
    struct sockaddr_in6 *sai6 = (struct sockaddr_in6 *)sai;
    int s = getnameinfo((struct sockaddr *) sai6, getSockSize6n4(sai),
                               ipStr, ipStrSize,
                               portStr, portStrSize, NI_NUMERICSERV | NI_NUMERICHOST);
    if (s != 0)
       errAbort("getnameinfo: %s\n", gai_strerror(s));
    trimIpv4MappingPrefix(ipStr);
    }
else if (sai->ss_family == AF_INET)  // ipv4
    {
    struct sockaddr_in *sai4 = (struct sockaddr_in *)sai;
    int s = getnameinfo((struct sockaddr *) sai4, getSockSize6n4(sai),
                               ipStr, ipStrSize,
                               portStr, portStrSize, NI_NUMERICSERV | NI_NUMERICHOST);
    if (s != 0)
       errAbort("getnameinfo: %s\n", gai_strerror(s));
    }
else
   errAbort("unknown sai->sa_family=%d in getAddrAndPortAsString6n4", sai->ss_family);
}


