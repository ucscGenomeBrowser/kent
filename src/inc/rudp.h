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

#ifndef RUDP_H
#define RUDP_H

struct rudp
/* A UDP socket and a little bit of stuff to help keep track
 * of how often we should resend unacknowledged messages. */
    {
    int socket;		/* The associated UDP socket. */
    int rttLast;	/* Last round trip time (RTT) for a message/ack in microseconds. */
    int rttAve;		/* Approximate average of recent RTTs. */
    int rttVary;	/* Approximate variation of recent RTTs. */
    int timeOut;	/* Ideal timeout for next receive. */
    int resendCount;	/* Number of resends. */
    int failCount;	/* Number of failures. */
    bits32 lastId;	/* Id number of last message sent. */
    };

enum rudpType
    {
    rudpAck = 199,	/* Acknowledge message. */
    rudpData = 200,	/* Message with some data. */
    };

struct rudpHeader
/* The header to a rudp message.  */
    {
    bits32 id;		/* Message id.  Returned with ack. */
    bits32 sendSec;	/* Time sent in seconds.  Returned with ack. */
    bits32 sendMicro;	/* Time sent microseconds. Returned with ack. */
    bits8 type;		/* One of rudpType above. */
    bits8 reserved1;	/* Reserved, always zero for now. */
    bits8 reserved2;	/* Reserved, always zero for now. */
    bits8 reserved3;	/* Reserved, always zero for now. */
    };

typedef bits32 rudpHost;  /* The IP address (in host order) of another computer. */

#define udpEthMaxSize 1444
    /* Max data size that will fit into a single ethernet packet after UDP and IP
     * headers.  Things are faster & more reliable if you stay below this */

#define rudpMaxSize (udpEthMaxSize - sizeof(struct rudpHeader)  )

struct rudp *rudpNew(int socket);
/* Wrap a rudp around a socket. */

void rudpFree(struct rudp **pRu);
/* Free up rudp.  Note this does *not* close the associated socket. */

int rudpSend(struct rudp *ru, rudpHost host, bits16 port, void *message, int size);
/* Send message of given size to port at host via rudp.  Prints a warning and
 * sets errno and returns -1 if there's a problem. */

int rudpReceive(struct rudp *ru, void *messageBuf, int bufSize);
/* Read message into buffer of given size.  Returns actual size read on
 * success. On failure prints a warning, sets errno, and returns -1. */

void rudpTest();
/* Test out rudp stuff. */

#endif /* RUDP_H */
