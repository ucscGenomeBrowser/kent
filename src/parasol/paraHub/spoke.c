/* A spoke - a process that initiates connections with remote hosts.
 * Parasol sends messages through spokes so as not to get bogged down
 * waiting on a remote host.  Parasol recieves all messages through it's
 * central socket though. */

#include "paraCommon.h"
#include "dlist.h"
#include "dystring.h"
#include "linefile.h"
#include "net.h"
#include "internet.h"
#include "portable.h"
#include "pthreadWrap.h"
#include "paraHub.h"
#include "log.h"

int spokeLastId;	/* Id of last spoke allocated. */
static char *runCmd = "run";

static void notifyNodeDown(char *machine)
/* Notify hub that a node is down. */
{
struct paraMessage *pm = pmNew(0,0);
pmPrintf(pm, "nodeDown %s", machine);
hubMessagePut(pm);
}

static void sendRecycleMessage(char *spokeName)
/* Tell hub spoke is free. */
{
struct paraMessage *pm = pmNew(0,0);
pmPrintf(pm, "recycleSpoke %s", spokeName);
hubMessagePut(pm);
}

static void spokeSendRemote(char *spokeName, char *machine, char *dottedQuad, char *message)
/* Send message to remote machine. */
{
boolean ok;
struct paraMessage pm;
struct rudp *ru = rudpOpen();
bits32 ip;

if (ru != NULL)
    {
    internetDottedQuadToIp(dottedQuad, &ip);
    if (ip == 0)
	pmInitFromName(&pm, machine, paraNodePort);
    else
        pmInit(&pm, ip, paraNodePort);
    pmSet(&pm, message);
    ok = pmSend(&pm, ru);
    if (!ok)
	{
	char *command = nextWord(&message);
	if (sameString(runCmd, command))
	    notifyNodeDown(machine);
	}
    rudpClose(&ru);
    }

/* Tell hub spoke is free. */
sendRecycleMessage(spokeName);
}

static void *spokeProcess(void *vptr)
/* Loop around forever listening to socket and forwarding messages to machines. */
{
struct spoke *spoke = vptr;
char *line, *machine, *dottedQuad;
struct paraMessage *message = NULL;

/* Wait on message and process it. */
for (;;)
    {
    /* Wait for next message. */
    pthreadMutexLock(&spoke->messageMutex);
    while (spoke->message == NULL)
	pthreadCondWait(&spoke->messageReady, &spoke->messageMutex);
    message = spoke->message;
    spoke->message = NULL;
    pthreadMutexUnlock(&spoke->messageMutex);

    line = message->data;
    logDebug("%s: %s", spoke->name, line);
    machine = nextWord(&line);
    dottedQuad = nextWord(&line);
    if (dottedQuad != NULL)
	{
	if (line != NULL && line[0] != 0)
	    spokeSendRemote(spoke->name, machine, dottedQuad, line);
	}
    pmFree(&message);
    }
return NULL;
}

struct spoke *spokeNew()
/* Get a new spoke.  This will create a thread for the spoke and
 * initialize the message synchronization stuff. */
{
struct spoke *spoke;
char spokeName[64];
int err;

sprintf(spokeName, "spoke_%03d", ++spokeLastId);
AllocVar(spoke);
AllocVar(spoke->node);
spoke->node->val = spoke;
spoke->name = cloneString(spokeName);
if ((err = pthread_mutex_init(&spoke->messageMutex, NULL)) != 0)
    errAbort("Couldn't create mutex for %s", spoke->name);
if ((err = pthread_cond_init(&spoke->messageReady, NULL)) != 0)
    errAbort("Couldn't create cond for %s", spoke->name);
err = pthread_create(&spoke->thread, NULL, spokeProcess, spoke);
if (err < 0)
    errAbort("Couldn't create %s %s", spoke->name, strerror(err));
return spoke;
}

void spokeFree(struct spoke **pSpoke)
/* Free spoke.  Close socket and kill process associated with it. */
{
/* Since this is multithreaded and threads may still be using
 * this, just defer to the big cleanup on exit. */
*pSpoke = NULL;
}

static struct paraMessage *messageWithHeader(struct machine *machine)
/* Return a message with machine name filled in. */
{
struct paraMessage *pm = pmNew(0,0);
char dottedQuad[17];
internetIpToDottedQuad(machine->ip, dottedQuad);
pmPrintf(pm, "%s %s ", machine->name, dottedQuad);
return pm;
}

static void spokeSyncSendMessage(struct spoke *spoke, struct paraMessage *pm)
/* Synchronize with spoke and update it's message pointer. */
{
pthreadMutexLock(&spoke->messageMutex);
if (spoke->message != NULL)
    warn("Sending message to %s, which is occupied", spoke->name);
spoke->message = pm;
pthreadCondSignal(&spoke->messageReady);
pthreadMutexUnlock(&spoke->messageMutex);
}

void spokeSendMessage(struct spoke *spoke, struct machine *machine, char *message)
/* Send a generic message to machine through spoke. */
{
struct paraMessage *pm = messageWithHeader(machine);
pmPrintf(pm, "%s", message);
spokeSyncSendMessage(spoke, pm);
}

void spokeSendJob(struct spoke *spoke, struct machine *machine, struct job *job)
/* Tell spoke to start up a job. */
{
struct paraMessage *pm = messageWithHeader(machine);
char *reserved = "0";	/* An extra parameter to fill in some day */
char err[512];

fillInErrFile(err, job->id, machine->tempDir);
freez(&job->err);
job->err = cloneString(err);
pmPrintf(pm, "%s", runCmd);
pmPrintf(pm, " %d", job->id);
pmPrintf(pm, " %s", reserved);
pmPrintf(pm, " %s", job->batch->user->name);
pmPrintf(pm, " %s", job->dir);
pmPrintf(pm, " %s", job->in);
pmPrintf(pm, " %s", job->out);
pmPrintf(pm, " %s", job->err);
pmPrintf(pm, " %s", job->cmd);
spokeSyncSendMessage(spoke, pm);
}
