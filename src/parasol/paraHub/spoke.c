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
#include "paraLib.h"
#include "net.h"
#include "paraHub.h"

int spokeLastId;	/* Id of last spoke allocated. */
static char *runCmd = "run";

static void notifyNodeDown(char *machine, char *job)
/* Notify hub that a node is down. */
{
int hubFd = hubConnect();
char buf[512];
if (hubFd > 0)
    {
    sprintf(buf, "nodeDown %s %s", machine, job);
    netSendLongString(hubFd, buf);
    close(hubFd);
    }
}

static void sendRecycleMessage(char *spokeName)
/* Tell hub spoke is free. */
{
char buf[512];
int hubFd = hubConnect();
if (hubFd > 0)
    {
    sprintf(buf, "recycleSpoke %s", spokeName);
    netSendLongString(hubFd, buf);
    freeMem(netGetLongString(hubFd));
    close(hubFd);
    }
}

static void spokeSendRemote(char *spokeName, char *machine, char *message)
/* Send message to remote machine. */
{
int sd;
boolean ok = FALSE;

/* Try and tell machine about job. */
sd = netConnect(machine, paraPort);
if (sd > 0)
    {
    ok = sendWithSig(sd, message);
    close(sd);
    }
if (!ok)
    {
    char *command = nextWord(&message);
    char *job = NULL;
    if (sameString(runCmd, command) || sameString("check", command))
	{
	char *hub = nextWord(&message);
	job = nextWord(&message);
	}
    if (job == NULL)
        job = "0";
    notifyNodeDown(machine, job);
    }

/* Tell hub spoke is free. */
sendRecycleMessage(spokeName);
}

static void spokeProcess(char *socketName)
/* Loop around forever listening to socket and forwarding messages to machines. */
{
int conn, fromLen;
char *buf, *line, *machine, sig[20];
int sd;
struct sockaddr_un sa;
int sigLen = strlen(paraSig);

/* Do some precomputation on signature. */
assert(sigLen < sizeof(sig));
sig[sigLen] = 0;

/* Make your basic Unix socket. */
sd = socket(AF_UNIX, SOCK_STREAM, 0);
if (sd < 0)
    {
    warn("Couldn't create Unix socket");
    return;
    }

/* Bind it to file system. */
ZeroVar(&sa);
sa.sun_family = AF_UNIX;
strncpy(sa.sun_path, socketName, sizeof(sa.sun_path));
if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    {
    warn("Couldn't bind socket to %s", socketName);
    close(sd);
    return;
    }

/* Listen on socket until we get killed. */
listen(sd,3);
conn = accept(sd, NULL, &fromLen);
for (;;)
    {
    if (netReadAll(conn, sig, sigLen) != sigLen)
	continue;
    if (!sameString(sig, paraSig))
	continue;
    line = buf = netGetLongString(conn);
    // logIt("%s: %s\n", socketName, line);
    machine = nextWord(&line);
    if (machine != NULL)
	{
	if (sameWord(machine, "ping"))
	    sendRecycleMessage(socketName);
	else if (line != NULL && line[0] != 0)
	    spokeSendRemote(socketName, machine, line);
	}
    freez(&buf);
    }
}

struct spoke *spokeNew(int *closeList)
/* Get a new spoke.  Close list is an optional, -1 terminated array of file
 * handles to close. This will fork and create a process for spoke and a 
 * socket for communicating with it. */
{
struct spoke *spoke;
int childId;
int sd = 0;
char socketName[64];
int spokeId = ++spokeLastId;
struct sockaddr_un sa;

sprintf(socketName, "spoke_%03d", spokeId);
remove(socketName);

childId = fork();
if (childId < 0)
    {
    warn("Couldn't fork in spokeNew");
    close(sd);
    return NULL;
    }
else if (childId == 0)
    {
    int i;
    int fd;
    if (closeList != NULL)
        {
	for (i=0; ;++i)
	    {
	    fd = closeList[i];
	    if (fd < 0)
	        break;
	    close(fd);
	    }
	}
    close(sd);
    spokeProcess(socketName);
    return NULL;
    }
else
    {
    AllocVar(spoke);
    AllocVar(spoke->node);
    spoke->node->val = spoke;
    spoke->id = spokeLastId;
    spoke->socketName = cloneString(socketName);
    spoke->pid = childId;
    return spoke;
    }
}

