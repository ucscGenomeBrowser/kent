/* paraHub - parasol hub server. */
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "common.h"
#include "dlist.h"
#include "dystring.h"
#include "linefile.h"
#include "paraLib.h"
#include "net.h"
#include "paraHub.h"

void usage()
/* Explain usage and exit. */
{
errAbort("paraHub - parasol hub server.\n"
         "usage:\n"
	 "    paraHub start\n");
}

struct spoke *spokeList;	/* List of all spokes. */
struct dlList *freeSpokes;      /* List of free spokes. */
struct dlList *busySpokes;	/* List of busy spokes. */

struct machine *machineList; /* List of all machines. */
struct dlList *freeMachines;     /* List of machines ready for jobs. */
struct dlList *busyMachines;     /* List of machines running jobs. */
struct dlList *downMachines;     /* List of machines that aren't running. */

struct dlList *pendingList;     /* Jobs still to do. */
struct dlList *runningList;     /* Jobs that are running. */
int nextJobId = 0;		/* Next free job id. */

void removeJobId(int id);
/* Remove job with given ID. */

void setupLists()
/* Make up data structure to keep track of each machine. 
 * Try to get sockets on all of them. */
{
freeMachines = newDlList();
busyMachines = newDlList();
downMachines = newDlList();
pendingList = newDlList();
runningList = newDlList();
freeSpokes = newDlList();
busySpokes = newDlList();
}

struct machine *machineNew(char *name)
/* Create a new machine structure. */
{
struct machine *mach;
AllocVar(mach);
mach->name = cloneString(name);
AllocVar(mach->node);
mach->node->val = mach;
return mach;
}

void machineFree(struct machine **pMach)
/* Delete machine structure. */
{
struct machine *mach = *pMach;
if (mach != NULL)
    {
    freeMem(mach->node);
    freeMem(mach->name);
    freez(pMach);
    }
}

void addMachine(char *name)
/* Add machine to pool. */
{
struct machine *mach;

name = trimSpaces(name);
mach = machineNew(name);
dlAddTail(freeMachines, mach->node);
slAddHead(&machineList, mach);
}

void removeMachine(char *name)
/* Remove machine form pool. */
{
struct machine *mach;
name = trimSpaces(name);
for (mach = machineList; mach != NULL; mach = mach->next)
    {
    if (sameString(mach->name, name))
        {
	if (mach->job != NULL)
	    removeJobId(mach->job->id);
	dlRemove(mach->node);
	slRemoveEl(&machineList, mach);
	machineFree(&mach);
	break;
	}
    }
}

struct job *jobNew(char *cmd, char *user, char *dir, char *in, char *out, char *err)
/* Create a new job structure */
{
struct job *job;
AllocVar(job);
AllocVar(job->node);
job->node->val = job;
job->id = ++nextJobId;
job->cmd = cloneString(cmd);
job->user = cloneString(user);
job->dir = cloneString(dir);
job->in = cloneString(in);
job->out = cloneString(out);
job->err = cloneString(err);
return job;
}

void jobFree(struct job **pJob)
/* Free up a job. */
{
struct job *job = *pJob;
if (job != NULL)
    {
    freeMem(job->node);
    freeMem(job->cmd);
    freeMem(job->user);
    freeMem(job->dir);
    freeMem(job->in);
    freeMem(job->out);
    freeMem(job->err);
    freez(pJob);
    }
}

void runNextJobs()
/* Assign next job in pending queue if any to a machine. */
{
}

void addJob(char *line)
/* Add job.  Line format is <user> <dir> <stdin> <stdout> <stderr> <command> */
{
char *user, *dir, *in, *out, *err, *command;
struct job *job;

if ((user = nextWord(&line)) == NULL)
    return;
if ((dir = nextWord(&line)) == NULL)
    return;
if ((in = nextWord(&line)) == NULL)
    return;
if ((out = nextWord(&line)) == NULL)
    return;
if ((err = nextWord(&line)) == NULL)
    return;
if (line == NULL || line[0] == 0)
    return;
command = line;
job = jobNew(command, user, dir, in, out, err);
job->submitTime = time(NULL);
dlAddTail(pendingList, job->node);
runNextJobs();
}

struct job *jobFind(struct dlList *list, int id)
/* Find node of job with given id on list.  Return NULL if
 * not found. */
{
struct dlNode *el;
struct job *job;
for (el = list->head; !dlEnd(el); el = el->next)
    {
    job = el->val;
    if (job->id == id)
        return job;
    }
return NULL;
}

void recycleMachine(struct machine *mach)
/* Recycle machine into free list. */
{
mach->job = NULL;
dlRemove(mach->node);
dlAddTail(freeMachines, mach->node);
}

void sendKillJobMessage(struct machine *mach, struct job *job)
/* Send message to compute node to kill job there. */
{
}

void recycleJob(struct job *job)
/* Remove job from lists and free up memory associated with it. */
{
dlRemove(job->node);
jobFree(&job);
}

void finishJob(struct job *job)
/* Recycle job memory and the machine it's running on. */
{
struct machine *mach = job->machine;
if (mach != NULL)
     recycleMachine(mach);
recycleJob(job);
runNextJobs();
}

void removeJob(struct job *job)
/* Remove job - if it's running kill it,  remove from job list. */
{
if (job->machine != NULL)
    sendKillJobMessage(job->machine, job);
finishJob(job);
}

void removeJobId(int id)
/* Remove job of a given id. */
{
struct job *job = jobFind(runningList, id);
if (job == NULL)
   job = jobFind(pendingList, id);
if (job != NULL)
    removeJob(job);
}

