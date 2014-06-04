/* paraMessage - routines to pack and unpack messages in
 * the parasol system, and also to send them via sockets. */

#include "paraCommon.h"
#include "paraLib.h"
#include "internet.h"
#include "rudp.h"
#include "paraMessage.h"
#include "errAbort.h"
#include "log.h"

void pmInit(struct paraMessage *pm, rudpHost ipAddress, bits16 port)
/* Initialize message (that might be on stack). ipAddress is in host
 * order. */
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
    warn("Message too long in pmSet, ignoring: %.20s...", message);
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

void pmCheckCommandSize(char *string, int len)
/* Check that string of given len is not too long to fit into paraMessage.
 * If it is, abort with good error message assuming it was a command string */
{
if (len > rudpMaxSize)
    {
    errAbort("The following string has %d bytes, but can only be %d:\n%s\n"
             "Please either shorten the current directory or the command line\n"
             "possibly by making a shell script that encapsulates a long command.\n"
             ,  len, (int)rudpMaxSize, string);
    }
}

boolean pmReceiveTimeOut(struct paraMessage *pm, struct rudp *ru, int timeOut)
/* Wait up to timeOut microseconds for message.  To wait forever
 * set timeOut to zero. */
{
int size = rudpReceiveTimeOut(ru, pm->data, sizeof(pm->data)-1, &pm->ipAddress, timeOut);
if (size < 0)
    {
    pmClear(pm);
    return FALSE;
    }
pm->size = size;
pm->data[size] = 0;
return TRUE;
}

boolean pmReceive(struct paraMessage *pm, struct rudp *ru)
/* Receive message.  Print warning message and return FALSE if
 * there is a problem. */
{
return pmReceiveTimeOut(pm, ru, 0);
}

void pmmInit(struct paraMultiMessage *pmm, struct paraMessage *pm, struct in_addr sin_addr)
/* Initialize structure for multi-message response  */
{
pmm->pm = pm;
pmm->ipAddress.sin_addr.s_addr = sin_addr.s_addr;
pmm->ipAddress.sin_port = 0;    /* we don't yet know what port the responder is going to use */
pmm->id = 0; 
}

boolean pmmReceiveTimeOut(struct paraMultiMessage *pmm, struct rudp *ru, int timeOut)
/* Multi-message receive
 * Wait up to timeOut microseconds for message.  To wait forever
 * set timeOut to zero.  For multi-message response
 * We know the ip, and can track the port for continuity
 * and the packet id for sequential series. 
 */
{
for(;;)
    {
    if (!pmReceiveTimeOut(pmm->pm, ru, timeOut))
	return FALSE;
    if (pmm->ipAddress.sin_addr.s_addr != pmm->pm->ipAddress.sin_addr.s_addr)
	{
	logDebug("rudp: pmmReceiveTimeOut ignoring unwanted packet from wrong sender expected %ux got %ux",
	    (unsigned int)pmm->ipAddress.sin_addr.s_addr, (unsigned int)pmm->pm->ipAddress.sin_addr.s_addr);
	continue;
	}
    if (pmm->ipAddress.sin_port == 0)  /* we don't yet know what port the responder is going to use */
       	/* Should be ASSERTABLE first packet received since pmmInit was called */
	{
	pmm->ipAddress.sin_port = pmm->pm->ipAddress.sin_port;
	}
    else
	{
	if (pmm->ipAddress.sin_port != pmm->pm->ipAddress.sin_port)
	    {
	    logDebug("rudp: pmmReceiveTimeOut ignoring unwanted packet from wrong port expected %x got %x",
		pmm->ipAddress.sin_port, pmm->pm->ipAddress.sin_port);
	    continue;
	    }
	if (pmm->id == ru->lastIdReceived && ru->resend)
	    {
	    logDebug("rudp: pmmReceiveTimeOut ignoring duplicate packet lastIdReceived=%d",	pmm->id);
	    continue;
	    }
	if (pmm->id+1 != ru->lastIdReceived)
	    {
	    logDebug("rudp: pmmReceiveTimeOut invalid msg id expected %d got %d", pmm->id+1, ru->lastIdReceived);
	    continue;
	    }
	}
    pmm->id = ru->lastIdReceived;
    return TRUE;
    }
}

boolean pmmReceive(struct paraMultiMessage *pmm, struct rudp *ru)
/* Receive multi message.  Print warning message and return FALSE if
 * there is a problem. */
{
return pmmReceiveTimeOut(pmm, ru, 0);
}