void spokeFree(struct spoke **pSpoke)
/* Free spoke.  Close socket and kill process associated with it. */
{
struct spoke *spoke = *pSpoke;
if (spoke != NULL)
    {
    if (spoke->socket != 0)
	close(spoke->socket);
    if (spoke->pid != 0)
	kill(spoke->pid, SIGTERM);
    if (spoke->socketName != NULL)
	{
        remove(spoke->socketName);
	freeMem(spoke->socketName);
	}
    freeMem(spoke->machine);
    freez(pSpoke);
    }
}

static int spokeGetSocket(struct spoke *spoke)
/* Get socket for spoke, opening it if not already open. */
{
int sd = spoke->socket;
if (sd == 0)
    {
    struct sockaddr_un sa;
    /* Make your basic socket. */
    sd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sd < 0)
	{
        warn("couldn't open socket in spokeSendMessage");
	return -1;
	}

    /* Get connection to socket process via file system. */
    ZeroVar(&sa);
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, spoke->socketName, sizeof(sa.sun_path));
    if (connect(sd, (struct sockaddr*) &sa, sizeof(sa)) < 0)
        {
	warn("couldn't connect to socket in spokeSendMessage");
	close(sd);
	return -1;
	}
    spoke->socket = sd;
    }
return sd;
}

void spokePing(struct spoke *spoke)
/* Send ping message to spoke.  It should eventually respond
 * with recycleSpoke message to hub socked.  */
{
int sd = spokeGetSocket(spoke);
if (sd > 0)
    {
    write(sd, paraSig, strlen(paraSig));
    netSendLongString(sd, "ping");
    }
}

void spokeSendMessage(struct spoke *spoke, struct machine *machine, char *message)
/* Send a generic message to machine through spoke. */
{
struct dyString *dy = newDyString(1024);
int sd = spokeGetSocket(spoke);

if (sd > 0)
    {
    /* Send out signature, machine, and message to spoke. */
    freez(&spoke->machine);
    spoke->machine = cloneString(machine->name);
    dyStringPrintf(dy, "%s %s", spoke->machine, message);
    write(sd, paraSig, strlen(paraSig));
    netSendLongString(sd, dy->string);
    }
dyStringFree(&dy);
}

void spokeSendJob(struct spoke *spoke, struct machine *machine, struct job *job)
/* Tell spoke to start up a job. */
{
struct dyString *dy = newDyString(1024);
int sd = spokeGetSocket(spoke);
char *reserved = "0";	/* An extra parameter to fill in some day */

if (sd > 0)
    {
    char err[512];
    sprintf(err, "%s/para%d.err", machine->tempDir, job->id);
    freez(&job->err);
    job->err = cloneString(err);
    freez(&spoke->machine);
    spoke->machine = cloneString(machine->name);
    dyStringPrintf(dy, "%s %s", machine->name, runCmd);
    dyStringPrintf(dy, " %s", hubHost);
    dyStringPrintf(dy, " %d", job->id);
    dyStringPrintf(dy, " %s", reserved);
    dyStringPrintf(dy, " %s", job->batch->user->name);
    dyStringPrintf(dy, " %s", job->dir);
    dyStringPrintf(dy, " %s", job->in);
    dyStringPrintf(dy, " %s", job->out);
    dyStringPrintf(dy, " %s", job->err);
    dyStringPrintf(dy, " %s", job->cmd);
    write(sd, paraSig, strlen(paraSig));
    netSendLongString(sd, dy->string);
    }
dyStringFree(&dy);
}
