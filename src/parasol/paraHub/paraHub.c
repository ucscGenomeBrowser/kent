/* paraHub - Parasol hub server.  This is the heart of the parasol system.
 * The hub daemon spawns a heartbeat daemon and a number of spoke deamons
 * on startup,  and then goes into a loop processing messages it recieves
 * on the hub TCP/IP socket.  The hub daemon does not do anything time consuming
 * in this loop.   The main thing the hub daemon does is put jobs on the
 * job list,  move machines from the busy list to the free list,  and call
 * the 'runner' routine.
 *
 * The runner routine looks to see if there is a free machine, a free spoke,
 * and a job to run.  If so it will send a message to the spoke telling
 * it to run the job on the machine,  and then move the job from the 'pending'
 * to the 'running' list,  the spoke from the freeSpoke to the busySpoke list, 
 * and the machine from the freeMachine to the busyMachine list.   This
 * indirection of starting jobs via a separate spoke process avoids the
 * hub daemon itself having to wait to find out if a machine is down.
 *
 * When a spoke is done assigning a job, the spoke sends a 'recycleSpoke'
 * message to the hub, which puts the spoke back on the freeSpoke list.
 * Likewise when a job is done the machine running the jobs sends a 
 * 'job done' message to the hub, which puts the machine back on the
 * free list,  writes the job exit code to a file, and removes the job
 * from the system.
 *
 * Sometimes a spoke will find that a machine is down.  In this case it
 * sends a 'node down' message to the hub as well as the 'spoke free'
 * message.   The hub will then move the machine to the deadMachines list,
 * and put the job back on the top of the pending list.
 *
 * The heartbeat daemon simply sits in a loop sending heartbeat messages to
 * the hub every so often (every 30 seconds currently), and sleeping
 * the rest of the time.   When the hub gets a heartbeat message it
 * does a few things:
 *     o - It calls runner to try and start some more jobs.  (Runner
 *         is also called at the end of processing a recycleSpoke, 
 *         jobDone, addJob or addMachine message.  Typically runner
 *         won't find anything new to run in the heartbeat, but this
 *         is put here mostly just in case of unforseen issues.)
 *    o -  It calls graveDigger, a routine which sees if machines
 *         on the dead list have come back to life.
 *    o -  It calls hangman, a routine which sees if jobs the system
 *         thinks have been running for a long time are still 
 *         running on the machine they have been assigned to.
 *         If the machine has gone down it is moved to the dead list
 *         and the job is reassigned. 
 *
 * This whole system depends on the hub daemon being able to finish
 * processing messages fast enough to keep the connection queue on the
 * hub socket from overflowing.  Each job involves 3 messages to the
 * hub socket:
 *     addJob - from a client to add the job to the system
 *     recycleSpoke - from the spoke after it's dispatched the job
 *     jobDone - from the compute node when the job is finished
 * On some of the earlier Linux kernals we had trouble with the
 * connection queue overflowing when dispatching lots of short
 * jobs.  This seemed to be from the jobDone messages coming
 * in faster than Linux could make connections rather than the
 * hub daemon being slow.  On the kilokluster with a more modern
 * kernal this has not been a problem - even with very 0.1 second
 * jobs on 1000 CPUs.  Should overflow occur the heartbeat processing
 * should gradually rescue the system in any case, but the throughput
 * will be greatly reduced. */

#include <time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "hash.h"
#include "errabort.h"
#include "dystring.h"
#include "dlist.h"
#include "net.h"
#include "paraLib.h"
#include "paraHub.h"
#include "machSpec.h"

int version = 1;	/* Version number. */

/* Some command-line configurable quantities and their defaults. */
int jobCheckPeriod = 10;	/* Minutes between checking running jobs. */
int machineCheckPeriod = 20;	/* Minutes between checking dead machines. */
int initialSpokes = 30;		/* Number of spokes to start with. */
unsigned char subnet[4] = {255,255,255,255};   /* Subnet to check. */
int nextJobId = 0;		/* Next free job id. */

unsigned char localHost[4] = {127,0,0,1};	   /* Address for local host */

