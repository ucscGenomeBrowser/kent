/* paraHub - parasol hub server. */
#include "common.h"
#include "dlist.h"
#include "hash.h"
#include "options.h"
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
	 "    paraHub start\n"
	 "options:\n"
	 "    spokes=N  Number of processes that feed jobs to nodes - default 50\n"
	 "    machines=list  File with list of node machines, one per line\n"
	 "    log=logFile Write a log to logFile. Use 'stdout' here for console\n"
	 );

}

FILE *logFile = NULL;

void vLogIt(char *format, va_list args)
/* Virtual logit. */
{
if (logFile != NULL)
    {
    vfprintf(logFile, format, args);
    fflush(logFile);
    }
}

void logIt(char *format, ...)
/* Print message to log file. */
{
if (logFile != NULL)
    {
    va_list args;
    va_start(args, format);
    vLogIt(format, args);
    va_end(args);
    }
}



struct spoke *spokeList;	/* List of all spokes. */
struct dlList *freeSpokes;      /* List of free spokes. */
struct dlList *busySpokes;	/* List of busy spokes. */
struct dlList *deadSpokes;	/* List of dead spokes. */

struct machine *machineList; /* List of all machines. */
struct dlList *freeMachines;     /* List of machines ready for jobs. */
struct dlList *busyMachines;     /* List of machines running jobs. */
struct dlList *deadMachines;     /* List of machines that aren't running. */

struct dlList *pendingJobs;     /* Jobs still to do. */
struct dlList *runningJobs;     /* Jobs that are running. */
int nextJobId = 0;		/* Next free job id. */

char *hubHost;	/* Name of machine running this. */

void removeJobId(int id);
/* Remove job with given ID. */

