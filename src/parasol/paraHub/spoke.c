/* A spoke - a process that initiates connections with remote hosts.
 * Parasol sends messages through spokes so as not to get bogged down
 * waiting on a remote host.  Parasol recieves all messages through it's
 * central socket though. */

#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include "common.h"
#include "dlist.h"
#include "dystring.h"
#include "linefile.h"
#include "net.h"
#include "internet.h"
#include "paraHub.h"
#include "portable.h"

int spokeLastId;	/* Id of last spoke allocated. */
static char *runCmd = "run";

static void notifyNodeDown(char *machine, char *job)
/* Notify hub that a node is down. */
{
struct paraMessage *pm = pmNew(0,0);
pmPrintf(pm, "nodeDown %s %s", machine, job);
hubMessagePut(pm);
}

static void sendRecycleMessage(char *spokeName)
/* Tell hub spoke is free. */
{
struct paraMessage *pm = pmNew(0,0);
pmPrintf(pm, "recycleSpoke %s", spokeName);
hubMessagePut(pm);
}

static void spokeSendRemote(char *spokeName, char *machine, char *message)
/* Send message to remote machine. */
{
boolean ok;
struct paraMessage pm;
struct rudp *ru = rudpOpen();

if (ru != NULL)
    {
    pmInitFromName(&pm, machine, paraNodePort);
    pmSet(&pm, message);
    ok = pmSend(&pm, ru);
    if (!ok)
	{
	char *command = nextWord(&message);
	char *job = NULL;
	if (sameString(runCmd, command))
	    {
	    char *hub = nextWord(&message);
	    job = nextWord(&message);
	    if (job != 0)
		notifyNodeDown(machine, job);
	    }
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
int conn, fromLen;
char *line, *machine;
struct paraMessage *message = NULL;

/* Wait on message and process it. */
for (;;)
    {
    /* Wait for next message. */
    mutexLock(&spoke->messageMutex);
    while (spoke->message == NULL)
	condWait(&spoke->messageReady, &spoke->messageMutex);
    message = spoke->message;
    spoke->message = NULL;
    mutexUnlock(&spoke->messageMutex);

    line = message->data;
    logIt("%s: %s\n", spoke->name, line);
    machine = nextWord(&line);
    if (machine != NULL)
	{
	if (line != NULL && line[0] != 0)
	    spokeSendRemote(spoke->name, machine, line);
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

static void spokeSyncSendMessage(struct spoke *spoke, struct paraMessage *pm)
/* Synchronize with spoke and update it's message pointer. */
{
mutexLock(&spoke->messageMutex);
if (spoke->message != NULL)
    warn("Sending message to %s, which is occupied", spoke->name);
spoke->message = pm;
condSignal(&spoke->messageReady);
mutexUnlock(&spoke->messageMutex);
}

void spokeSendMessage(struct spoke *spoke, struct machine *machine, char *message)
/* Send a generic message to machine through spoke. */
{
struct paraMessage *pm = pmNew(0,0);
freez(&spoke->machine);
spoke->machine = cloneString(machine->name);
pmPrintf(pm, "%s %s", spoke->machine, message);
spokeSyncSendMessage(spoke, pm);
}


void spokeSendJob(struct spoke *spoke, struct machine *machine, struct job *job)
/* Tell spoke to start up a job. */
{
struct paraMessage *pm = pmNew(0,0);
char *reserved = "0";	/* An extra parameter to fill in some day */
char err[512];

fillInErrFile(err, job->id, machine->tempDir);
freez(&job->err);
job->err = cloneString(err);
freez(&spoke->machine);
spoke->machine = cloneString(machine->name);
pmPrintf(pm, "%s %s", machine->name, runCmd);
pmPrintf(pm, " %s", hubHost);
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