void usage()
/* Explain usage and exit. */
{
errAbort("paraHub - parasol hub server version %d\n"
         "usage:\n"
	 "    paraHub machineList\n"
	 "Where machine list is a file with machine names in the\n"
	 "first column, and number of CPUs in the second column.\n"
	 "options:\n"
	 "    spokes=N  Number of processes that feed jobs to nodes - default %d\n"
	 "    jobCheckPeriod=N  Minutes between checking on job - default %d\n"
	 "    machineCheckPeriod=N Seconds between checking on machine - default %d\n"
	 "    subnet=XXX.YYY.ZZZ Only accept connections from subnet (example 192.168)\n"
	 "    nextJobId=N  Starting job ID number\n"
	 "    log=logFile Write a log to logFile. Use 'stdout' here for console\n"
	 "    logFlush Flush log with every write\n"
	               ,
	 version, initialSpokes, jobCheckPeriod, machineCheckPeriod
	 );
}

struct spoke *spokeList;	/* List of all spokes. */
struct dlList *freeSpokes;      /* List of free spokes. */
struct dlList *busySpokes;	/* List of busy spokes. */
struct dlList *deadSpokes;	/* List of dead spokes. */

struct machine *machineList; /* List of all machines. */
struct dlList *freeMachines;     /* List of machines ready for jobs. */
struct dlList *busyMachines;     /* List of machines running jobs. */
struct dlList *deadMachines;     /* List of machines that aren't running. */

struct dlList *runningJobs;     /* Jobs that are running. */

struct hash *userHash;		/* Hash of all users. */
struct user *userList;		/* List of all users. */
struct dlList *queuedUsers;	/* Users with jobs in queue. */
struct dlList *unqueuedUsers;   /* Users with no jobs in queue. */


struct resultQueue *resultQueues; /* Result files. */
int finishedJobCount = 0;		/* Number of finished jobs. */
int crashedJobCount = 0;		/* Number of crashed jobs. */

char *jobIdFileName = "parasol.jid";	/* File name where jobId file is. */
FILE *jobIdFile = NULL;			/* Handle to jobId file. */

char *hubHost;	/* Name of machine running this. */

void removeJobId(int id);
/* Remove job with given ID. */

void setupLists()
/* Make up machine, spoke, user and job lists - all doubly linked
 * so it is fast to remove items from one list and put them
 * on another. */
{
freeMachines = newDlList();
busyMachines = newDlList();
deadMachines = newDlList();
runningJobs = newDlList();
freeSpokes = newDlList();
busySpokes = newDlList();
deadSpokes = newDlList();
queuedUsers = newDlList();
unqueuedUsers = newDlList();
userHash = newHash(6);
}

struct user *findUser(char *name)
/* Find user.  If it's the first time we've seen this
 * user then make up a user object and put it on the
 * idle user list. */
{
struct user *user = hashFindVal(userHash, name);
if (user == NULL)
    {
    AllocVar(user);
    slAddHead(&userList, user);
    hashAddSaveName(userHash, name, user, &user->name);
    AllocVar(user->node);
    user->node->val = user;
    dlAddTail(unqueuedUsers, user->node);
    user->jobQueue = newDlList();
    }
return user;
}

struct user *findLuckyUser()
/* Find lucky user who gets to run a job. */
{
struct user *minUser = NULL;
int minCount = BIGNUM;
struct dlNode *node;
for (node = queuedUsers->head; !dlEnd(node); node = node->next)
    {
    struct user *user = node->val;
    if (user->runningCount < minCount && !dlEmpty(user->jobQueue))
        {
	minCount = user->runningCount;
	minUser = user;
	}
    }
return minUser;
}


