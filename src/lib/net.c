/* net.c some stuff to wrap around net communications. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <utime.h>
#include <pthread.h>
#include "internet.h"
#include "errabort.h"
#include "hash.h"
#include "net.h"
#include "linefile.h"
#include "base64.h"
#include "cheapcgi.h"
#include "https.h"
#include "sqlNum.h"
#include "obscure.h"

static char const rcsid[] = "$Id: net.c,v 1.80 2010/04/14 07:42:06 galt Exp $";

/* Brought errno in to get more useful error messages */

extern int errno;

static int netStreamSocket()
/* Create a TCP/IP streaming socket.  Complain and return something
 * negative if can't */
{
int sd = socket(AF_INET, SOCK_STREAM, 0);
if (sd < 0)
    warn("Couldn't make AF_INET socket.");
return sd;
}

static int netConnectWithTimeout(char *hostName, int port, long msTimeout)
/* In order to avoid a very long default timeout (several minutes) for hosts that will
 * not answer the port, we are forced to connect non-blocking.
 * After the connection has been established, we return to blocking mode. */
{
int sd;
struct sockaddr_in sai;		/* Some system socket info. */
int res;
fd_set mySet;
struct timeval lTime;
long fcntlFlags;

if (hostName == NULL)
    {
    warn("NULL hostName in netConnect");
    return -1;
    }
if (!internetFillInAddress(hostName, port, &sai))
    return -1;
if ((sd = netStreamSocket()) < 0)
    return sd;

// Set non-blocking
if ((fcntlFlags = fcntl(sd, F_GETFL, NULL)) < 0) 
    {
    warn("Error fcntl(..., F_GETFL) (%s)", strerror(errno));
    close(sd);
    return -1;
    }
fcntlFlags |= O_NONBLOCK;
if (fcntl(sd, F_SETFL, fcntlFlags) < 0) 
    {
    warn("Error fcntl(..., F_SETFL) (%s)", strerror(errno));
    close(sd);
    return -1;
    }

// Trying to connect with timeout
res = connect(sd, (struct sockaddr*) &sai, sizeof(sai));
if (res < 0)
    {
    if (errno == EINPROGRESS)
	{
	while (1) 
	    {
	    lTime.tv_sec = (long) (msTimeout/1000);
	    lTime.tv_usec = (long) (((msTimeout/1000)-lTime.tv_sec)*1000000);
	    FD_ZERO(&mySet);
	    FD_SET(sd, &mySet);
	    res = select(sd+1, NULL, &mySet, &mySet, &lTime);
	    if (res < 0) 
		{
		if (errno != EINTR) 
		    {
		    warn("Error in select() during TCP non-blocking connect %d - %s", errno, strerror(errno));
		    close(sd);
		    return -1;
		    }
		}
	    else if (res > 0)
		{
		// Socket selected for write when it is ready
		int valOpt;
		socklen_t lon;
                // But check the socket for any errors
                lon = sizeof(valOpt);
                if (getsockopt(sd, SOL_SOCKET, SO_ERROR, (void*) (&valOpt), &lon) < 0)
                    {
                    warn("Error in getsockopt() %d - %s", errno, strerror(errno));
                    close(sd);
                    return -1;
                    }
                // Check the value returned...
                if (valOpt)
                    {
                    warn("Error in TCP non-blocking connect() %d - %s", valOpt, strerror(valOpt));
                    close(sd);
                    return -1;
                    }
		break;
		}
	    else
		{
		warn("TCP non-blocking connect() to %s timed-out in select() after %ld milliseconds - Cancelling!", hostName, msTimeout);
		close(sd);
		return -1;
		}
	    }
	}
    else
	{
	warn("TCP non-blocking connect() error %d - %s", errno, strerror(errno));
	close(sd);
	return -1;
	}
    }

// Set to blocking mode again
if ((fcntlFlags = fcntl(sd, F_GETFL, NULL)) < 0)
    {
    warn("Error fcntl(..., F_GETFL) (%s)", strerror(errno));
    close(sd);
    return -1;
    }
fcntlFlags &= (~O_NONBLOCK);
if (fcntl(sd, F_SETFL, fcntlFlags) < 0)
    {
    warn("Error fcntl(..., F_SETFL) (%s)", strerror(errno));
    close(sd);
    return -1;
    }

return sd;

}


int netConnect(char *hostName, int port)
/* Start connection with a server. */
{
return netConnectWithTimeout(hostName, port, 10000); // 10 seconds connect timeout
}

int netMustConnect(char *hostName, int port)
/* Start connection with server or die. */
{
int sd = netConnect(hostName, port);
if (sd < 0)
   noWarnAbort();
return sd;
}

int netMustConnectTo(char *hostName, char *portName)
/* Start connection with a server and a port that needs to be converted to integer */
{
if (!isdigit(portName[0]))
    errAbort("netConnectTo: ports must be numerical, not %s", portName);
return netMustConnect(hostName, atoi(portName));
}


int netAcceptingSocketFrom(int port, int queueSize, char *host)
/* Create a socket that can accept connections from a 
 * IP address on the current machine if the current machine
 * has multiple IP addresses. */
{
struct sockaddr_in sai;
int sd;
int flag = 1;

netBlockBrokenPipes();
if ((sd = netStreamSocket()) < 0)
    return sd;
if (!internetFillInAddress(host, port, &sai))
    return -1;
if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)))
    return -1;
if (bind(sd, (struct sockaddr*)&sai, sizeof(sai)) == -1)
    {
    warn("Couldn't bind socket to %d: %s", port, strerror(errno));
    close(sd);
    return -1;
    }
listen(sd, queueSize);
return sd;
}

int netAcceptingSocket(int port, int queueSize)
/* Create a socket that can accept connections from
 * anywhere. */
{
return netAcceptingSocketFrom(port, queueSize, NULL);
}

int netAccept(int sd)
/* Accept incoming connection from socket descriptor. */
{
socklen_t fromLen;
return accept(sd, NULL, &fromLen);
}

int netAcceptFrom(int acceptor, unsigned char subnet[4])
/* Wait for incoming connection from socket descriptor
 * from IP address in subnet.  Subnet is something
 * returned from netParseSubnet or internetParseDottedQuad. 
 * Subnet may be NULL. */
{
struct sockaddr_in sai;		/* Some system socket info. */
ZeroVar(&sai);
sai.sin_family = AF_INET;
for (;;)
    {
    socklen_t addrSize = sizeof(sai);
    int sd = accept(acceptor, (struct sockaddr *)&sai, &addrSize);
    if (sd >= 0)
	{
	if (subnet == NULL)
	    return sd;
	else
	    {
	    unsigned char unpacked[4]; 
	    internetUnpackIp(ntohl(sai.sin_addr.s_addr), unpacked);
	    if (internetIpInSubnet(unpacked, subnet))
		{
		return sd;
		}
	    else
		{
		close(sd);
		}
	    }
	}
    }
}

FILE *netFileFromSocket(int socket)
/* Wrap a FILE around socket.  This should be fclose'd
 * and separately the socket close'd. */
{
FILE *f;
if ((socket = dup(socket)) < 0)
   errnoAbort("Couldn't dupe socket in netFileFromSocket");
f = fdopen(socket, "r+");
if (f == NULL)
   errnoAbort("Couldn't fdopen socket in netFileFromSocket");
return f;
}

static boolean plumberInstalled = FALSE;

void netBlockBrokenPipes()
/* Make it so a broken pipe doesn't kill us. */
{
if (!plumberInstalled)
    {
    signal(SIGPIPE, SIG_IGN);       /* Block broken pipe signals. */
    plumberInstalled = TRUE;
    }
}

