/* rudp - (semi) reliable UDP communication.  UDP is a packet based 
 * rather than stream based internet communication protocol.  
 * Messages sent by UDP are checked for integrety by the UDP layer, and 
 * discarded if transmission errors are detected.  However packets are
 * not necessarily received in the same order that they are sent,
 * and packets may be duplicated or lost. 
 *
 * For many, perhaps most applications, TCP/IP is a saner choice
 * than UDP.  If the communication channel is between just two
 * computers you can pretty much just treat TCP/IP as a fairly
 * reliable pipe.   However if the communication involves many
 * computers sometimes UDP can be a better choice.  It is possible to
 * do broadcast and multicast with UDP but not with TCP/IP.  Also
 * for systems like parasol, where thousand of computers may be
 * rapidly making and breaking connections a server,  TCP paradoxically
 * can end up less reliable than UDP.  Though TCP is relatively robust
 * when a connection is made,  it can it turns out relatively easily
 * fail to make a connection in the first place, and spend quite a long
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
 * by W. Richard Stevens, may he rest in peace. */

#include "common.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "rudp.h"

static int rudpCalcTimeOut(struct rudp *ru)
/* Return calculation of time out based on current data. */
{
int timeOut = ru->rttAve + (ru->rttVary<<2);
if (timeOut > 999999) timeOut = 999999;	/* No more than a second. */
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
}

struct rudp *rudpNew(int socket)
/* Wrap a rudp around a socket. */
{
struct rudp *ru;
assert(socket >= 0);
AllocVar(ru);
ru->socket = socket;
ru->rttVary = 250;	/* Initial variance 250 microseconds. */
ru->timeOut = rudpCalcTimeOut(ru);
return ru;
}

void rudpFree(struct rudp **pRu)
/* Free up rudp.  Note this does *not* close the associated socket. */
{
freez(pRu);
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
assert(microDiff >= 0);
return microDiff;
}

static boolean getOurAck(struct rudp *ru, struct timeval *startTv)
/* Wait for acknowledgement to the message we just sent.
 * The set should be zeroed out. Only wait for up to
 * ru->timeOut microseconds.   Prints a message and returns FALSE
 * if there's a problem.   */
{
struct rudpHeader head;
int readyCount, readSize;
int timeOut = ru->timeOut;
fd_set set;

for (;;)
    {
    /* Set up select with our time out. */
    struct sockaddr_in sai;
    int saiSize = sizeof(sai);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeOut;
    FD_ZERO(&set);
    FD_SET(ru->socket, &set);
    readyCount = select(ru->socket+1, &set, NULL, NULL, &tv);
    if (readyCount == EINTR)	/* Select interrupted, not timed out. */
	continue;
    if (readyCount == 0)
	return FALSE;		/* Timed out.  We return unsuccessfully. */

    /* Read message and if it's our ack return true.   */
    if (FD_ISSET(ru->socket, &set))
	{
	readSize = recvfrom(ru->socket, &head, sizeof(head), 0, NULL, NULL);
	if (readSize >= sizeof(head) && head.type == rudpAck && head.id == ru->lastId)
	    {
	    gettimeofday(&tv, NULL);
	    rudpAddRoundTripTime(ru, timeDiff(startTv, &tv));
	    return TRUE;
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

int rudpSend(struct rudp *ru, rudpHost host, bits16 port, void *message, int size)
/* Send message of given size to port at host via rudp.  Prints a warning and
 * sets errno and returns -1 if there's a problem. */
{
struct sockaddr_in sai;	/* Internet address. */
struct timeval sendTv;	/* Current time. */

char outBuf[udpEthMaxSize];
struct rudpHeader *head;
int fullSize = size + sizeof(*head);
int i, err = 0, maxRetry = 3;

/* Make buffer with header in front of message. 
 * At some point we might replace this with a scatter/gather
 * iovector. */
ru->sendCount += 1;
assert(size <= rudpMaxSize);
head = (struct rudpHeader *)outBuf;
memcpy(head+1, message, size);
head->id = ++ru->lastId;
head->type = rudpData;

/* Make up internet address for destination. */
ZeroVar(&sai);
sai.sin_addr.s_addr = htonl(host);
sai.sin_family = AF_INET;
sai.sin_port = htons(port);

/* Go into send/wait for ack/retry loop. */
for (i=0; i<maxRetry; ++i)
    {
    gettimeofday(&sendTv, NULL);
    head->sendSec = sendTv.tv_sec;
    head->sendMicro = sendTv.tv_usec;
    err =  sendto(ru->socket, outBuf, fullSize, 0, 
	(struct sockaddr *)&sai, sizeof(sai));
    if (err < 0) 
	{
	/* Warn, wait, and retry. */
	struct timeval tv;
	warn(" sendto problem on host %d:  %s", host, strerror(errno));
	tv.tv_sec = 0;
	tv.tv_usec = ru->timeOut;
	select(0, NULL, NULL, NULL, &tv);
	ru->resendCount += 1;
	rudpTimedOut(ru);
	continue;
	}
    if (getOurAck(ru, &sendTv))
	{
	return 0;
	}
    rudpTimedOut(ru);
    ru->resendCount += 1;
    }
if (err == 0)
    err = ETIMEDOUT;
ru->failCount += 1;
return err;
}


int rudpReceive(struct rudp *ru, void *messageBuf, int bufSize)
/* Read message into buffer of given size.  Returns actual size read on
 * success. On failure prints a warning, sets errno, and returns -1. */
{
char inBuf[udpEthMaxSize];
struct rudpHeader *head = (struct rudpHeader *)inBuf;
struct rudpHeader ackHead;
struct sockaddr_in sai;
int readSize, err;
assert(bufSize <= rudpMaxSize);
ru->receiveCount += 1;
for (;;)
    {
    int saiSize = sizeof(sai);
    readSize = recvfrom(ru->socket, inBuf, sizeof(inBuf), 0, 
	(struct sockaddr*)&sai, &saiSize);
    if (readSize == EINTR)
	continue;
    if (readSize < 0)
	{
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
	warn("skipping non-data message in rudpReceive");
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
    break;
    }
return readSize;
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
