/* Net.h some stuff to wrap around net communications. */

#ifndef NET_H
#define NET_H

#ifndef DYSTRING_H
#include "dystring.h"
#endif /* DYSTRING_H */

int netConnect(char *hostName, char *portName);
/* Start connection with server. */

int netMustConnect(char *hostName, char *portName);
/* Start connection with server or die. */

int netReadAll(int sd, void *vBuf, size_t size);
/* Read given number of bytes into buffer.
 * Don't give up on first read! */

int netMustReadAll(int sd, void *vBuf, size_t size);
/* Read given number of bytes into buffer or die.
 * Don't give up if first read is short! */

void netCatchPipes();
/* Set up to catch broken pipe signals. */

boolean netPipeIsBroken();
/* Return TRUE if pipe is broken */

void  netClearPipeFlag();
/* Clear broken pipe flag. */

struct netParsedUrl
/* A parsed URL. */
   {
   char protocol[16];	/* Protocol - http or ftp, etc. */
   char host[128];	/* Name of host computer - www.yahoo.com, etc. */
   char port[16];       /* Port, usually 80 or 8080. */
   char file[512];	/* Remote file name, starts with '/' */
   };

void netParseUrl(char *url, struct netParsedUrl *parsed);
/* Parse a URL into components.   A full URL is made up as so:
 *   http://hostName:port/file
 * This is set up so that the http:// and the port are optional. 
 */

struct dyString *netSlurpUrl(char *url);
/* Go grab all of URL and return it as dynamic string. */

#endif /* NET_H */

