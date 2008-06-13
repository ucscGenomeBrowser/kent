/* rudp - (semi) reliable UDP communication.  This adds an
 * acknowledgement and resend layer on top of UDP. 
 *
 * UDP is a packet based rather than stream based internet communication 
 * protocol. Messages sent by UDP are checked for integrety by the UDP layer, 
 * and discarded if transmission errors are detected.  However packets are
 * not necessarily received in the same order that they are sent,
 * and packets may be duplicated or lost.  
 
 * Using rudp packets are only very rarely lost, and the sender is 
 * notified if they are.  After rudp there are still duplicate
 * packets that may arrive out of order.  Aside from the duplicates
 * the packets are in order though.
 *
 * For many, perhaps most applications, TCP/IP is a saner choice
 * than UDP or rudp.  If the communication channel is between just 
 * two computers you can pretty much just treat TCP/IP as a fairly
 * reliable pipe.   However if the communication involves many
 * computers sometimes UDP can be a better choice.  It is possible to
 * do broadcast and multicast with UDP but not with TCP/IP.  Also
 * for systems like parasol, where a server may be making and breaking
 * connections rapidly to thousands of computers, TCP paradoxically
 * can end up less reliable than UDP.  Though TCP is relatively 
 * robust when a connection is made,  it can relatively easily fail
 * to make a connection in the first place, and spend quite a long
 * time figuring out that the connection can't be made.  Moreover at
 * the end of each connection TCP goes into a 'TIMED_WAIT' state,  which
 * prevents another connection from coming onto the same port for a
 * time that can be as long as 255 seconds.  Since there are only
 * about 15000 available ports,  this limits TCP/IP to 60 connections
 * per second in some cases.  Generally the system does not handle
 * running out of ports gracefully, and this did occur with the 
 * TCP/IP based version of Parasol.
 *
 * This module puts a thin layer around UDP to make it a little bit more
 * reliable.  Currently the interface is geared towards Parasol rather
 * than broadcast type applications.  This module will try to send
 * a message a limited number of times before giving up.  It puts
 * a small header containing a message ID and timestamp on each message.   
 * This header is echoed back in acknowledgment by the reciever. This
 * echo lets the sender know if it needs to resend the message, and
 * lets it know how long a message takes to get to the destination and
 * back.  It uses this round trip time information to figure out how
 * long to wait between resends. 
 *
 * Much of this code is based on the 'Adding Reliability to a UDP Application
 * section in volume I, chapter 20, section 5, of _UNIX Network Programming_
 * by W. Richard Stevens. */


#include "common.h"
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "errabort.h"
#include "rudp.h"

static char const rcsid[] = "$Id: rudp.c,v 1.21 2008/06/13 23:08:55 galt Exp $";

#define MAX_TIME_OUT 999999

static int rudpCalcTimeOut(struct rudp *ru)
/* Return calculation of time out based on current data. */
{
int timeOut = ru->rttAve + (ru->rttVary<<2);
if (timeOut > MAX_TIME_OUT) timeOut = MAX_TIME_OUT; /* No more than a second. */
if (timeOut < 10000) timeOut = 10000;	/* No less than 1/100th second */
return timeOut;
}

static void rudpAddRoundTripTime(struct rudp *ru, int time)
/* Add info about round trip time and based on this recalculate
 * time outs. */
{
int delta;
ru->rttLast = time;
delta = time - ru->rttAve;
ru->rttAve += (delta>>3);		      /* g = 1/8 */
if (delta < 0) delta = -delta;
ru->rttVary += ((delta - ru->rttVary) >> 2);  /* h = 1/4 */
ru->timeOut = rudpCalcTimeOut(ru);
}

static void rudpTimedOut(struct rudp *ru)
/* Tell system about a time-out. */
{
ru->timeOut <<=  1;   /* Back off exponentially. */
if (ru->timeOut >= MAX_TIME_OUT)
    ru->timeOut = MAX_TIME_OUT;
}

