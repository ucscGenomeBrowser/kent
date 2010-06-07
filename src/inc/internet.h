/* internet - some stuff for routines that use the internet
 * and aren't afraid to include some internet specific structures
 * and the like.   See also net for stuff that is higher level. */

#ifndef INTERNET_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

bits32 internetHostIp(char *hostName);
/* Get IP v4 address (in host byte order) for hostName.
 * Warn and return 0 if there's a problem. */

boolean internetFillInAddress(char *hostName, int port, 
	struct sockaddr_in *address);
/* Fill in address. Warn and return FALSE if can't.  */

boolean internetIpToDottedQuad(bits32 ip, char dottedQuad[17]);
/* Convert IP4 address in host byte order to dotted quad 
 * notation.  Warn and return FALSE if there's a 
 * problem. */

boolean internetDottedQuadToIp(char *dottedQuad, bits32 *retIp);
/* Convert dotted quad format address to IP4 address in
 * host byte order.  Warn and return FALSE if there's a 
 * problem. */

boolean internetIsDottedQuad(char *s);
/* Returns TRUE if it looks like s is a dotted quad. */

void internetParseDottedQuad(char *dottedQuad, unsigned char quad[4]);
/* Parse dotted quads into quad */

void internetUnpackIp(bits32 packed, unsigned char unpacked[4]);
/* Convert from 32 bit to 4-byte format with most significant
 * byte first. */

boolean internetIpInSubnet(unsigned char unpackedIp[4], 
	unsigned char subnet[4]);
/* Return true if unpacked IP address is in subnet. */

#endif /* INTERNET_H */
