/* Net.h some stuff to wrap around net communications. */

#ifndef NET_H
#define NET_H

#ifndef LINEFILE_H
#include "linefile.h"
#endif /* LINEFILE_H */

#ifndef DYSTRING_H
#include "dystring.h"
#endif /* DYSTRING_H */

int netSetupSocket(char *hostName, int port, struct sockaddr_in *sai);
/* Set up our socket. */

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

boolean netSendString(int sd, char *s);
/* Send a string down a socket - length byte first. */

boolean netSendLongString(int sd, char *s);
/* Send a string down a socket - up to 64k characters. */

char *netRecieveString(int sd, char buf[256]);
/* Read string into buf and return it.  If buf is NULL
 * an internal buffer will be used. Abort if any problem. */

char *netRecieveLongString(int sd);
/* Read string and return it.  freeMem
 * the result when done. Abort if any problem*/

char *netGetString(int sd, char buf[256]);
/* Read string into buf and return it.  If buf is NULL
 * an internal buffer will be used. Print warning message
 * and return NULL if any problem. */

char *netGetLongString(int sd);
/* Read string and return it.  freeMem
 * the result when done.  Print warning message and
 * return NULL if any problem. */

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

int netUrlOpen(char *url);
/* Return unix low-level file handle for url. 
 * Just close(result) when done. */

struct lineFile *netLineFileOpen(char *url);
/* Return a lineFile attatched to url.  This one
 * will skip any headers.   Free this with
 * lineFileClose(). */

struct dyString *netSlurpUrl(char *url);
/* Go grab all of URL and return it as dynamic string. */

#endif /* NET_H */

