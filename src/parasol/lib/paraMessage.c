/* paraMessage - routines to pack and unpack messages in
 * the parasol system, and also to send them via sockets. */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "common.h"
#include "paraLib.h"
#include "internet.h"
#include "rudp.h"
#include "paraMessage.h"

void pmInit(struct paraMessage *pm, rudpHost ipAddress, bits16 port)
/* Initialize message (that might be on stack). */
{
ZeroVar(pm);
pm->ipAddress.sin_family = AF_INET;
pm->ipAddress.sin_port = htons(port);
pm->ipAddress.sin_addr.s_addr = htonl(ipAddress);
}

void pmInitFromName(struct paraMessage *pm, char *hostName, bits16 port)
/* Initialize message with ascii ip address. */
{
pmInit(pm, internetHostIp(hostName), port);
}


struct paraMessage *pmNew(rudpHost ipAddress, bits16 port)
/* Create new message in memory */
{
struct paraMessage *pm;
AllocVar(pm);
pmInit(pm, ipAddress, port);
return pm;
}

struct paraMessage *pmNewFromName(char *hostName, bits16 port)
/* Create new message in memory */
{
return pmNew(internetHostIp(hostName), port);
}

void pmFree(struct paraMessage **pPm)
/* Free up message. */
{
freez(pPm);
}

void pmClear(struct paraMessage *pm)
/* Clear out data buffer. */
{
pm->size = 0;
}

void pmSet(struct paraMessage *pm, char *message)
/* Set message in data buffer. */
{
int len = strlen(message);
if (len >= sizeof(pm->data) - 1)
    {
    warn("Message too long in pmSet, ignoring", message);
    pmClear(pm);
    }
else
    {
    strcpy(pm->data, message);
    pm->size = len;
    }
}

void pmVaPrintf(struct paraMessage *pm, char *format, va_list args)
/* Print message into end of data buffer.  Warn if it goes
 * past limit. */
{
int sizeLeft = sizeof(pm->data) - pm->size;
int sz = vsnprintf(pm->data + pm->size, sizeLeft, format, args);
/* note that some version return -1 if too small */
if ((sz < 0) || (sz >= sizeLeft))
    {
    warn("pmVaPrintf buffer overflow size %d, format %s", sz, format);
    pmClear(pm);
    }
else
    pm->size += sz;
}

void pmPrintf(struct paraMessage *pm, char *format, ...)
/* Print message into end of data buffer.  Warn if it goes
 * past limit. */
{
va_list args;
va_start(args, format);
pmVaPrintf(pm, format, args);
va_end(args);
}

boolean pmSend(struct paraMessage *pm, struct rudp *ru)
/* Send out message.  Print warning message and return FALSE if
 * there is a problem. */
{
return rudpSend(ru, &pm->ipAddress, pm->data, pm->size) == 0;
}

boolean pmSendString(struct paraMessage *pm, struct rudp *ru, char *string)
/* Send out given message strng.  Print warning message and return FALSE if
 * there is a problem. */
{
pmSet(pm, string);
return pmSend(pm, ru);
}

boolean pmReceive(struct paraMessage *pm, struct rudp *ru)
/* Receive message.  Print warning message and return FALSE if
 * there is a problem. */
{
int size = rudpReceiveFrom(ru, pm->data, sizeof(pm->data)-1, &pm->ipAddress);
if (size < 0)
    {
    pmClear(pm);
    return FALSE;
    }
pm->size = size;
pm->data[size] = 0;
return TRUE;
}

static char *addLongData(char *data, bits32 val)
/* Add val to data stream, converting to network format.  Return
 * new position of data stream. */
{
val = htonl(val);
memcpy(data, &val, sizeof(val));
return data + sizeof(val);
}

static bits32 getLongData(char **pData)
/* Get val from data stream, converting from network to host format.
 * Update data stream position. */
{
bits32 val;
memcpy(&val, *pData, sizeof(val));
*pData += sizeof(val);
val = ntohl(val);
return val;
}


