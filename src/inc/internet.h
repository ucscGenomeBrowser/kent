/* internet - some stuff for routines that use the internet
 * and aren't afraid to include some internet specific structures
 * and the like.   See also net for stuff that is higher level. */

#ifndef INTERNET_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

bits32 internetHostIp(char *hostName);
/* Get IP v4 address (in host byte order) for hostName.
 * Warn and return 0 if there's a problem. */

boolean internetFillInAddress(char *hostName, int port, struct sockaddr_in *address);
/* Fill in address. Warn and return FALSE if can't.  */

#endif /* INTERNET_H */