void removeJobName(char *name)
/* Remove job of a given name. */
{
name = trimSpaces(name);
removeJobId(atoi(name));
}

void jobDone(char *name)
{
struct job *job;
name = trimSpaces(name);
job = jobFind(runningList, atoi(name));
if (job != NULL)
    finishJob(job);
}

void listMachines(int fd)
/* Write list of machines to fd.  Format is one machine per line
 * followed by a blank line. */
{
struct dyString *dy = newDyString(256);
struct machine *mach;
struct job *job;
for (mach = machineList; mach != NULL; mach = mach->next)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "%-10s ", mach->name);
    job = mach->job;
    if (job != NULL)
        dyStringPrintf(dy, "%-10s %s", job->user, job->cmd);
    else
        dyStringPrintf(dy, "idle");
    netSendLongString(fd, dy->string);
    }
netSendLongString(fd, "");
freeDyString(&dy);
}

void oneJobList(int fd, struct dlList *list, struct dyString *dy)
/* Write out one job list. */
{
struct dlNode *el;
struct job *job;
struct machine *mach;
char *machName;
for (el = list->head; !dlEnd(el); el = el->next)
    {
    machName = "none";
    job = el->val;
    mach = job->machine;
    if (mach != NULL)
        machName = mach->name;
    dyStringClear(dy);
    dyStringPrintf(dy, "%-4d %-10s %-10s %s", job->id, machName, job->user, job->cmd);
    netSendLongString(fd, dy->string);
    }
}

void listJobs(int fd)
/* Write list of jobs to fd. Format is one job per line
 * followed by a blank line. */
{
struct dyString *dy = newDyString(256);

oneJobList(fd, runningList, dy);
oneJobList(fd, pendingList, dy);
netSendLongString(fd, "");
freeDyString(&dy);
}

void status(int fd)
/* Write summary status to fd.  Format is lines of text
 * followed by a blank line. */
{
char buf[256];
sprintf(buf, "%d machines busy, %d free, %d jobs running, %d waiting, %d spokes busy, %d free", 
	dlCount(busyMachines), dlCount(freeMachines), 
	dlCount(runningList), dlCount(pendingList),
	dlCount(busySpokes), dlCount(freeSpokes));
netSendLongString(fd, buf);
netSendLongString(fd, "");
}

void addSpoke(int socketHandle, int connectionHandle)
/* Start up a new spoke and add it to free list. */
{
struct spoke *spoke;
static int closeList[4];
closeList[0] = connectionHandle;
closeList[1] = socketHandle;
closeList[2] = -1;
spoke = spokeNew(closeList);
if (spoke != NULL)
    {
    slAddHead(&spokeList, spoke);
    dlAddTail(freeSpokes, spoke->node);
    }
}

void killSpokes()
/* Kill all spokes. */
{
struct spoke *spoke, *next;
for (spoke = spokeList; spoke != NULL; spoke = next)
    {
    next = spoke->next;
    dlRemove(spoke->node);
    spokeFree(&spoke);
    }
}

void processHeartbeat()
/* Check that system is ok.  See if we can do anything useful. */
{
runNextJobs();
}

void startHub()
/* Do hub daemon - set up socket, and loop around on it until we get a quit. */
{
int readSize, res;
int socketHandle = 0, connectionHandle = 0;
char sig[20], *line, *command;
static struct sockaddr_in sai;		/* Some system socket info. */
int fromLen;
char *hostName = "localhost";
int sigLen = strlen(paraSig);
char *buf = NULL;

/* Set up various lists. */
setupLists();
assert(sigLen < sizeof(sig));
sig[sigLen] = 0;

/* Set up socket.  Get ready to listen to it. */
signal(SIGPIPE, SIG_IGN);	/* Block broken pipe signals. */
socketHandle = netSetupSocket(hostName, paraPort, &sai);
if (bind(socketHandle, (struct sockaddr*)&sai, sizeof(sai)) == -1)
     errAbort("Couldn't bind to %s port %d", hostName, paraPort);
res = listen(socketHandle, 100);

/* Start heartbeat. */
startHeartbeat();

/* Main event loop. */
for (;;)
    {
    int connectionHandle = accept(socketHandle, NULL, &fromLen);
    if (connectionHandle < 0)
        continue;
    if (!netMustReadAll(connectionHandle, sig, sigLen) || !sameString(sig, paraSig))
	{
	close(connectionHandle);
        continue;
	}
    line = buf = netGetLongString(connectionHandle);
    uglyf("hub: %s\n", buf);
    command = nextWord(&line);
    if (sameWord(command, "jobDone"))
         jobDone(line);
    else if (sameWord(command, "heartbeat"))
         processHeartbeat();
    else if (sameWord(command, "addJob"))
         addJob(line);
    else if (sameWord(command, "removeJob"))
         removeJobName(line);
    else if (sameWord(command, "addMachine"))
         addMachine(line);
    else if (sameWord(command, "removeMachine"))
         removeMachine(line);
    else if (sameWord(command, "listJobs"))
         listJobs(connectionHandle);
    else if (sameWord(command, "listMachines"))
         listMachines(connectionHandle);
    else if (sameWord(command, "status"))
         status(connectionHandle);
    else if (sameWord(command, "addSpoke"))
         addSpoke(socketHandle, connectionHandle);
    if (sameWord(command, "quit"))
         break;
    close(connectionHandle);
    freez(&buf);
    }
endHeartbeat();
killSpokes();
close(socketHandle);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;
if (argc < 2)
    usage();
command = argv[1];
if (sameString(command, "start"))
    startHub();
else
    errAbort("Unknown command %s", command);
return 0;
}


