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
    struct sockaddr_in ipAddress;	/* IP address of machine message is from/to */
    int size;			/* Size of data. */
    char data[rudpMaxSize+1];	/* Data. Extra byte for zero tag at end. */
    };

void pmInit(struct paraMessage *pm, rudpHost ipAddress, bits16 port);
/* Initialize message (that might be on stack). ipAddress is in host
 * order. */

void pmInitFromName(struct paraMessage *pm, char *hostName, bits16 port);
/* Initialize message with ascii ip address. */

struct paraMessage *pmNew(rudpHost ipAddress, bits16 port);
/* Create new message in memory.  ipAddress is in host order.  */

struct paraMessage *pmNewFromName(char *hostName, bits16 port);
/* Create new message in memory */

void pmFree(struct paraMessage **pPm);
/* Free up message. */

void pmClear(struct paraMessage *pm);
/* Clear out data buffer. */

void pmSet(struct paraMessage *pm, char *message);
/* Set message in data buffer. */

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

boolean pmReceive(struct paraMessage *pm, struct rudp *ru);
/* Receive message.  Print warning message and return FALSE if
 * there is a problem. */

boolean pmReceiveTimeOut(struct paraMessage *pm, struct rudp *ru, int timeOut);
/* Wait up to timeOut microseconds for message.  To wait forever
 * set timeOut to zero. */

#endif /* PARAMESSAGE_H */