struct rudp *rudpNew(int socket)
/* Wrap a rudp around a socket. Call rudpFree when done, or
 * rudpClose if you also want to close(socket). */
{
struct rudp *ru;
assert(socket >= 0);
AllocVar(ru);
ru->socket = socket;
ru->rttVary = 250;	/* Initial variance 250 microseconds. */
ru->timeOut = rudpCalcTimeOut(ru);
ru->maxRetries = 7;
return ru;
}

void rudpFree(struct rudp **pRu)
/* Free up rudp.  Note this does *not* close the associated socket. */
{
freez(pRu);
}

struct rudp *rudpOpen()
/* Open up an unbound rudp.   This is suitable for
 * writing to and for reading responses.  However 
 * you'll want to rudpBind if you want to listen for
 * incoming messages.   Call rudpClose() when done 
 * with this one.  Warns and returns NULL if there is
 * a problem. */
{
int sd = socket(AF_INET,  SOCK_DGRAM, IPPROTO_UDP);
if (sd < 0)
    {
    warn("Couldn't open socket in rudpOpen %s", strerror(errno));
    return NULL;
    }
return rudpNew(sd);
}

struct rudp *rudpMustOpen()
/* Open up unbound rudp.  Warn and die if there is a problem. */
{
struct rudp *ru = rudpOpen();
if (ru == NULL)
    noWarnAbort();
return ru;
}

struct rudp *rudpOpenBound(struct sockaddr_in *sai)
/* Open up a rudp socket bound to a particular port and address.
 * Use this rather than rudpOpen if you want to wait for
 * messages at a specific address in a server or the like. */
{
struct rudp *ru = rudpOpen();
if (ru != NULL)
    {
    if (bind(ru->socket, (struct sockaddr *)sai, sizeof(*sai)) < 0)
	{
	warn("Couldn't bind rudp socket: %s", strerror(errno));
	rudpClose(&ru);
	}
    }
return ru;
}

struct rudp *rudpMustOpenBound(struct sockaddr_in *sai)
/* Open up a rudp socket bound to a particular port and address
 * or die trying. */
{
struct rudp *ru = rudpOpenBound(sai);
if (ru == NULL)
    noWarnAbort();
return ru;
}

void rudpClose(struct rudp **pRu)
/* Close socket and free memory. */
{
struct rudp *ru = *pRu;
if (ru != NULL)
    {
    close(ru->socket);
    rudpFree(pRu);
    }
}

static int timeDiff(struct timeval *t1, struct timeval *t2)
/* Return difference in microseconds between t1 and t2.  t2 must be
 * later than t1 (but less than 50 minutes later). */
{
int secDiff = t2->tv_sec - t1->tv_sec;
int microDiff = 0;
if (secDiff != 0)
    microDiff = secDiff * 1000000;
microDiff += (t2->tv_usec - t1->tv_usec);
if (microDiff < 0)
    {
    /* Note, this case actually happens, currently particularly on
     * kkr2u62 and kkr8u19.  I think this is just a bug in their clock
     * hardware/software.  However in general it _could_ happen very
     * rarely on normal machines when the clock is reset by the
     * network time protocol thingie. */
    warn("t1 %u.%u, t2 %u.%u.  t1 > t2 but later?!", (unsigned)t1->tv_sec,
         (unsigned)t1->tv_usec, (unsigned)t2->tv_sec, (unsigned)t2->tv_usec);
    microDiff = 0;
    }
return microDiff;
}

