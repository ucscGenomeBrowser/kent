/* net.c some stuff to wrap around net communications. */

#include <netdb.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "common.h"
#include "errabort.h"
#include "net.h"
#include "linefile.h"


int netSetupSocket(char *hostName, int port, struct sockaddr_in *sai)
/* Set up a socket.  Warn and return error code if there's a problem. */
{
int sd;
struct hostent *hostent;

hostent = gethostbyname(hostName);
if (hostent == NULL)
    {
    warn("Couldn't find host %s. h_errno %d", hostName, h_errno);
    return -1;
    }
sai->sin_family = AF_INET;
sai->sin_port = htons(port);
memcpy(&sai->sin_addr.s_addr, hostent->h_addr_list[0], sizeof(sai->sin_addr.s_addr));
sd = socket(AF_INET, SOCK_STREAM, 0);
if (sd < 0)
    warn("Could't open INET socket code %d", sd);
return sd;
}

int netMustSetupSocket(char *hostName, int port, struct sockaddr_in *sai)
/* Set up our socket. Abort if a problem. */
{
int sd = netSetupSocket(hostName, port, sai);
if (sd < 0)
    noWarnAbort();
return sd;
}

int netConnPort(char *hostName, int port)
/* Start connection with a server. */
{
int sd, err;
struct sockaddr_in sai;		/* Some system socket info. */

/* Connect to server. */
ZeroVar(&sai);
sd = netSetupSocket(hostName, port, &sai);
if (sd < 0)
    {
    return sd;
    }
if ((err = connect(sd, (struct sockaddr*)&sai, sizeof(sai))) < 0)
   {
   warn("Couldn't connect to %s %d", hostName, port);
   return err;
   }
return sd;
}

int netConnect(char *hostName, char *portName)
/* Start connection with server. */
{
/* Convert port to number. */
if (!isdigit(portName[0]))
    errAbort("Expecting a port number got %s", portName);
return netConnPort(hostName, atoi(portName));
}

int netMustConnect(char *hostName, char *portName)
/* Start connection with server or die. */
{
int sd = netConnect(hostName, portName);
if (sd < 0)
   noWarnAbort();
return sd;
}

volatile boolean netPipeFlag = FALSE;	/* Flag broken pipes here. */
static boolean plumberInstalled = FALSE;	/* True if have installed pipe handler. */

static void netPipeHandler(int sigNum)
/* Set broken pipe flag. */
{
netPipeFlag = TRUE;
}

boolean netPipeIsBroken()
/* Return TRUE if pipe is broken */
{
return netPipeFlag;
}

void  netClearPipeFlag()
/* Clear broken pipe flag. */
{
netPipeFlag = FALSE;
}

void netCatchPipes()
/* Set up to catch broken pipe signals. */
{
if (!plumberInstalled)
    {
    signal(SIGPIPE, netPipeHandler);
    plumberInstalled = TRUE;
    }
}

int netReadAll(int sd, void *vBuf, size_t size)
/* Read given number of bytes into buffer.
 * Don't give up on first read! */
{
char *buf = vBuf;
size_t totalRead = 0;
int oneRead;

if (!plumberInstalled)
    netCatchPipes();
netPipeFlag = FALSE;
while (totalRead < size)
    {
    oneRead = read(sd, buf + totalRead, size - totalRead);
    if (oneRead < 0)
        {
	return oneRead;
	}
    totalRead += oneRead;
    }
return totalRead;
}

int netMustReadAll(int sd, void *vBuf, size_t size)
/* Read given number of bytes into buffer or die.
 * Don't give up if first read is short! */
{
int ret = netReadAll(sd, vBuf, size);
if (ret < 0)
    errnoAbort("Couldn't finish netReadAll");
return ret;
}

void netParseUrl(char *url, struct netParsedUrl *parsed)
/* Parse a URL into components.   A full URL is made up as so:
 *   http://hostName:port/file
 * This is set up so that the http:// and the port are optional. 
 */
{
char *s, *t, *u;
char buf[1024];

/* Make local copy of URL. */
if (strlen(url) >= sizeof(buf))
    errAbort("Url too long: '%s'", url);
strcpy(buf, url);
url = buf;

/* Find out protocol - default to http. */
s = trimSpaces(url);
s = stringIn("://", url);
if (s == NULL)
    {
    strcpy(parsed->protocol, "http");
    s = url;
    }
else
    {
    *s = 0;
    tolowers(url);
    strncpy(parsed->protocol, url, sizeof(parsed->protocol));
    s += 3;
    }

/* Split off file part. */
u = strchr(s, '/');
if (u == NULL)
   strcpy(parsed->file, "/");
else
   {
   strncpy(parsed->file, u, sizeof(parsed->file));
   *u = 0;
   }

/* Save port if it's there.  If not default to 80. */
t = strchr(s, ':');
if (t == NULL)
   {
   strcpy(parsed->port, "80");
   }
else
   {
   *t++ = 0;
   if (!isdigit(t[0]))
      errAbort("Non-numeric port name %s", t);
   strncpy(parsed->port, t, sizeof(parsed->port));
   }

/* What's left is the host. */
strncpy(parsed->host, s, sizeof(parsed->host));
}

