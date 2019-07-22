/* paraMessage - routines to pack and unpack messages in
 * the parasol system, and also to send them via sockets. */

#ifndef PARAMESSAGE_H
#define PARAMESSAGE_H

#ifndef RUDP_H
#include "rudp.h"
#endif


struct paraMessage
/* A parasol message. */
    {
    struct paraMessage *next;	/* Next in list. */
    struct sockaddr_storage ipAddress;	/* IP address of machine message is from/to */
    int size;			/* Size of data. */
    char data[rudpMaxSize+1];	/* Data. Extra byte for zero tag at end. */
    };

struct paraMultiMessage
/* A parasol multi-packet response message. */
    {
    struct paraMessage *pm;
    struct sockaddr_storage ipAddress;  /* IP address of machine message is from/to */
    bits32 id;                     /* Message id.  Returned with ack. */
    };

void pmInit(struct paraMessage *pm, char *ipStr, char *portStr);
/* Initialize message (that might be on stack).  */

void pmInitFromName(struct paraMessage *pm, char *hostName, char *portStr);
/* Initialize message with ascii ip address. */

struct paraMessage *pmNew(char *ipStr, char *portStr);
/* Create new message in memory.   */

struct paraMessage *pmNewFromName(char *hostName, bits16 port);
/* Create new message in memory */

void pmFree(struct paraMessage **pPm);
/* Free up message. */

void pmClear(struct paraMessage *pm);
/* Clear out data buffer. */

void pmSet(struct paraMessage *pm, char *message);
/* Set message in data buffer. Aborts if message too long (rudpMaxSize or more). */

void pmPrintf(struct paraMessage *pm, char *format, ...)
/* Print message into end of data buffer.  Warn if it goes
 * past limit. */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
;


boolean pmSend(struct paraMessage *pm, struct rudp *ru);
/* Send out message.  Print warning message and return FALSE if
 * there is a problem. */

boolean pmSendString(struct paraMessage *pm, struct rudp *ru, char *string);
/* Send out given message strng.  Print warning message and return FALSE if
 * there is a problem. */

void pmCheckCommandSize(char *string, int len);
/* Check that string of given len is not too long to fit into paraMessage.
 * If it is, abort with good error message assuming it was a command string */

boolean pmReceive(struct paraMessage *pm, struct rudp *ru);
/* Receive message.  Print warning message and return FALSE if
 * there is a problem. */

boolean pmReceiveTimeOut(struct paraMessage *pm, struct rudp *ru, int timeOut);
/* Wait up to timeOut microseconds for message.  To wait forever
 * set timeOut to zero. */

void pmmInit(struct paraMultiMessage *pmm, struct paraMessage *pm);
/* Initialize structure for multi-message response  */

boolean pmmReceiveTimeOut(struct paraMultiMessage *pmm, struct rudp *ru, int timeOut);
/* Multi-message receive
 * Wait up to timeOut microseconds for message.  To wait forever
 * set timeOut to zero.  For multi-message response
 * We know the ip, and can track the port for continuity
 * and the packet id for sequential series. 
 */

boolean pmmReceive(struct paraMultiMessage *pmm, struct rudp *ru);
/* Receive multi message.  Print warning message and return FALSE if
 * there is a problem. */

void pmFetchOpenFile(struct paraMessage *pm, struct rudp *ru, char *fileName);
/* Read everything you can from socket and output to file. */

void pmFetchFile(char *host, char *sourceName, char *destName);
/* Fetch small file. */

boolean pmSendStringWithRetries(struct paraMessage *pm, struct rudp *ru, char *string);
/* Send out given message strng.  Print warning message and return FALSE if
 * there is a problem. Try up to 5 times sleeping for 60 seconds in between.
 * This is an attempt to help automated processes. */

char *pmHubSendSimple(char *message, char *host);
/* Send message to host, no response. */

char *pmHubSingleLineQuery(char *query, char *host);
/* Send message to hub and get single line response.
 * This should be freeMem'd when done. */

struct slName *pmHubMultilineQuery(char *query, char *host);
/* Send a command with a multiline response to hub,
 * and return response as a list of strings. */

struct paraPstat2Job
/* The job information returned by a pstat2 message by parasol,
 * parsed out. */
    {
    struct paraPstat2Job *next;
    char *status;   // 'r' mostly
    char *parasolId; // Parasol ID as a string
    char *user;	    // Name of user
    char *program;  // Name of program being run
    char *host;	    // Host name of node running job.
    };
#define PARAPSTAT2JOB_NUM_COLS  5

struct paraPstat2Job *paraPstat2JobLoad(char **row);
/* Turn an array of 5 strings into a paraPstat2Job. */

#endif /* PARAMESSAGE_H */