boolean runNextJob()
/* Assign next job in pending queue if any to a machine. */
{
struct user *user = findLuckyUser();
if (user != NULL && !dlEmpty(freeMachines) && !dlEmpty(freeSpokes))
    {
    struct dlNode *mNode, *jNode, *sNode;
    struct spoke *spoke;
    struct job *job;
    struct machine *machine;
    time_t now = time(NULL);

    /* Get free resources from free lists and move them to busy lists. */
    mNode = dlPopHead(freeMachines);
    dlAddTail(busyMachines, mNode);
    machine = mNode->val;
    machine->lastChecked = now;
    jNode = dlPopHead(user->jobQueue);
    dlAddTail(runningJobs, jNode);
    job = jNode->val;
    sNode = dlPopHead(freeSpokes);
    dlAddTail(busySpokes, sNode);
    spoke = sNode->val;
    spoke->lastPinged = now;
    spoke->pingCount = 0;

    /* Update user and if this is the last job take them off queue. */
    ++user->runningCount;
    if (dlEmpty(user->jobQueue))
        {
	dlRemove(user->node);
	dlAddTail(unqueuedUsers, user->node);
	}

    /* Tell machine, job, and spoke about each other. */
    machine->job = job;
    job->machine = machine;
    job->startTime = now;
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

struct machine *machineNew(char *name, char *tempDir)
/* Create a new machine structure. */
{
struct machine *mach;
AllocVar(mach);
mach->name = cloneString(name);
mach->tempDir = cloneString(tempDir);
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
    freeMem(mach->tempDir);
    freez(pMach);
    }
}

void doAddMachine(char *name, char *tempDir)
/* Add machine to pool. */
{
struct machine *mach;
mach = machineNew(name, tempDir);
dlAddTail(freeMachines, mach->node);
slAddHead(&machineList, mach);
}

void addMachine(char *line)
/* Process message to add machine to pool. */
{
char *name = nextWord(&line);
char *tempDir = nextWord(&line);
if (tempDir != NULL)
    {
    doAddMachine(name, tempDir);
    runner(1);
    }
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

struct machine *findMachineWithJob(char *name, int jobId)
/* Find named machine that is running job.  If jobId is
 * 0, find it regardless of job it's running. */
{
struct machine *mach;
for (mach = machineList; mach != NULL; mach = mach->next)
     {
     if (sameString(mach->name, name))
	 {
	 struct job *job = mach->job;
	 if (jobId == 0)
	     return mach;
	 if (job != NULL && job->id == jobId)
	     return mach;
	 }
     }
return NULL;
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

void removeMachine(char *name)
/* Remove machine from pool. */
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

void requeueJob(struct job *job)
/* Move job from running queue back to a user pending
 * queue.  This happens when a node is down or when
 * it missed the message about a job. */
{
struct user *user = job->user;
struct machine *mach = job->machine;
if (mach != NULL)
    mach->job = NULL;
job->machine = NULL;
dlRemove(job->node);
dlAddHead(user->jobQueue, job->node);
dlRemove(user->node);
user->runningCount -= 1;
dlAddTail(queuedUsers, user->node);
}

void nodeDown(char *line)
/* Deal with a node going down - move it to dead list and
 * put job back on head of job list. */
{
struct machine *mach;
char *machName = nextWord(&line);
char *jobIdString = nextWord(&line);
int jobId;
struct job *job;

if (jobIdString == NULL)
    return;
jobId = atoi(jobIdString);
if ((mach = findMachineWithJob(machName, jobId)) != NULL)
    {
    if ((job = mach->job) != NULL)
	requeueJob(job);
    dlRemove(mach->node);
    mach->lastChecked = time(NULL);
    mach->isDead = TRUE;
    dlAddTail(deadMachines, mach->node);
    }
runner(1);
}

char *exeFromCommand(char *cmd)
/* Return executable name (without path) given command line. */
{
static char exe[128];
char *s,*e,*x;
int i, size;
int lastSlash = -1;

/* Isolate first space-delimited word between s and e. */
s = skipLeadingSpaces(cmd);
e = skipToSpaces(cmd);
if (e == NULL) 
    e = s + strlen(s);
size = e - s;

/* Find last '/' in this word if any, and reposition s after it. */
for (i=0; i<size; ++i)
    {
    if (s[i] == '/')
        lastSlash = i;
    }
if (lastSlash > 0)
    s += lastSlash + 1;

/* Copy whats left to string to return . */
size = e - s;
if (size >= sizeof(exe))
    size = sizeof(exe)-1;
memcpy(exe, s, size);
exe[size] = 0;
return exe;
}

struct job *jobNew(char *cmd, char *user, char *dir, char *in, char *out, char *results)
/* Create a new job structure */
{
struct job *job;
AllocVar(job);
AllocVar(job->node);
job->node->val = job;
job->id = ++nextJobId;
job->exe = cloneString(exeFromCommand(cmd));
job->cmd = cloneString(cmd);
job->user = findUser(user);
job->dir = cloneString(dir);
job->in = cloneString(in);
job->out = cloneString(out);
job->results = cloneString(results);
return job;
}

void jobFree(struct job **pJob)
/* Free up a job. */
{
struct job *job = *pJob;
if (job != NULL)
    {
    freeMem(job->node);
    freeMem(job->exe);
    freeMem(job->cmd);
    freeMem(job->dir);
    freeMem(job->in);
    freeMem(job->out);
    freeMem(job->err);
    freeMem(job->results);
    freez(pJob);
    }
}

boolean sendViaSpoke(struct machine *machine, char *message)
/* Send a message to machine via spoke. */
{
struct dlNode *node = dlPopHead(freeSpokes);
struct spoke *spoke;
if (node == NULL)
    return FALSE;
dlAddTail(busySpokes, node);
spoke = node->val;
spoke->lastPinged = time(NULL);
spokeSendMessage(spoke, machine, message);
return TRUE;
}

void checkPeriodically(struct dlList *machList, int period, char *checkMessage,
	int spokesToUse, time_t now)
/* Periodically send checkup messages to machines on list. */
{
struct dlNode *mNode;
struct machine *machine;
char message[512];
int i;

sprintf(message, "%s %s", checkMessage, hubHost);
for (i=0; i<spokesToUse; ++i)
    {
    /* If we have some free spokes and some busy machines, and
     * the busy machines haven't been checked for a while, go
     * check them. */
    if (dlEmpty(freeSpokes) || dlEmpty(machList))
        break;
    machine = machList->head->val;
    if (now - machine->lastChecked < period)
        break;
    machine->lastChecked = now;
    mNode = dlPopHead(machList);
    dlAddTail(machList, mNode);
    sendViaSpoke(machine, message);
    }
}

void hangman(int spokesToUse, time_t now)
/* Check that busy nodes aren't dead.  Also send message for 
 * busy nodes to check in, in case we missed one of their earlier
 * jobDone messages. */
{
int i, period = jobCheckPeriod*MINUTE;
struct dlNode *mNode;
struct machine *machine;
struct job *job;

for (i=0; i<spokesToUse; ++i)
    {
    if (dlEmpty(freeSpokes) || dlEmpty(busyMachines))
        break;
    machine = busyMachines->head->val;
    if (now - machine->lastChecked < period)
        break;
    machine->lastChecked = now;
    mNode = dlPopHead(busyMachines);
    dlAddTail(busyMachines, mNode);
    job = machine->job;
    if (job != NULL)
        {
	char message[512];
	sprintf(message, "check %s %d", hubHost, job->id);
	sendViaSpoke(machine, message);
	}
    }
}

void graveDigger(int spokesToUse, time_t now)
/* Check out dead nodes.  Try and resurrect them periodically. */
{
checkPeriodically(deadMachines, MINUTE * machineCheckPeriod, "resurrect", 
	spokesToUse, now);
}

void straightenSpokes(time_t now)
/* Move spokes that have been busy too long back to
 * free spoke list under the assumption that we
 * missed a recycleSpoke message. */
{
struct dlNode *node, *next;
struct spoke *spoke;

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

void flushResults()
/* Flush all results files. */
{
struct resultQueue *rq;
for (rq = resultQueues; rq != NULL; rq = rq->next)
    {
    if (rq->f != NULL)
       fflush(rq->f);
    }
}

void writeJobResults(struct job *job, time_t now, char *status,
	char *uTime, char *sTime)
/* Write out job results to output queue.  This
 * will create the output queue if it doesn't yet
 * exist. */
{
struct resultQueue *rq;
for (rq = resultQueues; rq != NULL; rq = rq->next)
    if (sameString(job->results, rq->name))
        break;
if (rq == NULL)
    {
    AllocVar(rq);
    slAddHead(&resultQueues, rq);
    rq->name = cloneString(job->results);
    rq->f = fopen(rq->name, "a");
    if (rq->f == NULL)
        warn("hub: couldn't open results file %s", rq->name);
    rq->lastUsed = now;
    }
if (rq->f != NULL)
    {
    char *machName;
    if (job->machine != NULL)
        machName = job->machine->name;
    else
        machName = "ghost";
    fprintf(rq->f, "%s %s %d %s %s %s %lu %lu %lu %s %s '%s'\n",
        status, machName, job->id, job->exe, 
	uTime, sTime, 
	job->submitTime, job->startTime, now,
	job->user->name, job->err, job->cmd);
    if (sameString(status, "0"))
        ++finishedJobCount;
    else
        ++crashedJobCount;
    rq->lastUsed = now;
    }
}

void resultQueueFree(struct resultQueue **pRq)
/* Free up a results queue, closing file if open. */
{
struct resultQueue *rq = *pRq;
if (rq != NULL)
    {
    carefulClose(&rq->f);
    freeMem(rq->name);
    freez(pRq);
    }
}


void sweepResults(time_t now)
/* Get rid of result queues that haven't been accessed for
 * a while. Flush all results. */
{
struct resultQueue *newList = NULL, *rq, *next;
for (rq = resultQueues; rq != NULL; rq = next)
    {
    next = rq->next;
    if (now - rq->lastUsed > 5*MINUTE)
	{
	logIt("hub: closing results file %s\n", rq->name);
        resultQueueFree(&rq);
	}
    else
        {
	slAddHead(&newList, rq);
	}
    }
slReverse(&newList);
resultQueues = newList;
flushResults();
}

void saveJobId()
/* Save job ID. */
{
rewind(jobIdFile);
writeOne(jobIdFile, nextJobId);
fflush(jobIdFile);
}

void openJobId()
/* Open file with jobID in it and read jobId.  Bump it
 * by 100000 in case we crashed to avoid reusing job
 * id's, but do reuse every 2 billion. Let command line
 * overwrite this though . */
{
jobIdFile = fopen(jobIdFileName, "r+");
if (jobIdFile != NULL)
    {
    readOne(jobIdFile, nextJobId);
    nextJobId += 100000;
    }
else
    jobIdFile = mustOpen(jobIdFileName, "w");
if (nextJobId < 0)
    nextJobId = 0;
nextJobId = optionInt("nextJobId", nextJobId);
}

void processHeartbeat()
/* Check that system is ok.  See if we can do anything useful. */
{
int spokesToUse;
time_t now = time(NULL);
runner(30);
straightenSpokes(now);
spokesToUse = dlCount(freeSpokes);
if (spokesToUse > 0)
    {
    spokesToUse >>= 1;
    spokesToUse -= 1;
    if (spokesToUse < 1) spokesToUse = 1;
    graveDigger(spokesToUse, now);
    hangman(spokesToUse, now);
    sweepResults(now);
    flushLog();
    saveJobId();
    }
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

void recycleJob(struct job *job)
/* Remove job from lists and free up memory associated with it. */
{
dlRemove(job->node);
jobFree(&job);
}

void nodeCheckIn(char *line)
/* Deal with check in message from node. */
{
char *machine = nextWord(&line);
char *jobIdString = nextWord(&line);
char *status = nextWord(&line);
int jobId = atoi(jobIdString);
if (status != NULL)
    {
    /* Node thinks it's free, we think it has a job.  Node
     * must have missed our job assignment... */
    if (sameString(status, "free"))
	{
	struct job *job = jobFind(runningJobs, jobId);
	struct user *user = job->user;
	if (job != NULL)
	    {
	    struct machine *mach = job->machine;
	    if (mach != NULL)
	        {
	        dlRemove(mach->node);
	        dlAddTail(freeMachines, mach->node);
		}
	    requeueJob(job);
	    logIt("hub:  requeueing job in nodeCheckIn\n");
	    }
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

int addJob(char *line)
/* Add job to queues. */
{
char *userName, *dir, *in, *out, *results, *command;
struct job *job;
struct user *user;

if ((userName = nextWord(&line)) == NULL)
    return 0;
if ((dir = nextWord(&line)) == NULL)
    return 0;
if ((in = nextWord(&line)) == NULL)
    return 0;
if ((out = nextWord(&line)) == NULL)
    return 0;
if ((results = nextWord(&line)) == NULL)
    return 0;
if (line == NULL || line[0] == 0)
    return 0;
command = line;
job = jobNew(command, userName, dir, in, out, results);
user = job->user;
dlAddTail(user->jobQueue, job->node);
dlRemove(user->node);
dlAddTail(queuedUsers, user->node);
job->submitTime = time(NULL);
return job->id;
}

void addJobAcknowledge(char *line, int connectionHandle)
/* Add job.  Line format is <user> <dir> <stdin> <stdout> <results> <command> 
 * Returns job ID or 0 if a problem.  Send jobId back to client. */
{
int id = addJob(line);
char jobIdString[16];
sprintf(jobIdString, "%d", id);
netSendLongString(connectionHandle, jobIdString);
runner(1);
}

void respondToPing(int connectionHandle)
/* Someone want's to know we're alive. */
{
netSendLongString(connectionHandle, "ok");
processHeartbeat();
}

void sendKillJobMessage(struct machine *machine, struct job *job)
/* Send message to compute node to kill job there. */
{
char message[64];
sprintf(message, "kill %d", job->id);
logIt("  %s %s\n", machine->name, message);
sendViaSpoke(machine, message);
     /* There's a race condition here that can cause jobs not
      * to be killed if there are no spokes free to handle
      * this message.  I'm trying to think of a simple solution
      * to this. The damage isn't too serious though,
      * since the jobs will at least stay on the main job list.
      * if they are not killed.  User can curse us and try to
      * kill jobs again.  At least the node won't end up 
      * double-booked.  */
}

void finishJob(struct job *job)
/* Recycle job memory and the machine it's running on. */
{
struct machine *mach = job->machine;
struct user *user = job->user;
if (mach != NULL)
    {
    recycleMachine(mach);
    user->runningCount -= 1;
    }
recycleJob(job);
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
logIt("Running job %x\n", job);
if (job == NULL)
    {
    /* If it's not running look in user job queues. */
    struct user *user;
    for (user = userList; user != NULL; user = user->next)
        {
	if ((job = jobFind(user->jobQueue, id)) != NULL)
	    break;
	}
    logIt("Pending job %x\n", job);
    }
if (job != NULL)
    removeJob(job);
}

void removeJobAcknowledge(char *names, int connectionHandle)
/* Remove job of a given name(s). */
{
char *name;
while ((name = nextWord(&names)) != NULL)
    removeJobId(atoi(name));
netSendLongString(connectionHandle, "ok");
}

void jobDone(char *line)
/* Handle job is done message. */
{
struct job *job;
char *id, *status, *uTime, *sTime, *tTime;
id = nextWord(&line);
status = nextWord(&line);
uTime = nextWord(&line);
sTime = nextWord(&line);
if (sTime != NULL)
    {
    job = jobFind(runningJobs, atoi(id));
    if (job != NULL)
	{
	time_t now = time(NULL);
	if (job->machine != NULL)
	    job->machine->lastChecked = now;
	writeJobResults(job, now, status, uTime, sTime);
	finishJob(job);
	runner(1);
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
        dyStringPrintf(dy, "%-10s %s", job->user->name, job->cmd);
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

void listUsers(int fd)
/* Write list of users to fd.  Format is one user per line
 * followed by a blank line. */
{
struct dyString *dy = newDyString(256);
struct user *user;
for (user = userList; user != NULL; user = user->next)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "%s ", user->name);
    dyStringPrintf(dy, "%d running, %d waiting", 
    	user->runningCount, dlCount(user->jobQueue));
    netSendLongString(fd, dy->string);
    }
netSendLongString(fd, "");
freeDyString(&dy);
}

void appendLocalTime(struct dyString *dy, time_t t)
/* Append time t converted to day/time format to dy. */
{
struct tm *tm;
tm = localtime(&t);
dyStringPrintf(dy, "%02d/%02d/%d %02d:%02d:%02d",
   tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

char *upToFirstDot(char *s, bool dotQ)
/* Return string up to first dot. */
{
static char ret[128];
int size;
char *e = strchr(s, '.');
if (e == NULL)
    size = strlen(s);
else
    size = e - s;
if (size >= sizeof(ret)-2)	/* Leave room for .q */
    size = sizeof(ret)-3;
memcpy(ret, s, size);
ret[size] = 0;
if (dotQ)
    strcat(ret, ".q");
return ret;
}

void oneJobList(int fd, struct dlList *list, struct dyString *dy, boolean sinceStart)
/* Write out one job list. */
{
struct dlNode *el;
struct job *job;
char *machName;
for (el = list->head; !dlEnd(el); el = el->next)
    {
    job = el->val;
    if (job->machine != NULL)
        machName = upToFirstDot(job->machine->name, FALSE);
    else
	machName = "none";
    dyStringClear(dy);
    dyStringPrintf(dy, "%-4d %-10s %-10s ", job->id, machName, job->user->name);
    if (sinceStart)
        appendLocalTime(dy, job->startTime);
    else
        appendLocalTime(dy, job->submitTime);
    dyStringPrintf(dy, " %s", job->cmd);
    netSendLongString(fd, dy->string);
    }
}

void listJobs(int fd)
/* Write list of jobs to fd. Format is one job per line
 * followed by a blank line. */
{
struct dyString *dy = newDyString(256);
struct user *user;
oneJobList(fd, runningJobs, dy, TRUE);
for (user = userList; user != NULL; user = user->next)
    oneJobList(fd, user->jobQueue, dy, FALSE);
netSendLongString(fd, "");
freeDyString(&dy);
}

void onePstatList(int fd, struct dlList *list, boolean running, struct dyString *dy)
/* Write out one job list in pstat format. */
{
struct dlNode *node;
struct job *job;
time_t t;
char *machName;
char *s;
char *state = (running ? "r" : "q");

flushResults();
for (node = list->head; !dlEnd(node); node = node->next)
    {
    job = node->val;
    if (job->machine != NULL)
	machName = job->machine->name;
    else
        machName = "none";
    if (running)
        t = job->startTime;
    else
        t = job->submitTime;
    dyStringClear(dy);
    dyStringPrintf(dy, "%s %d %s %s %lu %s", 
        state, job->id, job->user->name, job->exe, t, machName);
    netSendLongString(fd, dy->string);
    }
}

void pstat(int fd)
/* Write list of jobs in pstat format. */
{
struct dyString *dy = newDyString(0);
struct user *user;
onePstatList(fd, runningJobs, TRUE, dy);
for (user = userList; user != NULL; user = user->next)
    onePstatList(fd, user->jobQueue, FALSE, dy);
netSendLongString(fd, "");
freeDyString(&dy);
}

int sumPendingJobs()
/* Return sum of all pending jobs for all users. */
{
struct user *user;
int count = 0;

for (user = userList; user != NULL; user = user->next)
    count += dlCount(user->jobQueue);
return count;
}

int countActiveUsers()
/* Return count of users with jobs running or in queue */
{
struct user *user;
int count = 0;

for (user = userList; user != NULL; user = user->next)
    {
    if (user->runningCount > 0 || !dlEmpty(user->jobQueue))
        ++count;
    }
return count;
}

void status(int fd)
/* Write summary status to fd.  Format is lines of text
 * followed by a blank line. */
{
char buf[256];
sprintf(buf, "CPUs free: %d", dlCount(freeMachines));
netSendLongString(fd, buf);
sprintf(buf, "CPUs busy: %d", dlCount(busyMachines));
netSendLongString(fd, buf);
sprintf(buf, "CPUs dead: %d", dlCount(deadMachines));
netSendLongString(fd, buf);
sprintf(buf, "Jobs running:  %d", dlCount(runningJobs));
netSendLongString(fd, buf);
sprintf(buf, "Jobs waiting:  %d", sumPendingJobs());
netSendLongString(fd, buf);
sprintf(buf, "Jobs finished: %d", finishedJobCount);
netSendLongString(fd, buf);
sprintf(buf, "Jobs crashed:  %d", crashedJobCount);
netSendLongString(fd, buf);
sprintf(buf, "Spokes free: %d", dlCount(freeSpokes));
netSendLongString(fd, buf);
sprintf(buf, "Spokes busy: %d", dlCount(busySpokes));
netSendLongString(fd, buf);
sprintf(buf, "Spokes dead: %d", dlCount(deadSpokes));
netSendLongString(fd, buf);
sprintf(buf, "Active users: %d", countActiveUsers());
netSendLongString(fd, buf);
sprintf(buf, "Total users: %d", slCount(userList));
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
hubFd  = netConnect("127.0.0.1", paraPort);
if (hubFd > 0)
    write(hubFd, paraSig, sigSize);
return hubFd;
}

void startSpokes()
/* Start default number of spokes. */
{
int i;
for (i=0; i<initialSpokes; ++i)
    addSpoke(-1, -1);
}

void startMachines(char *fileName)
/* If they give us a beginning machine list use it here. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[7];
while (lineFileRow(lf, row))
    {
    struct machSpec ms;
    int i;
    machSpecStaticLoad(row, &ms);
    for (i=0; i<ms.cpus; ++i)
	doAddMachine(ms.name, ms.tempDir);
    }
lineFileClose(&lf);
}

void unpackIp(in_addr_t packed, unsigned char unpacked[4])
/* Unpack IP address into 4 bytes. */
{
memcpy(unpacked, &packed, sizeof(unpacked));
}

boolean ipAddressOk(in_addr_t packed, unsigned char *spec)
/* Return TRUE if packed IP address matches spec. */
{
unsigned char unpacked[4], c;
int i;
unpackIp(packed, unpacked);
for (i=0; i<sizeof(unpacked); ++i)
    {
    c = spec[i];
    if (c == 255)
        break;
    if (c != unpacked[i])
        return FALSE;
    }
return TRUE;
}


void startHub(char *machineList)
/* Do hub daemon - set up socket, and loop around on it until we get a quit. */
{
int socketHandle = 0, connectionHandle = 0;
char sig[20], *line, *command;
int sigLen = strlen(paraSig);
char *buf = NULL;

/* Find name and IP address of our machine. */
hubHost = getMachine();
setupDaemonLog(optionVal("log", NULL));
logIt("Starting paraHub on %s\n", hubHost);

/* Set up various lists. */
setupLists();
startMachines(machineList);
assert(sigLen < sizeof(sig));
sig[sigLen] = 0;

/* Start up daemons. */
startSpokes();
startHeartbeat();

/* Set up socket. */
socketHandle = netAcceptingSocket(paraPort, 1100);
if (socketHandle < 0)
    errAbort("Can't set up socket.  Urk!  I'm dead.");

openJobId();

printf("Starting paraHub. Next job ID is %d.\n", nextJobId);

/* Bump up our priority to just shy of real-time. */
nice(-40);
nice(-40);

/* Main event loop. */
for (;;)
    {
    struct sockaddr_in inAddress;
    int len_inet = sizeof(inAddress);
    int connectionHandle = accept(socketHandle, 
    	(struct sockaddr *)&inAddress, &len_inet);
    if (connectionHandle < 0)
        continue;
    if (ipAddressOk(inAddress.sin_addr.s_addr, subnet) || 
    	ipAddressOk(inAddress.sin_addr.s_addr, localHost))
	{
	if (netReadAll(connectionHandle, sig, sigLen) < sigLen || !sameString(sig, paraSig))
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
	     removeJobAcknowledge(line, connectionHandle);
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
	else if (sameWord(command, "listUsers"))
	     listUsers(connectionHandle);
	else if (sameWord(command, "status"))
	     status(connectionHandle);
	else if (sameWord(command, "pstat"))
	     pstat(connectionHandle);
	else if (sameWord(command, "addSpoke"))
	     addSpoke(socketHandle, connectionHandle);
	if (sameWord(command, "quit"))
	     break;
	freez(&buf);
	}
    else
        {
	char ip[4];
	unpackIp(inAddress.sin_addr.s_addr, ip);
	logIt("hub: unauthorized %d.%d.%d.%d\n", 
		ip[0], ip[1], ip[2], ip[3]);
	}
    close(connectionHandle);
    }
endHeartbeat();
killSpokes();
close(socketHandle);
saveJobId();
}

void notGoodSubnet(char *sns)
/* Complain about subnet format. */
{
errAbort("'%s' is not a properly formatted subnet.  Subnets must consist of\n"
         "one to three dot-separated numbers between 0 and 255\n");
}

void fillInSubnet()
/* Parse subnet paramenter if any into subnet variable. */
{
char *sns = optionVal("subnet", NULL);
if (sns == NULL)
    sns = optionVal("subNet", NULL);
if (sns != NULL)
    {
    char *snsCopy = strdup(sns);
    char *words[5];
    int wordCount, i;
    wordCount = chopString(snsCopy, ".", words, ArraySize(words));
    if (wordCount > 3 || wordCount < 1)
        notGoodSubnet(sns);
    for (i=0; i<wordCount; ++i)
	{
	char *s = words[i];
	int x;
	if (!isdigit(s[0]))
	    notGoodSubnet(sns);
	x = atoi(s);
	if (x > 255)
	    notGoodSubnet(sns);
	subnet[i] = x;
	}
    freez(&snsCopy);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc < 2)
    usage();
jobCheckPeriod = optionInt("jobCheckPeriod", jobCheckPeriod);
machineCheckPeriod = optionInt("machineCheckPeriod", machineCheckPeriod);
initialSpokes = optionInt("spokes",  initialSpokes);
fillInSubnet();
startHub(argv[1]);
return 0;
}