size_t netReadAll(int sd, void *vBuf, size_t size)
/* Read given number of bytes into buffer.
 * Don't give up on first read! */
{
char *buf = vBuf;
size_t totalRead = 0;
int oneRead;

if (!plumberInstalled)
    netBlockBrokenPipes();
while (totalRead < size)
    {
    oneRead = read(sd, buf + totalRead, size - totalRead);
    if (oneRead < 0)
	return oneRead;
    if (oneRead == 0)
        break;
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

static void notGoodSubnet(char *sns)
/* Complain about subnet format. */
{
errAbort("'%s' is not a properly formatted subnet.  Subnets must consist of\n"
         "one to three dot-separated numbers between 0 and 255", sns);
}

void netParseSubnet(char *in, unsigned char out[4])
/* Parse subnet, which is a prefix of a normal dotted quad form.
 * Out will contain 255's for the don't care bits. */
{
out[0] = out[1] = out[2] = out[3] = 255;
if (in != NULL)
    {
    char *snsCopy = strdup(in);
    char *words[5];
    int wordCount, i;
    wordCount = chopString(snsCopy, ".", words, ArraySize(words));
    if (wordCount > 3 || wordCount < 1)
        notGoodSubnet(in);
    for (i=0; i<wordCount; ++i)
	{
	char *s = words[i];
	int x;
	if (!isdigit(s[0]))
	    notGoodSubnet(in);
	x = atoi(s);
	if (x > 255)
	    notGoodSubnet(in);
	out[i] = x;
	}
    freez(&snsCopy);
    }
}

static void parseByteRange(char *url, ssize_t *rangeStart, ssize_t *rangeEnd, boolean terminateAtByteRange)
/* parse the byte range information from url */
{
char *x;
/* default to no byte range specified */
*rangeStart = -1;
*rangeEnd = -1;
x = strrchr(url, ';');
if (x)
    {
    if (startsWith(";byterange=", x))
	{
	char *y=strchr(x, '=');
	++y;
	char *z=strchr(y, '-');
	if (z)
	    {
	    ++z;
	    if (terminateAtByteRange)
		*x = 0;
	    // TODO: use something better than atoll() ?
	    *rangeStart = atoll(y); 
	    if (z[0] != '\0')
		*rangeEnd = atoll(z);
	    }    
	}
    }

}

void netParseUrl(char *url, struct netParsedUrl *parsed)
/* Parse a URL into components.   A full URL is made up as so:
 *   http://user:password@hostName:port/file;byterange=0-499
 * User and password may be cgi-encoded.
 * This is set up so that the http:// and the port are optional. 
 */
{
char *s, *t, *u, *v, *w;
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
    safecpy(parsed->protocol, sizeof(parsed->protocol), url);
    s += 3;
    }

/* Split off file part. */
parsed->byteRangeStart = -1;  /* default to no byte range specified */
parsed->byteRangeEnd = -1;
u = strchr(s, '/');
if (u == NULL)
    strcpy(parsed->file, "/");
else
    {
    parseByteRange(u, &parsed->byteRangeStart, &parsed->byteRangeEnd, TRUE);

    /* need to encode spaces, but not ! other characters */
    char *t=replaceChars(u," ","%20");
    safecpy(parsed->file, sizeof(parsed->file), t);
    freeMem(t);
    *u = 0;
    }

/* Split off user part */
v = strchr(s, '@');
if (v == NULL)
    {
    if (sameWord(parsed->protocol,"http") ||
        sameWord(parsed->protocol,"https"))
	{
	strcpy(parsed->user, "");
	strcpy(parsed->password, "");
	}
    if (sameWord(parsed->protocol,"ftp"))
	{
	strcpy(parsed->user, "anonymous");
	strcpy(parsed->password, "x@genome.ucsc.edu");
	}
    }
else
    {
    *v = 0;
    /* split off password part */
    w = strchr(s, ':');
    if (w == NULL)
	{
	safecpy(parsed->user, sizeof(parsed->user), s);
	strcpy(parsed->password, "");
	}
    else
	{
	*w = 0;
	safecpy(parsed->user, sizeof(parsed->user), s);
	safecpy(parsed->password, sizeof(parsed->password), w+1);
	}
    
    cgiDecode(parsed->user,parsed->user,strlen(parsed->user));
    cgiDecode(parsed->password,parsed->password,strlen(parsed->password));
    s = v+1;
    }


/* Save port if it's there.  If not default to 80. */
t = strchr(s, ':');
if (t == NULL)
    {
    if (sameWord(parsed->protocol,"http"))
	strcpy(parsed->port, "80");
    if (sameWord(parsed->protocol,"https"))
	strcpy(parsed->port, "443");
    if (sameWord(parsed->protocol,"ftp"))
	strcpy(parsed->port, "21");
    }
else
    {
    *t++ = 0;
    if (!isdigit(t[0]))
	errAbort("Non-numeric port name %s", t);
    safecpy(parsed->port, sizeof(parsed->port), t);
    }

/* What's left is the host. */
safecpy(parsed->host, sizeof(parsed->host), s);
}

char *urlFromNetParsedUrl(struct netParsedUrl *npu)
/* Build URL from netParsedUrl structure */
{
struct dyString *dy = newDyString(512);

dyStringAppend(dy, npu->protocol);
dyStringAppend(dy, "://");
if (npu->user[0] != 0)
    {
    char *encUser = cgiEncode(npu->user);
    dyStringAppend(dy, encUser);
    freeMem(encUser);
    if (npu->password[0] != 0)
	{
	dyStringAppend(dy, ":");
	char *encPassword = cgiEncode(npu->password);
	dyStringAppend(dy, encPassword);
	freeMem(encPassword);
	}
    dyStringAppend(dy, "@");
    }
dyStringAppend(dy, npu->host);
/* do not include port if it is the default */
if (!(
 (sameString(npu->protocol, "ftp"  ) && sameString("21", npu->port)) ||
 (sameString(npu->protocol, "http" ) && sameString("80", npu->port)) ||
 (sameString(npu->protocol, "https") && sameString("443",npu->port))
    ))
    {
    dyStringAppend(dy, ":");
    dyStringAppend(dy, npu->port);
    }
dyStringAppend(dy, npu->file);
if (npu->byteRangeStart != -1)
    {
    dyStringPrintf(dy, ";byterange=%lld-", (long long)npu->byteRangeStart);
    if (npu->byteRangeEnd != -1)
	dyStringPrintf(dy, "%lld", (long long)npu->byteRangeEnd);
    }

/* Clean up and return handle. */
return dyStringCannibalize(&dy);
}

/* this was cloned from rudp.c - move it later for sharing */
static boolean readReadyWait(int sd, int microseconds)
/* Wait for descriptor to have some data to read, up to
 * given number of microseconds. */
{
struct timeval tv;
fd_set set;
int readyCount;

for (;;)
    {
    if (microseconds >= 1000000)
	{
	tv.tv_sec = microseconds/1000000;
	tv.tv_usec = microseconds%1000000;
	}
    else
	{
	tv.tv_sec = 0;
	tv.tv_usec = microseconds;
	}
    FD_ZERO(&set);
    FD_SET(sd, &set);
    readyCount = select(sd+1, &set, NULL, NULL, &tv);
    if (readyCount < 0) 
	{
	if (errno == EINTR)	/* Select interrupted, not timed out. */
	    continue;
    	else 
    	    warn("select failure in rudp: %s", strerror(errno));
    	}
    else
	{
    	return readyCount > 0;	/* Zero readyCount indicates time out */
	}
    }
}

static void sendFtpCommandOnly(int sd, char *cmd)
/* send command to ftp server */
{   
mustWriteFd(sd, cmd, strlen(cmd));
}

#define NET_FTP_TIMEOUT 1000000

static boolean receiveFtpReply(int sd, char *cmd, struct dyString **retReply, int *retCode)
/* send command to ftp server and check resulting reply code, 
 * warn and return FALSE if not desired reply.  If retReply is non-NULL, store reply text there. */
{
char *startLastLine = NULL;
struct dyString *rs = newDyString(4*1024);
while (1)
    {
    int readSize = 0;
    while (1)
	{
	char buf[4*1024];
	if (!readReadyWait(sd, NET_FTP_TIMEOUT))
	    {
	    warn("ftp server response timed out > %d microsec", NET_FTP_TIMEOUT);
	    return FALSE;
	    }
	if ((readSize = read(sd, buf, sizeof(buf))) == 0)
	    break;

	dyStringAppendN(rs, buf, readSize);
	if (endsWith(rs->string,"\n"))
	    break;
	}
	
    /* find the start of the last line in the buffer */
    startLastLine = rs->string+strlen(rs->string)-1;
    if (startLastLine >= rs->string)
	if (*startLastLine == '\n') 
	    --startLastLine;
    while ((startLastLine >= rs->string) && (*startLastLine != '\n'))
	--startLastLine;
    ++startLastLine;
	
    if (strlen(startLastLine)>4)
      if (
	isdigit(startLastLine[0]) &&
	isdigit(startLastLine[1]) &&
	isdigit(startLastLine[2]) &&
	startLastLine[3]==' ')
	break;
	
    if (readSize == 0)
	break;  // EOF
    /* must be some text info we can't use, ignore it till we get status code */
    }

int reply = atoi(startLastLine);
if ((reply < 200) || (reply > 399))
    {
    warn("ftp server error on cmd=[%s] response=[%s]",cmd,rs->string);
    return FALSE;
    }
    
if (retReply)
    *retReply = rs;
else
    dyStringFree(&rs);
if (retCode)
    *retCode = reply;
return TRUE;
}

static boolean sendFtpCommand(int sd, char *cmd, struct dyString **retReply, int *retCode)
/* send command to ftp server and check resulting reply code, 
 * warn and return FALSE if not desired reply.  If retReply is non-NULL, store reply text there. */
{   
sendFtpCommandOnly(sd, cmd);
return receiveFtpReply(sd, cmd, retReply, retCode);
}

static int parsePasvPort(char *rs)
/* parse PASV reply to get the port and return it */
{
char *words[7];
int wordCount;
char *rsStart = strchr(rs,'(');
char *rsEnd = strchr(rs,')');
int result = 0;
rsStart++;
*rsEnd=0;
wordCount = chopString(rsStart, ",", words, ArraySize(words));
if (wordCount != 6)
    errAbort("PASV reply does not parse correctly");
result = atoi(words[4])*256+atoi(words[5]);    
return result;
}    


static long long parseFtpSIZE(char *rs)
/* parse reply to SIZE and return it */
{
char *words[3];
int wordCount;
char *rsStart = rs;
long long result = 0;
wordCount = chopString(rsStart, " ", words, ArraySize(words));
if (wordCount != 2)
    errAbort("SIZE reply does not parse correctly");
result = atoll(words[1]);    
return result;
}    


static time_t parseFtpMDTM(char *rs)
/* parse reply to MDTM and return it
 * 200 YYYYMMDDhhmmss */
{
char *words[3];
int wordCount = chopString(rs, " ", words, ArraySize(words));
if (wordCount != 2)
    errAbort("MDTM reply should have 2 words but has %d", wordCount);
#define EXPECTED_MDTM_FORMAT "YYYYmmddHHMMSS"
if (strlen(words[1]) < strlen(EXPECTED_MDTM_FORMAT))
    errAbort("MDTM reply word [%s] shorter than " EXPECTED_MDTM_FORMAT, words[1]);
// man strptime ->
// "There should be whitespace or other alphanumeric characters between any two field descriptors."
// There are no separators in the MDTM timestamp, so make a spread-out version for strptime:
char spread[] = "YYYY mm dd HH MM SS";
char *from = words[1];
char *to = spread;
*to++ = *from++;
*to++ = *from++;
*to++ = *from++;
*to++ = *from++;
*to++ = '-';
*to++ = *from++;
*to++ = *from++;
*to++ = '-';
*to++ = *from++;
*to++ = *from++;
*to++ = ' ';
*to++ = *from++;
*to++ = *from++;
*to++ = ':';
*to++ = *from++;
*to++ = *from++;
*to++ = ':';
*to++ = *from++;
*to++ = *from++;
*to++ = 0;
struct tm tm;
if (strptime(spread, "%Y-%m-%d %H:%M:%S", &tm) == NULL)
    errAbort("unable to parse MDTM string [%s]", spread);
// Not set by strptime(); tells mktime() to determine whether daylight saving time is in effect:
tm.tm_isdst = -1;
time_t t = mktime(&tm);
if (t == -1)
    errAbort("mktime failed while parsing strptime'd MDTM string [%s]", words[1]);
return t;
}    


static int openFtpControlSocket(char *host, int port, char *user, char *password)
/* Open a socket to host,port; authenticate anonymous ftp; set type to I; 
 * return socket desc or -1 if there was an error. */
{
int sd = netConnect(host, port);
if (sd < 0)
    return -1;

/* First read the welcome msg */
if (readReadyWait(sd, NET_FTP_TIMEOUT))
    sendFtpCommand(sd, "", NULL, NULL);

char cmd[256];
int retCode = 0;
safef(cmd,sizeof(cmd),"USER %s\r\n", user);
if (!sendFtpCommand(sd, cmd, NULL, &retCode))
    {
    close(sd);
    return -1;
    }

if (retCode == 331)
    {
    safef(cmd,sizeof(cmd),"PASS %s\r\n", password);
    if (!sendFtpCommand(sd, cmd, NULL, NULL))
	{
	close(sd);
	return -1;
	}
    }

if (!sendFtpCommand(sd, "TYPE I\r\n", NULL, NULL))
    {
    close(sd);
    return -1;
    }
/* 200 Type set to I */
/* (send the data as binary, so can support compressed files) */
return sd;
}

boolean netGetFtpInfo(char *url, long long *retSize, time_t *retTime)
/* Return date in UTC and size of ftp url file */
{
/* Parse the URL and connect. */
struct netParsedUrl npu;
netParseUrl(url, &npu);
if (!sameString(npu.protocol, "ftp"))
    errAbort("netGetFtpInfo: url (%s) is not for ftp.", url);

// TODO maybe remove this workaround where udc cache wants info on URL "/" ?
if (sameString(npu.file,"/"))
    {
    *retSize = 0;
    *retTime = time(NULL);
    return TRUE;
    }

int sd = openFtpControlSocket(npu.host, atoi(npu.port), npu.user, npu.password);
if (sd < 0)
    return FALSE;
char cmd[256];
safef(cmd,sizeof(cmd),"SIZE %s\r\n", npu.file);
struct dyString *rs = NULL;
if (!sendFtpCommand(sd, cmd, &rs, NULL))
    {
    close(sd);
    return FALSE;
    }
*retSize = parseFtpSIZE(rs->string);
/* 200 12345 */
dyStringFree(&rs);

safef(cmd,sizeof(cmd),"MDTM %s\r\n", npu.file);
if (!sendFtpCommand(sd, cmd, &rs, NULL))
    {
    close(sd);
    return FALSE;
    }
*retTime = parseFtpMDTM(rs->string);
/* 200 YYYYMMDDhhmmss */
dyStringFree(&rs);
close(sd);   
return TRUE;
}

struct netConnectFtpParams
/* params to pass to thread */
{
pthread_t thread;
int pipefd[2];
int sd;
int sdata; 
struct netParsedUrl npu;
};

static void *sendFtpDataToPipeThread(void *threadParams)
/* This is to be executed by the child process after the fork in netGetOpenFtpSockets.
 * It keeps the ftp control socket alive while reading from the ftp data socket
 * and writing to the pipe to the parent process, which closes the ftp sockets
 * and reads from the pipe. */
{

struct netConnectFtpParams *params = threadParams;

pthread_detach(params->thread);  // this thread will never join back with it's progenitor

char buf[32768];
int rd = 0;
long long dataPos = 0; 
if (params->npu.byteRangeStart != -1)
    dataPos = params->npu.byteRangeStart;
while((rd = read(params->sdata, buf, 32768)) > 0) 
    {
    if (params->npu.byteRangeEnd != -1 && (dataPos + rd) > params->npu.byteRangeEnd)
	rd = params->npu.byteRangeEnd - dataPos + 1;
    int wt = write(params->pipefd[1], buf, rd);
    if (wt == -1 && params->npu.byteRangeEnd != -1)
	{
	// errAbort in child process is messy; let reader complain if
	// trouble.  If byterange was open-ended, we will hit this point
	// when the parent stops reading and closes the pipe.
	errnoWarn("error writing ftp data to pipe");
	break;
	}
    dataPos += rd;
    if (params->npu.byteRangeEnd != -1 && dataPos >= params->npu.byteRangeEnd)
	break;	    
    }
if (rd == -1)
    // Again, avoid abort in child process.
    errnoWarn("error reading ftp socket");
close(params->pipefd[1]);  /* we are done with it */
close(params->sd);
close(params->sdata);
return NULL;
}

static int netGetOpenFtpSockets(char *url, int *retCtrlSd)
/* Return a socket descriptor for url data (url can end in ";byterange:start-end",
 * or -1 if error.
 * If retCtrlSd is non-null, keep the control socket alive and set *retCtrlSd.
 * Otherwise, create a pipe and fork to keep control socket alive in the child 
 * process until we are done fetching data. */
{
char cmd[256];

/* Parse the URL and connect. */
struct netParsedUrl npu;
netParseUrl(url, &npu);
if (!sameString(npu.protocol, "ftp"))
    errAbort("netGetOpenFtpSockets: url (%s) is not for ftp.", url);
int sd = openFtpControlSocket(npu.host, atoi(npu.port), npu.user, npu.password);
if (sd == -1)
    return -1;

struct dyString *rs = NULL;
if (!sendFtpCommand(sd, "PASV\r\n", &rs, NULL))
    {
    close(sd);
    return -1;
    }
/* 227 Entering Passive Mode (128,231,210,81,222,250) */

if (npu.byteRangeStart != -1)
    {
    safef(cmd,sizeof(cmd),"REST %lld\r\n", (long long) npu.byteRangeStart);
    if (!sendFtpCommand(sd, cmd, NULL, NULL))
	{
	close(sd);
	return -1;
	}
    }

/* RETR for files, LIST for directories ending in / */
safef(cmd,sizeof(cmd),"%s %s\r\n",((npu.file[strlen(npu.file)-1]) == '/') ? "LIST" : "RETR", npu.file);
sendFtpCommandOnly(sd, cmd);

int sdata = netConnect(npu.host, parsePasvPort(rs->string));
dyStringFree(&rs);
if (sdata < 0)
    {
    close(sd);
    return -1;
    }

int secondsWaited = 0;
while (TRUE)
    {
    if (secondsWaited >= 10)
	{
	warn("ftp server error on cmd=[%s] timed-out waiting for data or error", cmd);
	close(sd);
	close(sdata);
	return -1;
	}
    if (readReadyWait(sdata, NET_FTP_TIMEOUT))
	break;   // we have some data
    if (readReadyWait(sd, 0)) /* wait in microsec */
	{
	// this can see an error like bad filename
	if (!receiveFtpReply(sd, cmd, NULL, NULL))
	    {
	    close(sd);
	    close(sdata);
	    return -1;
	    }
	}
    ++secondsWaited;
    }

if (retCtrlSd != NULL)
    {
    *retCtrlSd = sd;
    return sdata;
    }
else
    {
    /* Because some FTP servers will kill the data connection
     * as soon as the control connection closes,
     * we have to develop a workaround using a partner process. */
    fflush(stdin);
    fflush(stdout);
    fflush(stderr);

    struct netConnectFtpParams *params;
    AllocVar(params);
    params->sd = sd;
    params->sdata = sdata;
    params->npu = npu;
    /* make a pipe (fds go in pipefd[0] and pipefd[1])  */
    if (pipe(params->pipefd) != 0)
	errAbort("netGetOpenFtpSockets: failed to create pipe: %s", strerror(errno));
    int rc;
    rc = pthread_create(&params->thread, NULL, sendFtpDataToPipeThread, (void *)params);
    if (rc)
	{
	errAbort("Unexpected error %d from pthread_create(): %s",rc,strerror(rc));
	}

    return params->pipefd[0];
    }
}


int connectNpu(struct netParsedUrl npu, char *url)
/* Connect using NetParsedUrl. */
{
int sd = -1;
if (sameString(npu.protocol, "http"))
    {
    sd = netConnect(npu.host, atoi(npu.port));
    }
else if (sameString(npu.protocol, "https"))
    {
    sd = netConnectHttps(npu.host, atoi(npu.port));
    }
else
    {
    errAbort("netHttpConnect: url (%s) is not for http.", url);
    return -1;  /* never gets here, fixes compiler complaint */
    }
return sd;
}

void setAuthorization(struct netParsedUrl npu, char *authHeader, struct dyString *dy)
/* Set the specified authorization header with BASIC auth base64-encoded user and password */
{
if (!sameString(npu.user,""))
    {
    char up[256];
    char *b64up = NULL;
    safef(up, sizeof(up), "%s:%s", npu.user, npu.password);
    b64up = base64Encode(up, strlen(up));
    dyStringPrintf(dy, "%s: Basic %s\r\n", authHeader, b64up);
    freez(&b64up);
    }
}

int netHttpConnect(char *url, char *method, char *protocol, char *agent, char *optionalHeader)
/* Parse URL, connect to associated server on port, and send most of
 * the request to the server.  If specified in the url send user name
 * and password too.  Typically the "method" will be "GET" or "POST"
 * and the agent will be the name of your program or
 * library. optionalHeader may be NULL or contain additional header
 * lines such as cookie info. 
 * Proxy support via hg.conf httpProxy or env var http_proxy
 * Return data socket, or -1 if error.*/
{
struct netParsedUrl npu;
struct netParsedUrl pxy;
struct dyString *dy = newDyString(512);
int sd = -1;
/* Parse the URL and connect. */
netParseUrl(url, &npu);

char *proxyUrl = getenv("http_proxy");

if (proxyUrl)
    {
    netParseUrl(proxyUrl, &pxy);
    sd = connectNpu(pxy, url);
    }
else
    {
    sd = connectNpu(npu, url);
    }
if (sd < 0)
    return -1;

/* Ask remote server for a file. */
char *urlForProxy = NULL;
if (proxyUrl)
    {
    /* trim off the byterange part at the end of url because proxy does not understand it. */
    urlForProxy = cloneString(url);
    char *x = strrchr(urlForProxy, ';');
    if (x && startsWith(";byterange=", x))
	*x = 0;
    }
dyStringPrintf(dy, "%s %s %s\r\n", method, proxyUrl ? urlForProxy : npu.file, protocol);
freeMem(urlForProxy);
dyStringPrintf(dy, "User-Agent: %s\r\n", agent);
/* do not need the 80 since it is the default */
if ((sameString(npu.protocol, "http" ) && sameString("80", npu.port)) ||
    (sameString(npu.protocol, "https") && sameString("443",npu.port)))
    dyStringPrintf(dy, "Host: %s\r\n", npu.host);
else
    dyStringPrintf(dy, "Host: %s:%s\r\n", npu.host, npu.port);
setAuthorization(npu, "Authorization", dy);
if (proxyUrl)
    setAuthorization(pxy, "Proxy-Authorization", dy);
dyStringAppend(dy, "Accept: */*\r\n");
if (npu.byteRangeStart != -1)
    {
    if (npu.byteRangeEnd != -1)
	dyStringPrintf(dy, "Range: bytes=%lld-%lld\r\n"
		       , (long long)npu.byteRangeStart
		       , (long long)npu.byteRangeEnd);
    else
	dyStringPrintf(dy, "Range: bytes=%lld-\r\n"
		       , (long long)npu.byteRangeStart);
    }

if (optionalHeader)
    dyStringAppend(dy, optionalHeader);

/* finish off the header with final blank line */
dyStringAppend(dy, "\r\n");

mustWriteFd(sd, dy->string, dy->stringSize);

/* Clean up and return handle. */
dyStringFree(&dy);
return sd;
}


int netOpenHttpExt(char *url, char *method, char *optionalHeader)
/* Return a file handle that will read the url.  optionalHeader
 * may by NULL or may contain cookies and other info.  */
{
return netHttpConnect(url, method, "HTTP/1.0", "genome.ucsc.edu/net.c", optionalHeader);
}

static int netGetOpenHttp(char *url)
/* Return a file handle that will read the url.  */
{
return netOpenHttpExt(url, "GET", NULL);
}

int netUrlHeadExt(char *url, char *method, struct hash *hash)
/* Go get head and return status.  Return negative number if
 * can't get head. If hash is non-null, fill it with header
 * lines with upper cased keywords for case-insensitive lookup, 
 * including hopefully CONTENT-TYPE: . */
{
int sd = netOpenHttpExt(url, method, NULL);
int status = EIO;
if (sd >= 0)
    {
    char *line, *word;
    struct lineFile *lf = lineFileAttach(url, TRUE, sd);

    if (lineFileNext(lf, &line, NULL))
	{
	if (startsWith("HTTP/", line))
	    {
	    word = nextWord(&line);
	    word = nextWord(&line);
	    if (word != NULL && isdigit(word[0]))
	        {
		status = atoi(word);
		if (hash != NULL)
		    {
		    while (lineFileNext(lf, &line, NULL))
		        {
			word = nextWord(&line);
			if (word == NULL)
			    break;
			hashAdd(hash, strUpper(word), cloneString(skipLeadingSpaces(line)));
			}
		    }
		}
	    }
	}
    lineFileClose(&lf);
    }
else
    status = errno;
return status;
}


int netUrlHead(char *url, struct hash *hash)
/* Go get head and return status.  Return negative number if
 * can't get head. If hash is non-null, fill it with header
 * lines with upper cased keywords for case-insensitive lookup, 
 * including hopefully CONTENT-TYPE: . */
{
return netUrlHeadExt(url, "HEAD", hash);
}


long long netUrlSizeByRangeResponse(char *url)
/* Use byteRange as a work-around alternate method to get file size (content-length).  
 * Return negative number if can't get. */
{
long long retVal = -1;
char rangeUrl[2048];
safef(rangeUrl, sizeof(rangeUrl), "%s;byterange=0-0", url);
struct hash *hash = newHash(0);
int status = netUrlHeadExt(rangeUrl, "GET", hash);
if (status == 206)
    { 
    char *rangeString = hashFindValUpperCase(hash, "Content-Range:");
    if (rangeString)
	{
 	/* input pattern: Content-Range: bytes 0-99/2738262 */
	char *slash = strchr(rangeString,'/');
	if (slash)
	    {
	    retVal = atoll(slash+1);
	    }
	}
    }
hashFree(&hash);
return retVal;
}


int netUrlOpenSockets(char *url, int *retCtrlSocket)
/* Return socket descriptor (low-level file handle) for read()ing url data,
 * or -1 if error. 
 * If retCtrlSocket is non-NULL and url is FTP, set *retCtrlSocket
 * to the FTP control socket which is left open for a persistent connection.
 * close(result) (and close(*retCtrlSocket) if applicable) when done. 
 * If url is missing :// then it's just treated as a file. */
{
if (stringIn("://", url) == NULL)
    return open(url, O_RDONLY);
else
    {
    if (startsWith("http://",url) || startsWith("https://",url))
	return netGetOpenHttp(url);
    else if (startsWith("ftp://",url))
	return netGetOpenFtpSockets(url, retCtrlSocket);
    else    
	errAbort("Sorry, can only netUrlOpen http, https and ftp currently, not '%s'", url);
    }
return -1;    
}

int netUrlOpen(char *url)
/* Return socket descriptor (low-level file handle) for read()ing url data,
 * or -1 if error.  Just close(result) when done. */
{
return netUrlOpenSockets(url, NULL);
}

struct dyString *netSlurpFile(int sd)
/* Slurp file into dynamic string and return. */
{
char buf[4*1024];
int readSize;
struct dyString *dy = newDyString(4*1024);

/* Slurp file into dy and return. */
while ((readSize = read(sd, buf, sizeof(buf))) > 0)
    dyStringAppendN(dy, buf, readSize);
return dy;
}

struct dyString *netSlurpUrl(char *url)
/* Go grab all of URL and return it as dynamic string. */
{
int sd = netUrlOpen(url);
if (sd < 0)
    errAbort("netSlurpUrl: failed to open socket for [%s]", url);
struct dyString *dy = netSlurpFile(sd);
close(sd);
return dy;
}

static void parseContentRange(char *x, ssize_t *rangeStart, ssize_t *rangeEnd)
/* parse the content range information from response header value 
	"bytes 763400000-763400112/763400113"
 */
{
/* default to no byte range specified */
*rangeStart = -1;
*rangeEnd = -1;
if (startsWith("bytes ", x))
    {
    char *y=strchr(x, ' ');
    ++y;
    char *z=strchr(y, '-');
    if (z)
	{
	++z;
	// TODO: use something better than atoll() ?
	*rangeStart = atoll(y); 
	if (z[0] != '\0')
	    *rangeEnd = atoll(z);
	}    
    }

}


boolean netSkipHttpHeaderLinesWithRedirect(int sd, char *url, char **redirectedUrl)
/* Skip http header lines. Return FALSE if there's a problem.
 * The input is a standard sd or fd descriptor.
 * This is meant to be able work even with a re-passable stream handle,
 * e.g. can pass it to the pipes routines, which means we can't
 * attach a linefile since filling its buffer reads in more than just the http header.
 * Handles 300, 301, 302, 303, 307 http redirects by setting *redirectedUrl to
 * the new location. */
{
char buf[2000];
char *line = buf;
int maxbuf = sizeof(buf);
int i=0;
char c = ' ';
int nread = 0;
char *sep = NULL;
char *headerName = NULL;
char *headerVal = NULL;
boolean redirect = FALSE;
boolean byteRangeUsed = (strstr(url,";byterange=") != NULL);
ssize_t byteRangeStart = -1;
ssize_t byteRangeEnd = -1;
boolean foundContentRange = FALSE;
ssize_t contentRangeStart = -1;
ssize_t contentRangeEnd = -1;

boolean mustUseProxy = FALSE;  /* User must use proxy 305 error*/
char *proxyLocation = NULL;
boolean mustUseProxyAuth = FALSE;  /* User must use proxy authentication 407 error*/

if (byteRangeUsed)
    {
    parseByteRange(url, &byteRangeStart, &byteRangeEnd, FALSE);
    }

while(TRUE)
    {
    i = 0;
    while (TRUE)
	{
	nread = read(sd, &c, 1);  /* one char at a time, but http headers are small */
	if (nread != 1)
	    {
	    if (nread == -1)
    		warn("Error (%s) reading http header on %s", strerror(errno), url);
	    else if (nread == 0)
    		warn("Error unexpected end of input reading http header on %s", url);
	    else
    		warn("Error reading http header on %s", url);
	    return FALSE;  /* err reading descriptor */
	    }
	if (c == 10)
	    break;
	if (c != 13)
    	    buf[i++] = c;
	if (i >= maxbuf)
	    {
	    warn("http header line too long > %d chars.",maxbuf);
	    return FALSE;
	    }
	}
    buf[i] = 0;  /* add string terminator */

    if (sameString(line,""))
	{
	break; /* End of Header found */
	}
    if (startsWith("HTTP/", line))
        {
	char *version, *code;
	version = nextWord(&line);
	code = nextWord(&line);
	if (code == NULL)
	    {
	    warn("Strange http header on %s", url);
	    return FALSE;
	    }
	if (startsWith("30", code) && isdigit(code[2])
	    && ((code[2] >= '0' && code[2] <= '3') || code[2] == '7') && code[3] == 0)
	    {
	    redirect = TRUE;
	    }
	else if (sameString(code, "305"))
	    {
	    mustUseProxy = TRUE;
	    }
	else if (sameString(code, "407"))
	    {
	    mustUseProxyAuth = TRUE;
	    }
	else if (byteRangeUsed)
	    {
	    if (!sameString(code, "206"))
		{
		if (sameString(code, "200"))
		    warn("Byte-range request was ignored by server. ");
		warn("Expected Partial Content 206. %s: %s %s", url, code, line);
		return FALSE;
		}
	    }
	else if (!sameString(code, "200"))
	    {
	    warn("Expected 200 %s: %s %s", url, code, line);
	    return FALSE;
	    }
	line = buf;  /* restore it */
	}
    headerName = line;
    sep = strchr(line,':');
    if (sep)
	{
	*sep = 0;
	headerVal = skipLeadingSpaces(++sep);
	}
    else
	{
	headerVal = NULL;
	}
    if (sameWord(headerName,"Location"))
	{
	if (redirect)
	    *redirectedUrl = cloneString(headerVal);
	if (mustUseProxy)
	    proxyLocation = cloneString(headerVal);
	}
    if (sameWord(headerName,"Content-Range"))
	{
	if (byteRangeUsed)
	    {
	    foundContentRange = TRUE;
	    parseContentRange(headerVal, &contentRangeStart, &contentRangeEnd);	
    	    if ((contentRangeStart != byteRangeStart) ||
		(byteRangeEnd != -1 && (contentRangeEnd != byteRangeEnd)))
		{
		char bre[256];
		safef(bre, sizeof bre, "%lld", (long long)byteRangeEnd);
		if (byteRangeEnd == -1)
		    bre[0] = 0;
		warn("Found Content-Range: %s. Expected bytes %lld-%s. Improper caching of 206 reponse byte-ranges?",
		    headerVal, (long long) byteRangeStart, bre);
    		return FALSE;
		}
	    }
	}
    }
if (mustUseProxy ||  mustUseProxyAuth)
    {
    warn("%s: %s error. Use Proxy%s. Location = %s", url, 
	mustUseProxy ? "" : " Authentication", 
	mustUseProxy ? "305" : "407", 
	proxyLocation ? proxyLocation : "not given");
    return FALSE;
    }
if (byteRangeUsed && !foundContentRange)
    {
    char bre[256];
    safef(bre, sizeof bre, "%lld", (long long)byteRangeEnd);
    if (byteRangeEnd == -1)
	bre[0] = 0;
    warn("Expected response header Content-Range: %lld-%s", (long long) byteRangeStart, bre);
    return FALSE;
    }

return TRUE;
}


boolean netSkipHttpHeaderLinesHandlingRedirect(int sd, char *url, int *redirectedSd, char **redirectedUrl)
/* Skip http headers lines, returning FALSE if there is a problem.  Generally called as
 *    netSkipHttpHeaderLine(sd, url, &sd, &url);
 * where sd is a socket (file) opened with netUrlOpen(url), and url is in dynamic memory.
 * If the http header indicates that the file has moved, then it will update the *redirectedSd and
 * *redirectedUrl with the new socket and URL, first closing sd.
 * If for some reason you want to detect whether the forwarding has occurred you could
 * call this as:
 *    char *newUrl = NULL;
 *    int newSd = 0;
 *    netSkipHttpHeaderLine(sd, url, &newSd, &newUrl);
 *    if (newUrl != NULL)
 *          // Update sd with newSd, free url if appropriate and replace it with newUrl, etc.
 *          //  free newUrl when finished.
 * This routine handles up to 5 steps of redirection.
 * The logic to this routine is also complicated a little to make it work in a pipe, which means we
 * can't attach a lineFile since filling the lineFile buffer reads in more than just the http header. */
{
int redirectCount = 0;
while (TRUE)
    {
    /* url needed for err msgs, and to return redirect location */
    char *newUrl = NULL;
    boolean success = netSkipHttpHeaderLinesWithRedirect(sd, url, &newUrl);
    if (success && !newUrl) /* success after 0 to 5 redirects */
        {
	if (redirectCount > 0)
	    {
	    *redirectedSd = sd;
	    *redirectedUrl = url;
	    }
	else
	    {
	    *redirectedSd = -1;
	    *redirectedUrl = NULL;
	    }
	return TRUE;
	}
    close(sd);
    if (redirectCount > 0)
	freeMem(url);
    if (success)
	{
	/* we have a new url to try */
	++redirectCount;
	if (redirectCount > 5)
	    {
	    warn("code 30x redirects: exceeded limit of 5 redirects, %s", newUrl);
	    success = FALSE;
	    }
	else if (!startsWith("http://",newUrl) 
              && !startsWith("https://",newUrl))
	    {
	    warn("redirected to non-http(s): %s", newUrl);
	    success = FALSE;
	    }
	else 
	    {
	    struct netParsedUrl npu, newNpu;
	    /* Parse the old URL to make parts available for graft onto the redirected url. */
	    /* This makes redirection work with byterange urls and user:password@ */
	    netParseUrl(url, &npu);
	    netParseUrl(newUrl, &newNpu);
	    boolean updated = FALSE;
	    if (npu.byteRangeStart != -1)
		{
		newNpu.byteRangeStart = npu.byteRangeStart;
		newNpu.byteRangeEnd = npu.byteRangeEnd;
		updated = TRUE;
		}
	    if ((npu.user[0] != 0) && (newNpu.user[0] == 0))
		{
		safecpy(newNpu.user,     sizeof newNpu.user,     npu.user);
		safecpy(newNpu.password, sizeof newNpu.password, npu.password);
		updated = TRUE;
		}
	    if (updated)
		{
		freeMem(newUrl);
		newUrl = urlFromNetParsedUrl(&newNpu);
		}
	    sd = netUrlOpen(newUrl);
	    if (sd < 0)
		{
		warn("Couldn't open %s", newUrl);
		success = FALSE;
		}
	    }
	}
    if (!success)
	{  /* failure after 0 to 5 redirects */
	if (redirectCount > 0)
	    freeMem(newUrl);
	return FALSE;
	}
    url = newUrl;
    }
return FALSE;
}

struct lineFile *netLineFileMayOpen(char *url)
/* Return a lineFile attached to url. http skips header.
 * Supports some compression formats.  Prints warning message, but
 * does not abort, just returning NULL if there's a problem. */
{
int sd = netUrlOpen(url);
if (sd < 0)
    {
    warn("Couldn't open %s", url);
    return NULL;
    }
else
    {
    struct lineFile *lf = NULL;
    char *newUrl = NULL;
    int newSd = 0;
    if (startsWith("http://",url) || startsWith("https://",url))
	{  
	if (!netSkipHttpHeaderLinesHandlingRedirect(sd, url, &newSd, &newUrl))
	    {
	    return NULL;
	    }
	if (newUrl != NULL)
	    {
    	    /*  Update sd with newSd, replace it with newUrl, etc. */
	    sd = newSd;
	    url = newUrl;
	    }
	}
    if (endsWith(url, ".gz") ||
	endsWith(url, ".Z")  ||
    	endsWith(url, ".bz2"))
	{
	lf = lineFileDecompressFd(url, TRUE, sd);
           /* url needed only for compress type determination */
	}
    else
	{
	lf = lineFileAttach(url, TRUE, sd);
	}
    if (newUrl) 
	freeMem(newUrl); 
    return lf;
    }
}

struct lineFile *netLineFileSilentOpen(char *url)
/* Open a lineFile on a URL.  Just return NULL without any user
 * visible warning message if there's a problem. */
{
pushSilentWarnHandler();
struct lineFile *lf = netLineFileMayOpen(url);
popWarnHandler();
return lf;
}

char *netReadTextFileIfExists(char *url)
/* Read entire URL and return it as a string.  URL should be text (embedded zeros will be
 * interpreted as end of string).  If the url doesn't exist or has other problems,
 * returns NULL. */
{
struct lineFile *lf = netLineFileSilentOpen(url);
if (lf == NULL)
    return NULL;
char *text = lineFileReadAll(lf);
lineFileClose(&lf);
return text;
}


struct parallelConn
/* struct to information on a parallel connection */
    {
    struct parallelConn *next;  /* next connection */
    int sd;                     /* socket descriptor */
    off_t rangeStart;           /* where does the range start */
    off_t partSize;             /* range size */
    off_t received;             /* bytes received */
    };

void writeParaFetchStatus(char *origPath, struct parallelConn *pcList, char *url, off_t fileSize, char *dateString, boolean isFinal)
/* Write a status file.
 * This has two purposes.
 * First, we can use it to resume a failed transfer.
 * Second, we can use it to follow progress */
{
char outTempX[1024];
char outTemp[1024];
safef(outTempX, sizeof(outTempX), "%s.paraFetchStatusX", origPath);
safef(outTemp, sizeof(outTemp), "%s.paraFetchStatus", origPath);
struct parallelConn *pc = NULL;

FILE *f = mustOpen(outTempX, "w");
int part = 0;
fprintf(f, "%s\n", url);
fprintf(f, "%lld\n", (long long)fileSize);
fprintf(f, "%s\n", dateString);
for(pc = pcList; pc; pc = pc->next)
    {
    fprintf(f, "part%d %lld %lld %lld\n", part
	, (long long)pc->rangeStart
	, (long long)pc->partSize
	, (long long)pc->received);
    ++part;
    }

carefulClose(&f);

/* rename the successful status to the original name */
rename(outTempX, outTemp);

if (isFinal)  /* We are done and just looking to get rid of the file. */
    unlink(outTemp);
}


boolean readParaFetchStatus(char *origPath, 
    struct parallelConn **pPcList, char **pUrl, off_t *pFileSize, char **pDateString, off_t *pTotalDownloaded)
/* Write a status file.
 * This has two purposes.
 * First, we can use it to resume a failed transfer.
 * Second, we can use it to follow progress */
{
char outTemp[1024];
char outStat[1024];
safef(outStat, sizeof(outStat), "%s.paraFetchStatus", origPath);
safef(outTemp, sizeof(outTemp), "%s.paraFetch", origPath);
struct parallelConn *pcList = NULL, *pc = NULL;
off_t totalDownloaded = 0;

if (!fileExists(outStat))
    {
    unlink(outTemp);
    return FALSE;
    }

if (!fileExists(outTemp))
    {
    unlink(outStat);
    return FALSE;
    }

char *line, *word;
struct lineFile *lf = lineFileOpen(outStat, TRUE);
if (!lineFileNext(lf, &line, NULL))
    {
    unlink(outTemp);
    unlink(outStat);
    return FALSE;
    }
char *url = cloneString(line);
if (!lineFileNext(lf, &line, NULL))
    {
    unlink(outTemp);
    unlink(outStat);
    return FALSE;
    }
off_t fileSize = sqlLongLong(line);
if (!lineFileNext(lf, &line, NULL))
    {
    unlink(outTemp);
    unlink(outStat);
    return FALSE;
    }
char *dateString = cloneString(line);
while (lineFileNext(lf, &line, NULL))
    {
    word = nextWord(&line);
    AllocVar(pc);
    pc->next = NULL;
    pc->sd = -4;  /* no connection tried yet */
    word = nextWord(&line);
    pc->rangeStart = sqlLongLong(word);
    word = nextWord(&line);
    pc->partSize = sqlLongLong(word);
    word = nextWord(&line);
    pc->received = sqlLongLong(word);
    if (pc->received == pc->partSize)
	pc->sd = -1;  /* part all done already */
    totalDownloaded += pc->received;
    slAddHead(&pcList, pc);
    }
slReverse(&pcList);

lineFileClose(&lf);

if (slCount(pcList) < 1)
    {
    unlink(outTemp);
    unlink(outStat);
    return FALSE;
    }

*pPcList = pcList;
*pUrl = url;
*pFileSize = fileSize;
*pDateString = dateString;
*pTotalDownloaded = totalDownloaded;

return TRUE;

}


boolean parallelFetch(char *url, char *outPath, int numConnections, int numRetries, boolean newer, boolean progress)
/* Open multiple parallel connections to URL to speed downloading */
{
char *origPath = outPath;
char outTemp[1024];
safef(outTemp, sizeof(outTemp), "%s.paraFetch", outPath);
outPath = outTemp;
/* get the size of the file to be downloaded */
off_t fileSize = 0;
off_t totalDownloaded = 0;
ssize_t sinceLastStatus = 0;
char *dateString = "";
int star = 1;  
int starMax = 20;  
int starStep = 1;
// TODO handle case-sensitivity of protocols input
if (startsWith("http://",url) || startsWith("https://",url))
    {
    struct hash *hash = newHash(0);
    int status = netUrlHead(url, hash);
    if (status != 200) // && status != 302 && status != 301)
	{
	warn("Error code: %d, expected 200 for %s, can't proceed, sorry", status, url);
	return FALSE;
	}
    char *sizeString = hashFindValUpperCase(hash, "Content-Length:");
    if (sizeString)
	{
	fileSize = atoll(sizeString);
	}
    else
	{
	warn("No Content-Length: returned in header for %s, must limit to a single connection, will not know if data is complete", url);
	numConnections = 1;
	fileSize = -1;
	}
    char *ds = hashFindValUpperCase(hash, "Last-Modified:");
    if (ds)
	dateString = cloneString(ds);
    hashFree(&hash);
    }
else if (startsWith("ftp://",url))
    {
    long long size = 0;
    time_t t;
    boolean ok = netGetFtpInfo(url, &size, &t);
    if (!ok)
	{
	warn("Unable to get size info from FTP for %s, can't proceed, sorry", url);
	return FALSE;
	}
    fileSize = size;

    struct tm  *ts;
    char ftpTime[80];
 
    /* Format the time "Tue, 15 Jun 2010 06:45:08 GMT" */
    ts = localtime(&t);
    strftime(ftpTime, sizeof(ftpTime), "%a, %d %b %Y %H:%M:%S %Z", ts);
    dateString = cloneString(ftpTime);

    }
else
    {
    warn("unrecognized protocol: %s", url);
    return FALSE;
    }


verbose(2,"fileSize=%lld\n", (long long) fileSize);

if (fileSize < 65536)    /* special case small file */
    numConnections = 1;

if (numConnections > 50)    /* ignore high values for numConnections */
    {
    warn("Currently maximum number of connections is 50. You requested %d. Will proceed with 50 on %s", numConnections, url);
    numConnections = 50;
    }

verbose(2,"numConnections=%d\n", numConnections); //debug

if (numConnections < 1)
    {
    warn("number of connections must be greater than 0 for %s, can't proceed, sorry", url);
    return FALSE;
    }

if (numRetries < 0)
    numRetries = 0;

/* what is the size of each part */
off_t partSize = (fileSize + numConnections -1) / numConnections;
if (fileSize == -1) 
    partSize = -1;
off_t base = 0;
int c;

verbose(2,"partSize=%lld\n", (long long) partSize); //debug


/* n is the highest-numbered descriptor */
int n = 0;
int connOpen = 0;
int reOpen = 0;


struct parallelConn *restartPcList = NULL;
char *restartUrl = NULL;
off_t restartFileSize = 0;
char *restartDateString = "";
off_t restartTotalDownloaded = 0;
boolean restartable = readParaFetchStatus(origPath, &restartPcList, &restartUrl, &restartFileSize, &restartDateString, &restartTotalDownloaded);
if (fileSize == -1)
    restartable = FALSE;

struct parallelConn *pcList = NULL, *pc;

if (restartable 
 && sameString(url, restartUrl)
 && fileSize == restartFileSize
 && sameString(dateString, restartDateString))
    {
    pcList = restartPcList;
    totalDownloaded = restartTotalDownloaded;
    }
else
    {

    if (newer) // only download it if it is newer than what we already have
	{
	/* datestamp mtime from last-modified header */
	struct tm tm;
	// Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT
	// These strings are always GMT
	if (strptime(dateString, "%a, %d %b %Y %H:%M:%S %Z", &tm) == NULL)
	    {
	    warn("unable to parse last-modified string [%s]", dateString);
	    }
	else
	    {
	    time_t t;
	    // convert to UTC (GMT) time
	    t = mktimeFromUtc(&tm);
	    if (t == -1)
		{
		warn("mktimeFromUtc failed while converting last-modified string to UTC [%s]", dateString);
		}
	    else
		{
		// get the file mtime
		struct stat mystat;
		ZeroVar(&mystat);
		if (stat(origPath,&mystat)==0)
		    {
		    if (t <= mystat.st_mtime)
			{
			verbose(2,"Since nothing newer was found, skipping %s\n", origPath);
			verbose(3,"t from last-modified = %ld; st_mtime = %ld\n", (long) t, (long)mystat.st_mtime);
			return TRUE;
			}
		    }
		}
	    }
	}

    /* make a list of connections */
    for (c = 0; c < numConnections; ++c)
	{
	AllocVar(pc);
	pc->next = NULL;
	pc->rangeStart = base;
	base += partSize;
	pc->partSize = partSize;
	if (fileSize != -1 && pc->rangeStart+pc->partSize >= fileSize)
	    pc->partSize = fileSize - pc->rangeStart;
	pc->received = 0;
	pc->sd = -4;  /* no connection tried yet */
	slAddHead(&pcList, pc);
	}
    slReverse(&pcList);
    }

if (progress)
    {
    char nicenumber[1024]="";
    sprintWithGreekByte(nicenumber, sizeof(nicenumber), fileSize);
    printf("downloading %s ", nicenumber); fflush(stdout);
    starStep = fileSize/starMax;
    if (starStep < 1)
	starStep = 1;
    }

int out = open(outPath, O_CREAT|O_WRONLY, 0664);
if (out < 0)
    {
    warn("Unable to open %s for write while downloading %s, can't proceed, sorry", url, outPath);
    return FALSE;
    }


/* descriptors for select() */
fd_set rfds;
struct timeval tv;
int retval;

ssize_t readCount = 0;
#define BUFSIZE 65536 * 4
char buf[BUFSIZE];

/* create paraFetchStatus right away for monitoring programs */
writeParaFetchStatus(origPath, pcList, url, fileSize, dateString, FALSE);
sinceLastStatus = 0;

int retryCount = 0;

time_t startTime = time(NULL);

#define SELTIMEOUT 5
#define RETRYSLEEPTIME 30    
while (TRUE)
    {

    verbose(2,"Top of big loop\n");

    if (progress)
	{
	while (totalDownloaded >= star * starStep)
	    {
	    printf("*");fflush(stdout);
	    ++star;
	    }
	}

    /* are we done? */
    if (connOpen == 0)
	{
	boolean done = TRUE;
	for(pc = pcList; pc; pc = pc->next)
	    if (pc->sd != -1)
		done = FALSE;
	if (done) break;
	}

    /* See if we need to open any connections, either new or retries */
    for(pc = pcList; pc; pc = pc->next)
	{
	if ((pc->sd == -4)  /* never even tried to open yet */
	 || ((reOpen>0)      /* some connections have been released */
	    && (pc->sd == -2  /* started but not finished */
	    ||  pc->sd == -3)))  /* not even started */
	    {
	    char urlExt[1024];
	    safef(urlExt, sizeof(urlExt), "%s;byterange=%llu-%llu"
	    , url
	    , (unsigned long long) (pc->rangeStart + pc->received)
	    , (unsigned long long) (pc->rangeStart + pc->partSize - 1) );


	    int oldSd = pc->sd;  /* in case we need to remember where we were */
	    if (oldSd != -4)      /* decrement whether we succeed or not */
		--reOpen;
	    if (oldSd == -4) 
		oldSd = -3;       /* ok this one just changes */
	    if (fileSize == -1)
		{
		verbose(2,"opening url %s\n", url);
		pc->sd = netUrlOpen(url);
		}
	    else
		{
		verbose(2,"opening url %s\n", urlExt);
		pc->sd = netUrlOpen(urlExt);
		}
	    if (pc->sd < 0)
		{
		pc->sd = oldSd;  /* failed to open, can retry later */
		}
	    else
		{
		char *newUrl = NULL;
		int newSd = 0;
		if (startsWith("http://",url) || startsWith("https://",url))
		    {
		    if (!netSkipHttpHeaderLinesHandlingRedirect(pc->sd, urlExt, &newSd, &newUrl))
			{
			warn("Error processing http response for %s", url);
			return FALSE;
			}
		    if (newUrl) 
			{
			/*  Update sd with newSd, replace it with newUrl, etc. */
			pc->sd = newSd;
			}
		    }
		++connOpen;
		}
	    }
	}


    if (connOpen == 0)
	{
	warn("Unable to open any connections to download %s, can't proceed, sorry", url);
	return FALSE;
	}


    FD_ZERO(&rfds);
    n = 0;
    for(pc = pcList; pc; pc = pc->next)
	{
	if (pc->sd >= 0)
	    {
	    FD_SET(pc->sd, &rfds);    /* reset descriptor in readfds for select() */
	    if (pc->sd > n)
		n = pc->sd;
	    }
	}


    /* Wait up to SELTIMEOUT seconds. */
    tv.tv_sec = SELTIMEOUT;
    tv.tv_usec = 0;
    retval = select(n+1, &rfds, NULL, NULL, &tv);
    /* Don't rely on the value of tv now! */

    if (retval < 0)
	{
	perror("select retval < 0");
	return FALSE;
	}
    else if (retval)
	{

	verbose(2,"returned from select, retval=%d\n", retval);

	for(pc = pcList; pc; pc = pc->next)
	    {
	    if ((pc->sd >= 0) && FD_ISSET(pc->sd, &rfds))
		{

		verbose(2,"found a descriptor with data: %d\n", pc->sd);

		readCount = read(pc->sd, buf, BUFSIZE);

		verbose(2,"readCount = %lld\n", (long long) readCount);

		if (readCount == 0)
		    {
		    close(pc->sd);

		    verbose(2,"closing descriptor: %d\n", pc->sd);
		    pc->sd = -1;

		    if (fileSize != -1 && pc->received != pc->partSize)	
			{
			pc->sd = -2;  /* conn was closed before all data was sent, can retry later */
			return FALSE;
			}
		    --connOpen;
		    ++reOpen;
		    writeParaFetchStatus(origPath, pcList, url, fileSize, dateString, FALSE);
		    sinceLastStatus = 0;
		    continue; 
		    }
		if (readCount < 0)
		    {
		    warn("error reading from socket for url %s", url);
		    return FALSE;
		    }

		verbose(2,"rangeStart %llu  received %llu\n"
			, (unsigned long long) pc->rangeStart
			, (unsigned long long) pc->received );

		verbose(2,"seeking to %llu\n", (unsigned long long) (pc->rangeStart + pc->received));

		if (lseek(out, pc->rangeStart + pc->received, SEEK_SET) == -1)
		    {
		    perror("error seeking output file");
		    warn("error seeking output file %s: rangeStart %llu  received %llu for url %s"
			, outPath
			, (unsigned long long) pc->rangeStart
			, (unsigned long long) pc->received
			, url);
		    return FALSE;
		    }
		int writeCount = write(out, buf, readCount);
		if (writeCount < 0)
		    {
		    warn("error writing output file %s", outPath);
		    return FALSE;
		    }
		pc->received += readCount;
		totalDownloaded += readCount;
		sinceLastStatus += readCount;
		if (sinceLastStatus >= 100*1024*1024)
		    {
		    writeParaFetchStatus(origPath, pcList, url, fileSize, dateString, FALSE);
		    sinceLastStatus = 0;
		    }
		}
	    }
	}
    else
	{
	warn("No data within %d seconds for %s", SELTIMEOUT, url);
	/* Retry ? */
	if (retryCount >= numRetries)
	    {
    	    return FALSE;
	    }
	else
	    {
	    ++retryCount;
	    /* close any open connections */
	    for(pc = pcList; pc; pc = pc->next)
		{
		if (pc->sd >= 0) 
		    {
		    close(pc->sd);
		    verbose(2,"closing descriptor: %d\n", pc->sd);
		    }
		if (pc->sd != -1) 
		    pc->sd = -4;
		}
	    connOpen = 0;
	    reOpen = 0;
	    /* sleep for a while, maybe the server will recover */
	    sleep(RETRYSLEEPTIME);
	    }
	}

    }

close(out);

/* delete the status file - by passing TRUE */
writeParaFetchStatus(origPath, pcList, url, fileSize, dateString, TRUE); 

/* restore original file datestamp mtime from last-modified header */
struct tm tm;
// Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT
// These strings are always GMT
if (strptime(dateString, "%a, %d %b %Y %H:%M:%S %Z", &tm) == NULL)
    {
    warn("unable to parse last-modified string [%s]", dateString);
    }
else
    {
    time_t t;
    // convert to UTC (GMT) time
    t = mktimeFromUtc(&tm);
    if (t == -1)
	{
	warn("mktimeFromUtc failed while converting last-modified string to UTC [%s]", dateString);
	}
    else
	{
	// update the file mtime
	struct utimbuf ut;
	struct stat mystat;
	ZeroVar(&mystat);
	if (stat(outTemp,&mystat)==0)
	    {
	    ut.actime = mystat.st_atime;
	    ut.modtime = t;
	    if (utime(outTemp, &ut)==-1)
		{
		char errMsg[256];
                safef(errMsg, sizeof(errMsg), "paraFetch: error setting modification time of %s to %s\n", outTemp, dateString);
		perror(errMsg);
		}
	    }
	}
    }

/* rename the successful download to the original name */
rename(outTemp, origPath);



if (progress)
    {
    while (star <= starMax)
	{
	printf("*");fflush(stdout);
	++star;
	}
    long timeDiff = (long)(time(NULL) - startTime);
    if (timeDiff > 0)
	{
	printf(" %ld seconds", timeDiff);
	float mbpersec =  ((totalDownloaded - restartTotalDownloaded)/1000000) / timeDiff;
	printf(" %0.1f MB/sec", mbpersec);
	}
    printf("\n");fflush(stdout);
    }

if (fileSize != -1 && totalDownloaded != fileSize)
    {
    warn("Unexpected result: Total downloaded bytes %lld is not equal to fileSize %lld"
	, (long long) totalDownloaded
	, (long long) fileSize);
    return FALSE;
    }
return TRUE;
}


struct lineFile *netLineFileOpen(char *url)
/* Return a lineFile attached to url.  This one
 * will skip any headers.   Free this with
 * lineFileClose(). */
{
struct lineFile *lf = netLineFileMayOpen(url);
if (lf == NULL)
    noWarnAbort();
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
    {
    warn("Trying to send a string longer than 64k bytes (%d bytes)", length);
    return FALSE;
    }
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

boolean netSendHugeString(int sd, char *s)
/* Send a long string down socket: four bytes for length. */
{
unsigned long length = strlen(s);
unsigned long l = length;
UBYTE b[4];
int i;
for (i=3; i>=0; --i)
    {
    b[i] = l & 0xff;
    l >>= 8;
    }
if (write(sd, b, 4) < 0)
    {
    warn("Couldn't send huge string to socket");
    return FALSE;
    }
if (write(sd, s, length) < 0)
    {
    warn("Couldn't send huge string to socket");
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
UBYTE len = 0;
int length;
int sz;
if (buf == NULL) buf = sbuf;
sz = netReadAll(sd, &len, 1);
if (sz == 0)
    return NULL;
if (sz < 0)
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
int sz;
b[0] = b[1] = 0;
sz = netReadAll(sd, b, 2);
if (sz == 0)
    return NULL;
if (sz < 0)
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

char *netGetHugeString(int sd)
/* Read string and return it.  freeMem
 * the result when done. */
{
UBYTE b[4];
char *s = NULL;
unsigned long length = 0;
int sz, i;
sz = netReadAll(sd, b, 4);
if (sz == 0)
    return NULL;
if (sz < 4)
    {
    warn("Couldn't read huge string length");
    return NULL;
    }
for (i=0; i<4; ++i)
    {
    length <<= 8;
    length += b[i];
    }
s = needMem(length+1);
if (length > 0)
    {
    if (netReadAll(sd, s, length) < 0)
	{
	warn("Couldn't read huge string body");
	return NULL;
	}
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

char *netRecieveHugeString(int sd)
/* Read string and return it.  freeMem
 * the result when done. Abort if any problem*/
{
char *s = netGetHugeString(sd);
if (s == NULL)
     noWarnAbort();   
return s;
}


struct lineFile *netHttpLineFileMayOpen(char *url, struct netParsedUrl **npu)
/* Parse URL and open an HTTP socket for it but don't send a request yet. */
{
int sd;
struct lineFile *lf;

/* Parse the URL and try to connect. */
AllocVar(*npu);
netParseUrl(url, *npu);
if (!sameString((*npu)->protocol, "http"))
    errAbort("netHttpLineFileMayOpen: url (%s) is not for http.", url);
sd = netConnect((*npu)->host, atoi((*npu)->port));
if (sd < 0)
    return NULL;

/* Return handle. */
lf = lineFileAttach(url, TRUE, sd);
return lf;
} /* netHttpLineFileMayOpen */


void netHttpGet(struct lineFile *lf, struct netParsedUrl *npu,
		boolean keepAlive)
/* Send a GET request, possibly with Keep-Alive. */
{
struct dyString *dy = newDyString(512);

/* Ask remote server for the file/query. */
dyStringPrintf(dy, "GET %s HTTP/1.1\r\n", npu->file);
dyStringPrintf(dy, "User-Agent: genome.ucsc.edu/net.c\r\n");
dyStringPrintf(dy, "Host: %s:%s\r\n", npu->host, npu->port);
if (!sameString(npu->user,""))
    {
    char up[256];
    char *b64up = NULL;
    safef(up,sizeof(up), "%s:%s", npu->user, npu->password);
    b64up = base64Encode(up, strlen(up));
    dyStringPrintf(dy, "Authorization: Basic %s\r\n", b64up);
    freez(&b64up);
    }
dyStringAppend(dy, "Accept: */*\r\n");
if (keepAlive)
  {
    dyStringAppend(dy, "Connection: Keep-Alive\r\n");
    dyStringAppend(dy, "Connection: Persist\r\n");
  }
else
    dyStringAppend(dy, "Connection: close\r\n");
dyStringAppend(dy, "\r\n");
mustWriteFd(lf->fd, dy->string, dy->stringSize);
/* Clean up. */
dyStringFree(&dy);
} /* netHttpGet */

int netHttpGetMultiple(char *url, struct slName *queries, void *userData,
		       void (*responseCB)(void *userData, char *req,
					  char *hdr, struct dyString *body))
/* Given an URL which is the base of all requests to be made, and a 
 * linked list of queries to be appended to that base and sent in as 
 * requests, send the requests as a batch and read the HTTP response 
 * headers and bodies.  If not all the requests get responses (i.e. if 
 * the server is ignoring Keep-Alive or is imposing a limit), try again 
 * until we can't connect or until all requests have been served. 
 * For each HTTP response, do a callback. */
{
  struct slName *qStart;
  struct slName *qPtr;
  struct lineFile *lf;
  struct netParsedUrl *npu;
  struct dyString *dyQ    = newDyString(512);
  struct dyString *body;
  char *base;
  char *hdr;
  int qCount;
  int qTotal;
  int numParseFailures;
  int contentLength;
  boolean chunked;
  boolean done;
  boolean keepAlive;

  /* Find out how many queries we'll need to do so we know how many times 
   * it's OK to run into end of file in case server ignores Keep-Alive. */
  qTotal = 0;
  for (qPtr = queries;  qPtr != NULL;  qPtr = qPtr->next)
    {
      qTotal++;
    }

  done = FALSE;
  qCount = 0;
  numParseFailures = 0;
  qStart = queries;
  while ((! done) && (qStart != NULL))
    {
      lf = netHttpLineFileMayOpen(url, &npu);
      if (lf == NULL)
	{
	  done = TRUE;
	  break;
	}
      base = cloneString(npu->file);
      /* Send all remaining requests with keep-alive. */
      for (qPtr = qStart;  qPtr != NULL;  qPtr = qPtr->next)
	{
	  dyStringClear(dyQ);
	  dyStringAppend(dyQ, base);
	  dyStringAppend(dyQ, qPtr->name);
	  strcpy(npu->file, dyQ->string);
	  keepAlive = (qPtr->next == NULL) ? FALSE : TRUE;
	  netHttpGet(lf, npu, keepAlive);
	}
      /* Get as many responses as we can; call responseCB() and 
       * advance qStart for each. */
      for (qPtr = qStart;  qPtr != NULL;  qPtr = qPtr->next)
        {
	  if (lineFileParseHttpHeader(lf, &hdr, &chunked, &contentLength))
	    {
	      body = lineFileSlurpHttpBody(lf, chunked, contentLength);
	      dyStringClear(dyQ);
	      dyStringAppend(dyQ, base);
	      dyStringAppend(dyQ, qPtr->name);
	      responseCB(userData, dyQ->string, hdr, body);
	      qStart = qStart->next;
	      qCount++;
	    }
	  else
	    {
	      if (numParseFailures++ > qTotal) {
		done = TRUE;
	      }
	      break;
	    }
	}
    }

  return qCount;
} /* netHttpMultipleQueries */