static int netGetOpenHttp(char *url)
/* Return a file handle that will read the url.  This
 * skips past any header. */
{
struct netParsedUrl npu;
struct dyString *dy = newDyString(512);
int sd;

/* Parse the URL and connect. */
netParseUrl(url, &npu);
if (!sameString(npu.protocol, "http"))
    errAbort("Sorry, can only slurp http's currently");
sd = netMustConnect(npu.host, npu.port);

/* Ask remote server for a file. */
dyStringPrintf(dy, "GET %s HTTP/1.0\r\n\r\n", npu.file);
write(sd, dy->string, dy->stringSize);

/* Clean up and return handle. */
dyStringFree(&dy);
return sd;
}

int netUrlOpen(char *url)
/* Return unix low-level file handle for url. 
 * Just close(result) when done. */
{
return netGetOpenHttp(url);
}

struct dyString *netSlurpUrl(char *url)
/* Go grab all of URL and return it as dynamic string. */
{
char buf[4*1024];
int readSize;
struct dyString *dy = newDyString(4*1024);
int sd = netUrlOpen(url);

/* Slurp file into dy and return. */
while ((readSize = read(sd, buf, sizeof(buf))) > 0)
    dyStringAppendN(dy, buf, readSize);
close(sd);
return dy;
}

static void netSkipHttpHeaderLines(struct lineFile *lf)
/* Skip http header lines. */
{
char *line;
if (lineFileNext(lf, &line, NULL))
    {
    if (startsWith("HTTP/", line))
        {
	while (lineFileNext(lf, &line, NULL))
	    {
	    if ((line[0] == '\r' && line[1] == 0) || line[0] == 0)
	        break;
	    }
	}
    else
        lineFileReuse(lf);
    }
}

struct lineFile *netLineFileOpen(char *url)
/* Return a lineFile attatched to url.  This one
 * will skip any headers.   Free this with
 * lineFileClose(). */
{
int sd = netUrlOpen(url);
struct lineFile *lf = lineFileAttatch(url, TRUE, sd);
netSkipHttpHeaderLines(lf);
return lf;
}

boolean netSendString(int sd, char *s)
/* Send a string down a socket - length byte first. */
{
int length = strlen(s);
UBYTE len;

if (length > 255)
    errAbort("Trying to send a string longer than 255 bytes (%d bytes)", length);
len = length;
if (write(sd, &len, 1)<0)
    {
    warn("Couldn't send string to socket");
    return FALSE;
    }
if (write(sd, s, length)<0)
    {
    warn("Couldn't send string to socket");
    return FALSE;
    }
return TRUE;
}

boolean netSendLongString(int sd, char *s)
/* Send a long string down socket: two bytes for length. */
{
unsigned length = strlen(s);
UBYTE b[2];

if (length >= 64*1024)
    errAbort("Trying to send a string longer than 64k bytes (%d bytes)", length);
b[0] = (length>>8);
b[1] = (length&0xff);
if (write(sd, b, 2) < 0)
    {
    warn("Couldn't send long string to socket");
    return FALSE;
    }
if (write(sd, s, length)<0)
    {
    warn("Couldn't send long string to socket");
    return FALSE;
    }
return TRUE;
}

char *netGetString(int sd, char buf[256])
/* Read string into buf and return it.  If buf is NULL
 * an internal buffer will be used. Print warning message
 * and return NULL if any problem. */
{
static char sbuf[256];
UBYTE len;
int length;
if (buf == NULL) buf = sbuf;
if (read(sd, &len, 1)<0)
    {
    warn("Couldn't read string length");
    return NULL;
    }
length = len;
if (length > 0)
    if (netReadAll(sd, buf, length) < 0)
	{
	warn("Couldn't read string body");
	return NULL;
	}
buf[length] = 0;
return buf;
}

char *netGetLongString(int sd)
/* Read string and return it.  freeMem
 * the result when done. */
{
UBYTE b[2];
char *s = NULL;
int length = 0;
if (netReadAll(sd, b, 2)<0)
    {
    warn("Couldn't read long string length");
    return NULL;
    }
length = (b[0]<<8) + b[1];
s = needMem(length+1);
if (length > 0)
    if (netReadAll(sd, s, length) < 0)
	{
	warn("Couldn't read long string body");
	return NULL;
	}
s[length] = 0;
return s;
}


char *netRecieveString(int sd, char buf[256])
/* Read string into buf and return it.  If buf is NULL
 * an internal buffer will be used. Abort if any problem. */
{
char *s = netGetString(sd, buf);
if (s == NULL)
     noWarnAbort();   
return s;
}

char *netRecieveLongString(int sd)
/* Read string and return it.  freeMem
 * the result when done. Abort if any problem*/
{
char *s = netGetLongString(sd);
if (s == NULL)
     noWarnAbort();   
return s;
}

