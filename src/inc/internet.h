/* internet - some stuff for routines that use the internet
 * and aren't afraid to include some internet specific structures
 * and the like.   See also net for stuff that is higher level. */

#ifndef INTERNET_H
#define INTERNET_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define IPV4MAPPED_PREFIX "::ffff:"

struct cidr
/* Store CIDR address as IP and length */
    {
    struct cidr *next;   // can be a list
    struct in6_addr ipv6;  // 16 byte address in network (big-endian) byte order
    int subnetLength;		
    };

boolean isIpv4Address(char *ipStr);
/* Return TRUE if string is a properly formatted IPV4 string */

boolean isIpv6Address(char *ipStr);
/* Return TRUE if string is a properly formatted IPV6 string */

char *familyToString(sa_family_t family);
/* Convert family to pritable string */

boolean internetGetAddrInfo6n4(char *server, char *servPort, struct addrinfo **pAddress);
/* Fill in address. Return FALSE if can't. Free address with freeaddrinfo(). */

boolean internetFillInAddress6n4(char *hostStr, char *portStr, 
    sa_family_t family, int socktype, struct sockaddr_storage *sai,
    boolean ipOnly);
/* Fill in address. hostStr and portStr are strings that contain ipv4 or ipv6 addresses and port numbers. 
 * hostStr can also be a name, but it aborts if ipOnly specified. Returns TRUE if result found. */

boolean internetIpToDottedQuad(bits32 ip, char dottedQuad[17]);
/* Convert IP4 address in host byte order to dotted quad 
 * notation.  Warn and return FALSE if there's a 
 * problem. */

boolean internetDottedQuadToIp(char *dottedQuad, bits32 *retIp);
/* Convert dotted quad format address to IP4 address in
 * host byte order.  Warn and return FALSE if there's a 
 * problem. */

void internetParseDottedQuad(char *dottedQuad, unsigned char quad[4]);
/* Parse dotted quads into quad */

void internetUnpackIp(bits32 packed, unsigned char unpacked[4]);
/* Convert from 32 bit to 4-byte format with most significant
 * byte first. */

bits32 internetPackIp(unsigned char unpacked[4]);
/* Convert unsigned char network order to bits32 host order */

void internetCidrRange(struct cidr *cidr, struct in6_addr *pStartIp, struct in6_addr *pEndIp);
/* get range of CIDR formatted subnet as start and end IPs. Use network byte order. */

boolean internetIpInSubnetCidr(struct in6_addr *clientIp, struct cidr *cidrList);
/* Return true if clientIp address is in one of the subnets of cidrList. Use network byte order. */

struct cidr *internetParseSubnetCidr(char *cidr);
/* parse input CIDR format IP for range or subnet */

boolean internetIpStringToIp6(char *ipStr, struct in6_addr *retIp);
/* Convert IPv6 string to IPv6 binary address in network byte order.
 * Warn and return FALSE if there's a problem. */

void ip6AddrCopy(struct in6_addr *src, struct in6_addr *dst);
/* copy 16 byte ipv6 address from source to destination */

void ip6AddrShowAsHex(struct in6_addr *ip);
/* show 16 byte ipv6 address as hex */

void ip6AddrToHexStr(struct in6_addr *ip, char *s, int size);
/* show 16 byte ipv6 address as hex string. */

void ip6AddrFlipBits(struct in6_addr *ip);
/* flip bits by XOR . */

void ip6AddrAndBits(struct in6_addr *ip1, struct in6_addr *ip2, struct in6_addr *result);
/* result = ip1 AND ip2 bits together.  */

void ip6AddrOrBits(struct in6_addr *ip1, struct in6_addr *ip2, struct in6_addr *result);
/* result = ip1 OR ip2 bits together. */

int ip6AddrCmpBits(struct in6_addr *ip1, struct in6_addr *ip2);
/* Return signed value subracting ip2 bits from ip1 bits. 
 * Returns 0 if equal, positive if ip2 < ip1 and negative if ip1 < ip2. */

void ip6AddrMaskLeft(struct in6_addr *ip, int bits);
/* create mask of bits all 1 on left. */

void ip6AddrMaskRight(struct in6_addr *ip, int bits);
/* create mask of bits all 1 on right. */

socklen_t getSockSize6n4(struct sockaddr_storage *sai);
/* figure out the size of the structure from the socket type */

void getAddrAsString6n4(struct sockaddr_storage *sai, char *ipStr, int ipStrSize);
/* convert ip to string based on the socket type */

void getAddrAndPortAsString6n4(struct sockaddr_storage *sai, char *ipStr, int ipStrSize, char *portStr, int portStrSize);
/* convert ip and port to strings based on the socket type */

#endif /* INTERNET_H */