void setupLists()
/* Make up data structure to keep track of each machine. 
 * Try to get sockets on all of them. */
{
freeMachines = newDlList();
busyMachines = newDlList();
deadMachines = newDlList();
pendingJobs = newDlList();
runningJobs = newDlList();
freeSpokes = newDlList();
busySpokes = newDlList();
deadSpokes = newDlList();
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

struct machine *findMachine(char *name)
/* Find named machine. */
{
struct machine *mach;
for (mach = machineList; mach != NULL; mach = mach->next)
     {
     if (sameString(mach->name, name))
         return mach;
     }
return NULL;
}

struct machine *findMachineOnDlList(char *name, struct dlList *list)
/* Find machine on list, or return NULL */
{
struct dlNode *node;
struct machine *mach;
for (node = list->head; !dlEnd(node); node = node->next)
    {
    mach = node->val;
    if (sameString(mach->name, name))
        return mach;
    }
return NULL;
}

void removeMachine(char *name)
/* Remove machine form pool. */
{
struct machine *mach;
name = trimSpaces(name);
if ((mach = findMachine(name)) != NULL)
    {
    if (mach->job != NULL)
	removeJobId(mach->job->id);
    dlRemove(mach->node);
    slRemoveEl(&machineList, mach);
    machineFree(&mach);
    }
}

void nodeDown(char *name)
/* Deal with a node going down - move it to dead list and
 * put job back on head of job list. */
{
struct machine *mach;
struct job *job;
name = trimSpaces(name);
if ((mach = findMachine(name)) != NULL)
    {
    if ((job = mach->job) != NULL)
        {
	mach->job = NULL;
	job->machine = NULL;
	dlRemove(job->node);
	dlAddHead(pendingJobs, job->node);
	}
    dlRemove(mach->node);
    mach->lastChecked = time(NULL);
    mach->isDead = TRUE;
    dlAddTail(deadMachines, mach->node);
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

boolean runNextJob()
/* Assign next job in pending queue if any to a machine. */
{
if (!dlEmpty(freeMachines) && !dlEmpty(freeSpokes) && !dlEmpty(pendingJobs))
    {
    struct dlNode *mNode, *jNode, *sNode;
    struct spoke *spoke;
    struct job *job;
    struct machine *machine;

    /* Get free resources from free list and move them to busy lists. */
    mNode = dlPopHead(freeMachines);
    dlAddTail(busyMachines, mNode);
    machine = mNode->val;
    jNode = dlPopHead(pendingJobs);
    dlAddTail(runningJobs, jNode);
    job = jNode->val;
    sNode = dlPopHead(freeSpokes);
    dlAddTail(busySpokes, sNode);
    spoke = sNode->val;
    spoke->lastPinged = time(NULL);
    spoke->pingCount = 0;

    /* Tell machine, job, and spoke about each other. */
    machine->job = job;
    job->machine = machine;
    spokeSendJob(spoke, machine, job);
    return TRUE;
    }
else
    return FALSE;
}

void runner(int count)
/* Try to run a couple of jobs. */
{
while (--count >= 0)
    if (!runNextJob())
        break;
}

void checkPeriodically(struct dlList *machList, int period, char *checkMessage)
/* Periodically send checkup messages to machines on list. */
{
struct dlNode *mNode, *sNode;
struct machine *machine;
struct spoke *spoke;
char message[512];

sprintf(message, "%s %s", checkMessage, hubHost);
for (;;)
    {
    /* If we have some free spokes and some busy machines, and
     * the busy machines haven't been checked for a while, go
     * check them. */
    if (dlEmpty(freeSpokes) || dlEmpty(machList))
        break;
    machine = machList->head->val;
    if (time(NULL) - machine->lastChecked < period)
        break;
    machine->lastChecked = time(NULL);
    mNode = dlPopHead(machList);
    sNode = dlPopHead(freeSpokes);
    dlAddTail(machList, mNode);
    dlAddTail(busySpokes, sNode);
    spoke = sNode->val;
    spoke->lastPinged = time(NULL);
    spokeSendMessage(spoke, machine, message);
    }
}

void hangman()
/* Check that nodes aren't hung.  Check that they aren't
 * free and we don't know about it. */
{
checkPeriodically(busyMachines, 60 * 1 /* 15 */, "check");
}

void graveDigger()
/* Check out dead nodes.  Try and resurrect them periodically. */
{
checkPeriodically(deadMachines, 60 * 2 /* 30 */, "resurrect");
}

void straightenSpokes()
/* Move spokes that have been busy too long back to
 * free spoke list under the assumption that we
 * missed a recycleSpoke message. */
{
struct dlNode *node, *next;
struct spoke *spoke;
time_t now = time(NULL);

for (node = busySpokes->head; !dlEnd(node); node = next)
    {
    next = node->next;
    spoke = node->val;
    if (now - spoke->lastPinged > 30)
	{
	if (spoke->pingCount < 10)
	    {
	    ++spoke->pingCount;
	    spoke->lastPinged = now;
	    spokePing(spoke);
	    }
	else
	    {
	    dlRemove(node);
	    dlAddTail(deadSpokes, node);
	    }
        }
    }
}

void processHeartbeat()
/* Check that system is ok.  See if we can do anything useful. */
{
runner(50);
graveDigger();
hangman();
straightenSpokes();
}


void nodeAlive(char *name)
/* Deal with message from node that says it's alive.
 * Move it from dead to free list. */
{
struct machine *mach;
struct dlNode *node;
for (node = deadMachines->head; !dlEnd(node); node = node->next)
    {
    mach = node->val;
    if (sameString(mach->name, name))
        {
	dlRemove(node);
	dlAddTail(freeMachines, node);
	mach->isDead = FALSE;
	runner(1);
	break;
	}
    }
}

void recycleMachine(struct machine *mach)
/* Recycle machine into free list. */
{
mach->job = NULL;
dlRemove(mach->node);
dlAddTail(freeMachines, mach->node);
}

void nodeCheckIn(char *line)
/* Deal with check in message from node. */
{
char *machine = nextWord(&line);
char *status = nextWord(&line);
if (status != NULL && sameString(status, "free"))
    {
    struct machine *mach = findMachineOnDlList(machine, busyMachines);
    if (mach != NULL)
         {
	 recycleMachine(mach);
	 logIt("hub:  >>>RECYCLING MACHINE IN NODE CHECK IN<<<<\n");
	 }
    }
}

void sendOk(boolean ok, boolean connectionHandle)
/* Send ok (or error) to handle. */
{
netSendLongString(connectionHandle, (ok ? "ok" : "error"));
}

void recycleSpoke(char *spokeName, int connectionHandle)
/* Try to find spoke and put it back on free list. */
{
struct dlNode *node;
struct spoke *spoke;
boolean foundSpoke = FALSE;
for (node = busySpokes->head; !dlEnd(node); node = node->next)
    {
    spoke = node->val;
    if (sameString(spoke->socketName, spokeName))
        {
	dlRemove(spoke->node);
	freez(&spoke->machine);
	dlAddTail(freeSpokes, spoke->node);
	foundSpoke = TRUE;
	break;
	}
    }
sendOk(foundSpoke, connectionHandle);
if (!foundSpoke)
    warn("Couldn't free spoke %s", spokeName);
else
    runner(1);
}

boolean addJob(char *line)
/* Add job.  Line format is <user> <dir> <stdin> <stdout> <stderr> <command> */
{
char *user, *dir, *in, *out, *err, *command;
struct job *job;

if ((user = nextWord(&line)) == NULL)
    return FALSE;
if ((dir = nextWord(&line)) == NULL)
    return FALSE;
if ((in = nextWord(&line)) == NULL)
    return FALSE;
if ((out = nextWord(&line)) == NULL)
    return FALSE;
if ((err = nextWord(&line)) == NULL)
    return FALSE;
if (line == NULL || line[0] == 0)
    return FALSE;
command = line;
job = jobNew(command, user, dir, in, out, err);
job->submitTime = time(NULL);
dlAddTail(pendingJobs, job->node);
runner(1);
return TRUE;
}

void addJobAcknowledge(char *line, int connectionHandle)
/* Add job and send 'ok' or 'err' back to client. */
{
sendOk(addJob(line), connectionHandle);
}

void respondToPing(int connectionHandle)
/* Someone want's to know we're alive. */
{
netSendLongString(connectionHandle, "ok");
processHeartbeat();
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
runner(1);
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
struct job *job = jobFind(runningJobs, id);
if (job == NULL)
   job = jobFind(pendingJobs, id);
if (job != NULL)
    removeJob(job);
}

void removeJobName(char *name)
/* Remove job of a given name. */
{
name = trimSpaces(name);
removeJobId(atoi(name));
}

void jobDone(char *line)
/* Handle job is done message. */
{
struct job *job;
char *machine, *id, *status;
id = nextWord(&line);
status = nextWord(&line);
if (status != NULL)
    {
    job = jobFind(runningJobs, atoi(id));
    if (job != NULL)
	{
	job->machine->lastChecked = time(NULL);
	finishJob(job);
	}
    }
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
	{
	if (mach->isDead)
	    dyStringPrintf(dy, "dead");
	else
	    dyStringPrintf(dy, "idle");
	}
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

oneJobList(fd, runningJobs, dy);
oneJobList(fd, pendingJobs, dy);
netSendLongString(fd, "");
freeDyString(&dy);
}

void status(int fd)
/* Write summary status to fd.  Format is lines of text
 * followed by a blank line. */
{
char buf[256];
sprintf(buf, "%d machines busy, %d free, %d dead, %d jobs running, %d waiting, %d spokes busy, %d free, %d dead", 
	dlCount(busyMachines), dlCount(freeMachines),  dlCount(deadMachines), 
	dlCount(runningJobs), dlCount(pendingJobs),
	dlCount(busySpokes), dlCount(freeSpokes), dlCount(deadSpokes));
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

int hubConnect()
/* Return connection to hub socket - with paraSig already written. */
{
int hubFd;
int sigSize = strlen(paraSig);
hubFd  = netConnect(hubHost, paraPort);
if (hubFd > 0)
    write(hubFd, paraSig, sigSize);
return hubFd;
}

void startSpokes()
/* Start default number of spokes. */
{
int i, initialSpokes = atoi(optionVal("spokes", "50"));
for (i=0; i<initialSpokes; ++i)
    addSpoke(-1, -1);
}

void startMachines()
/* If they give us a beginning machine list use it here. */
{
char *fileName = optionVal("machines", NULL);
if (fileName != NULL)
    {
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    char *line;
    while (lineFileNext(lf, &line, NULL))
	addMachine(line);
    lineFileClose(&lf);
    }
}

void startHub()
/* Do hub daemon - set up socket, and loop around on it until we get a quit. */
{
int socketHandle = 0, connectionHandle = 0;
char sig[20], *line, *command;
int sigLen = strlen(paraSig);
char *buf = NULL;

/* Find name and IP address of our machine. */
hubHost = getHost();
logIt("Starting paraHub on %s\n", hubHost);

/* Set up various lists. */
setupLists();
startMachines();
assert(sigLen < sizeof(sig));
sig[sigLen] = 0;

/* Set up socket.  Get ready to listen to it. */
socketHandle = netAcceptingSocket(paraPort, 100);
if (socketHandle < 0)
    errAbort("Can't set up socket.  Urk!  I'm dead.");

/* Do some initialization. */
startSpokes();
startHeartbeat();

/* Main event loop. */
for (;;)
    {
    int connectionHandle = netAccept(socketHandle);
    if (connectionHandle < 0)
        continue;
    if (!netMustReadAll(connectionHandle, sig, sigLen) || !sameString(sig, paraSig))
	{
	close(connectionHandle);
        continue;
	}
    line = buf = netGetLongString(connectionHandle);
    if (line == NULL)
        {
	close(connectionHandle);
        continue;
	}
    logIt("hub: %s\n", line);
    command = nextWord(&line);
    if (sameWord(command, "jobDone"))
         jobDone(line);
    else if (sameWord(command, "recycleSpoke"))
         recycleSpoke(line, connectionHandle);
    else if (sameWord(command, "heartbeat"))
         processHeartbeat();
    else if (sameWord(command, "addJob"))
         addJobAcknowledge(line, connectionHandle);
    else if (sameWord(command, "nodeDown"))
         nodeDown(line);
    else if (sameWord(command, "alive"))
         nodeAlive(line);
    else if (sameWord(command, "checkIn"))
         nodeCheckIn(line);
    else if (sameWord(command, "removeJob"))
         removeJobName(line);
    else if (sameWord(command, "ping"))
         respondToPing(connectionHandle);
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
char *command, *log;
optionHash(&argc, argv);
if (argc < 2)
    usage();
log = optionVal("log", NULL);
if (log != NULL)
    logFile = mustOpen(log, "w");
pushWarnHandler(vLogIt);
command = argv[1];
if (sameString(command, "start"))
    startHub();
else
    errAbort("Unknown command %s", command);
return 0;
}