static boolean readReadyWait(int sd, int microseconds)
/* Wait for descriptor to have some data to read, up to
 * given number of microseconds. */
{
struct timeval tv;
fd_set set;
int readyCount;

for (;;)
    {
    if (microseconds > 1000000)
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

static boolean getOurAck(struct rudp *ru, struct timeval *startTv, struct sockaddr_in *sai)
/* Wait for acknowledgement to the message we just sent.
 * The set should be zeroed out. Only wait for up to
 * ru->timeOut microseconds.   Prints a message and returns FALSE
 * if there's a problem.   */
{
struct rudpHeader head;
int readSize;
int timeOut = ru->timeOut;
struct sockaddr_in retFrom;
unsigned int retFromSize = sizeof(retFrom);

for (;;)
    {
    /* Set up select with our time out. */
    int dt;
    struct timeval tv;

    if (readReadyWait(ru->socket, timeOut))
	{
	/* Read message and if it's our ack return true.   */
	readSize = recvfrom(ru->socket, &head, sizeof(head), 0, 
		    (struct sockaddr*)&retFrom, &retFromSize);
	if (readSize >= sizeof(head) && head.type == rudpAck && head.id == ru->lastId)
	    {
	    if ((sai->sin_addr.s_addr==retFrom.sin_addr.s_addr) &&
		(sai->sin_port==retFrom.sin_port))
		{
		gettimeofday(&tv, NULL);
		dt = timeDiff(startTv, &tv);
		rudpAddRoundTripTime(ru, dt);
		return TRUE;
		}
	    else
		{
		char retFromDottedQuad[17];
		char saiDottedQuad[17];
		internetIpToDottedQuad(retFrom.sin_addr.s_addr, retFromDottedQuad);
		internetIpToDottedQuad(sai->sin_addr.s_addr, saiDottedQuad);
		warn("rudp: discarding mistaken ack from %s:%d by confirming recipient ip:port %s:%d"
		    , retFromDottedQuad, retFrom.sin_port
		    , saiDottedQuad, sai->sin_port
		    );
		}
	    }
	}

    /* If we got to here then we did get a message, but it's not our
     * ack.  We ignore the message and loop around again,  but update
     * our timeout so that we won't keep getting other people's messages
     * forever. */
    gettimeofday(&tv, NULL);
    timeOut = ru->timeOut - timeDiff(startTv, &tv);
    if (timeOut <= 0)
	return FALSE;
    }
}

int rudpSend(struct rudp *ru, struct sockaddr_in *sai, void *message, int size)
/* Send message of given size to port at host via rudp.  Prints a warning and
 * sets errno and returns -1 if there's a problem. */
{
struct timeval sendTv;	/* Current time. */

char outBuf[udpEthMaxSize];
struct rudpHeader *head;
int fullSize = size + sizeof(*head);
int i, err = 0, maxRetry = ru->maxRetries;


/* Make buffer with header in front of message. 
 * At some point we might replace this with a scatter/gather
 * iovector. */
ru->sendCount += 1;
ru->resend = FALSE;
assert(size <= rudpMaxSize);
head = (struct rudpHeader *)outBuf;
memcpy(head+1, message, size);
head->id = ++ru->lastId;
head->type = rudpData;

/* Go into send/wait for ack/retry loop. */
for (i=0; i<maxRetry; ++i)
    {
    gettimeofday(&sendTv, NULL);
    head->sendSec = sendTv.tv_sec;
    head->sendMicro = sendTv.tv_usec;
    err =  sendto(ru->socket, outBuf, fullSize, 0, 
	(struct sockaddr *)sai, sizeof(*sai));
    if (err < 0) 
	{
	/* Warn, wait, and retry. */
	struct timeval tv;
	warn("sendto problem %s", strerror(errno));
	tv.tv_sec = 0;
	tv.tv_usec = ru->timeOut;
	select(0, NULL, NULL, NULL, &tv);
	rudpTimedOut(ru);
	ru->resendCount += 1;
	ru->resend = TRUE;
	continue;
	}
    if (getOurAck(ru, &sendTv, sai))
	{
	return 0;
	}
    rudpTimedOut(ru);
    ru->resendCount += 1;
    ru->resend = TRUE;
    }
if (err >= 0)
    {
    err = ETIMEDOUT;
    warn("rudpSend timed out");
    }
ru->failCount += 1;
return err;
}


int rudpReceiveTimeOut(struct rudp *ru, void *messageBuf, int bufSize, 
	struct sockaddr_in *retFrom, int timeOut)
/* Read message into buffer of given size.  Returns actual size read on
 * success. On failure prints a warning, sets errno, and returns -1. 
 * Also returns ip address of message source. If timeOut is nonzero,
 * it represents the timeout in milliseconds.  It will set errno to
 * ETIMEDOUT in this case.*/
{
char inBuf[udpEthMaxSize];
struct rudpHeader *head = (struct rudpHeader *)inBuf;
struct rudpHeader ackHead;
struct sockaddr_in sai;
socklen_t saiSize = sizeof(sai);
int readSize, err;
assert(bufSize <= rudpMaxSize);
ru->receiveCount += 1;
for (;;)
    {
    if (timeOut != 0)
	{
	if (!readReadyWait(ru->socket, timeOut))
	    {
	    warn("rudpReceive timed out\n");
	    errno = ETIMEDOUT;
	    return -1;
	    }
	}
    readSize = recvfrom(ru->socket, inBuf, sizeof(inBuf), 0, 
	(struct sockaddr*)&sai, &saiSize);
    if (retFrom != NULL)
	*retFrom = sai;
    if (readSize < 0)
	{
	if (errno == EINTR)
	    continue;
	warn("recvfrom error: %s", strerror(errno));
	ru->failCount += 1;
	return readSize;
	}
    if (readSize < sizeof(*head))
	{
	warn("rudpRecieve truncated message");
	continue;
	}
    if (head->type != rudpData)
	{
	if (head->type != rudpAck)
	    warn("skipping non-data message %d in rudpReceive", head->type);
	continue;
	}
    ackHead = *head;
    ackHead.type = rudpAck;
    err = sendto(ru->socket, &ackHead, sizeof(ackHead), 0, 
	(struct sockaddr *)&sai, sizeof(sai));
    if (err < 0)
	{
	warn("problem sending ack in rudpRecieve: %s", strerror(errno));
	}
    readSize -= sizeof(*head);
    if (readSize > bufSize)
	{
	warn("read more bytes than have room for in rudpReceive");
	readSize = bufSize;
	}
    memcpy(messageBuf, head+1, readSize);
    ru->lastIdReceived = head->id;
    break;
    }
return readSize;
}

int rudpReceiveFrom(struct rudp *ru, void *messageBuf, int bufSize, 
	struct sockaddr_in *retFrom)
/* Read message into buffer of given size.  Returns actual size read on
 * success. On failure prints a warning, sets errno, and returns -1. 
 * Also returns ip address of message source. */
{
return rudpReceiveTimeOut(ru, messageBuf, bufSize, retFrom, 0);
}

int rudpReceive(struct rudp *ru, void *messageBuf, int bufSize)
/* Read message into buffer of given size.  Returns actual size read on
 * success. On failure prints a warning, sets errno, and returns -1. */
{
return rudpReceiveFrom(ru, messageBuf, bufSize, NULL);
}

void rudpPrintStatus(struct rudp *ru)
/* Print out rudpStatus */
{
printf("rudp status:\n");
printf("  receiveCount %d\n", ru->receiveCount);
printf("  sendCount %d\n", ru->sendCount);
printf("  resendCount %d\n", ru->resendCount);
printf("  failCount %d\n", ru->failCount);
printf("  timeOut %d\n", ru->timeOut);
printf("  rttVary %d\n", ru->rttVary);
printf("  rttAve %d\n", ru->rttAve);
printf("  rttLast %d\n", ru->rttLast);
}

void rudpTest()
/* Test out rudp stuff. */
{
static int times[] = {1000, 200, 200, 100, 200, 200, 200, 400, 200, 200, 200, 200, 1000, 
	200, 200, 200, 200};
struct rudp *ru = rudpNew(0);
int i;

for (i=0; i<ArraySize(times); ++i)
    {
    int oldTimeOut = ru->timeOut;
    rudpAddRoundTripTime(ru, times[i]);
    printf("%d\t%d\t%d\t%d\n", i, oldTimeOut, times[i], ru->timeOut);
    }
}