#if 0 /* unused static functions */
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
#endif

void pmFetchOpenFile(struct paraMessage *pm, struct rudp *ru, char *fileName)
/* Read everything you can from socket and output to file. */
{
struct paraMultiMessage pmm;
FILE *f = mustOpen(fileName, "w");
/* ensure the multi-message response comes from the correct ip and has no duplicate msgs*/
pmmInit(&pmm, pm, pm->ipAddress.sin_addr);
while (pmmReceive(&pmm, ru))
    {
    if (pm->size == 0)
	break;
    mustWrite(f, pm->data, pm->size);
    }
carefulClose(&f);
}

void pmFetchFile(char *host, char *sourceName, char *destName)
/* Fetch small file. Only works if you are on hub if they've set up any security. */
{
struct rudp *ru = rudpOpen();
struct paraMessage pm;
if (ru != NULL)
    {
    pmInitFromName(&pm, host, paraNodePort);
    pmPrintf(&pm, "fetch %s %s", getUser(), sourceName);
    if (pmSend(&pm, ru))
	pmFetchOpenFile(&pm, ru, destName);
    rudpClose(&ru);
    }
}

boolean pmSendStringWithRetries(struct paraMessage *pm, struct rudp *ru, char *string)
/* Send out given message strng.  Print warning message and return FALSE if
 * there is a problem. Try up to 5 times sleeping for 60 seconds in between.
 * This is an attempt to help automated processes. */
{
int tries = 0;
#define PMSENDSLEEP 60
#define PMSENDMAXTRIES 5
boolean result = FALSE;
while (TRUE)
    {
    result = pmSendString(pm, ru, string);
    if (result)
	break;
    warn("pmSendString timed out!");
    ++tries;
    if (tries >= PMSENDMAXTRIES)
	break;
    warn("pmSendString: will sleep %d seconds and retry", PMSENDSLEEP);
    sleep(PMSENDSLEEP);
    }
return result;
}

char *pmHubSendSimple(char *message, char *host)
/* Send message to host, no response. */
{
struct rudp *ru = rudpMustOpen();
struct paraMessage pm;
pmInitFromName(&pm, host, paraHubPort);
if (!pmSendStringWithRetries(&pm, ru, message))
    noWarnAbort();
rudpClose(&ru);
return cloneString(pm.data);
}

char *pmHubSingleLineQuery(char *query, char *host)
/* Send message to hub and get single line response.
 * This should be freeMem'd when done. */
{
struct rudp *ru = rudpMustOpen();
struct paraMessage pm;

pmInitFromName(&pm, host, paraHubPort);
if (!pmSendStringWithRetries(&pm, ru, query))
    noWarnAbort();
if (!pmReceive(&pm, ru))
    noWarnAbort();
rudpClose(&ru);
return cloneString(pm.data);
}

struct slName *pmHubMultilineQuery(char *query, char *host)
/* Send a command with a multiline response to hub,
 * and return response as a list of strings. */
{
struct slName *list = NULL;
struct rudp *ru = rudpMustOpen();
struct paraMessage pm;
struct paraMultiMessage pmm;
char *row[256];
int count = 0;
pmInitFromName(&pm, host, paraHubPort);
/* ensure the multi-message response comes from the correct ip and has no duplicate msgs*/
pmmInit(&pmm, &pm, pm.ipAddress.sin_addr);
if (!pmSendStringWithRetries(&pm, ru, query))
    noWarnAbort();
for (;;)
    {
    if (!pmmReceive(&pmm, ru))
	break;
    if (pm.size == 0)
	break;
    count = chopByChar(pm.data, '\n', row, sizeof(row));
    if (count > 1) --count;  /* for multiline, count is inflated by one */

    int i;
    for(i=0;i<count;++i)
	{
	slNameAddHead(&list, row[i]);
	}
    }
rudpClose(&ru);
slReverse(&list);
return list;
}

struct paraPstat2Job *paraPstat2JobLoad(char **row)
/* Turn an array of 5 strings into a paraPstat2Job. */
{
struct paraPstat2Job *job;
AllocVar(job);
job->status = cloneString(row[0]);
job->parasolId = cloneString(row[1]);
job->user = cloneString(row[2]);
job->program = cloneString(row[3]);
job->host = cloneString(row[4]);
return job;
}

