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

void usage()
/* Explain usage and exit. */
{
errAbort("paraHub - parasol hub server.\n"
         "usage:\n"
	 "    paraHub machineList jobList\n");
}

struct machine
/* A machine for running jobs on. */
    {
    struct machine *next;	/* Next in master list. */
    char *name;                 /* Name.  Not alloced here. */
    int socket;                 /* Socket connection */
    struct sockaddr_in sai;	/* Other socket address info. */
    struct dlNode *jobNode;     /* Current job if any. */
    struct dlNode *machNode;    /* List node of machine. */
    int supPid;			/* Process ID of supervisor. */
    int errCount;               /* Number of errors. */
    };
struct machine *machineList; /* List of all machines. */
struct dlList *readyList;    /* List of machines ready for jobs. */
struct dlList *busyList;     /* List of machines running jobs. */
struct dlList *downList;     /* List of machines that aren't running. */

struct job
/* A job .*/
    {
    struct job *next;		/* Next in master list. */
    int id;			/* Uniq	job id. */
    char *exe;                  /* Executable name. */
    char *params;               /* Remaining parameters. */
    time_t startTime;           /* Start time in seconds past 1972 */
    time_t endTime;             /* End time in seconds past 1972 */
    struct batch *batch;        /* Batch this job is in. */
    };
struct dlList *pendingList;     /* Jobs still to do. */
struct dlList *runningList;     /* Jobs that are running. */
struct dlList *finishedList;    /* Finished jobs. */

struct batch
/* A batch of jobs. */
    {
    struct batch *next;        /* Next in master list. */
    int id;                    /* Uniq batch id. */
    char *name;                /* Name of batch. */
    time_t submitTime;         /* Time submitted. */
    time_t endTime;            /* Time last job finished. */
    struct job *jobList;       /* List of all jobs. */
    struct user *user;         /* Batch user. */
    };

struct user
/* A user. */
    {
    struct user *next;	        /* Next in list. */
    char *name;			/* User name. */
    struct batch *batchList;	/* List of all batches submitted. */
    };
struct user *userList = NULL;

boolean getSocket(struct machine *mach)
/* Try and fill in socket of mach. */
{
int sd;
struct sockaddr_in sai;
int fromlen;
struct hostent *host;
char *hostName = mach->name;

/* Set up connection. */
host = gethostbyname(hostName);
if (host == NULL)
    {
    herror("");
    warn("Couldn't find host %s.  h_errno %d", hostName, h_errno);
    return FALSE;
    }
sai.sin_family = AF_INET;
sai.sin_port = htons(paraPort);
memcpy(&sai.sin_addr.s_addr, host->h_addr_list[0], sizeof(sai.sin_addr.s_addr));
if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
    warn("Couldn't make socket to %s", hostName);
    return FALSE;
    }
if (connect(sd, &sai, sizeof(sai)) == -1)
    {
    warn("Couldn't connect to %s", hostName);
    close(sd);
    return FALSE;
    }
mach->socket = sd;
return TRUE;
}

void closeSocket(struct machine *mach)
/* Close machine's open socket if any. */
{
if (mach->socket != 0)
    close(mach->socket);
mach->socket = 0;
}

void setupMachines(char *machineList)
/* Make up data structure to keep track of each machine. 
 * Try to get sockets on all of them. */
{
int mCount;
char *mBuf, **mNames, *name;
int i;
struct machine *mach;
struct dlNode *node;

readyList = newDlList();
busyList = newDlList();
downList = newDlList();
readAllWords(machineList, &mNames, &mCount, &mBuf);
for (i=0; i<mCount; ++i)
    {
    name = mNames[i];
    uglyf("Setting up %s\n", name);
    AllocVar(mach);
    mach->name = name;
    mach->machNode = AllocVar(node);
    node->val = mach;
    dlAddTail(readyList, node);
    }
freeMem(mNames);
// Don't free mBuf - used in mach. 
}

void cleanupMachines()
/* Close sockets, free machines. */
{
struct machine *mach;
int sock;
char buf[128];

uglyf("cleanupMachines\n");
sprintf(buf, "%s kill", paraSig);
for (mach = machineList; mach != NULL; mach = mach->next)
    {
    if ((sock = mach->socket) != 0)
	{
	write(sock, buf, strlen(buf));
	}
    closeSocket(mach);
    }
freeDlList(&readyList);
freeDlList(&busyList);
freeDlList(&downList);
slFreeList(&machineList);
}

void cleanupAndExit()
/* Close sockets, etc. */
{
cleanupMachines();
exit(-1);
}

void setupJobLists()
/* Create job lists. */
{
pendingList = newDlList();     /* Jobs still to do. */
runningList = newDlList();     /* Jobs that are running. */
finishedList = newDlList();    /* Finished jobs. */
}

struct user *findUser(char *name)
/* Find user given name.  If doesn't exist create it.  */
{
struct user *user;
for (user = userList; user != NULL; user = user->next)
    {
    if (sameString(name, user->name))
	return user;
    }
AllocVar(user);
user->name = cloneString(name);
slAddHead(&userList, user);
return user;
}

boolean addBatch(char *userName, char *batchName)
/* Add a batch job from user to queues. */
{
struct job *job;
struct batch *batch;
struct user *user;
struct lineFile *lf;
char *line, *command;
int lineSize;
static int batchId;
static int jobId;

if (!fileExists(batchName))
    {
    warn("%s doesn't exist", batchName);
    return FALSE;
    }
user = findUser(userName);
lf = lineFileOpen(batchName, TRUE);
AllocVar(batch);
batch->id = ++batchId;
batch->name = cloneString(batchName);
batch->submitTime = time(NULL);
batch->user = user;
slAddHead(&user->batchList, batch);

while (lineFileNext(lf, &line, &lineSize))
    {
    line = skipLeadingSpaces(line);
    if (line[0] == 0 || line[0] == '#')
	continue;
    uglyf("job %s\n", line);
    AllocVar(job);
    line = cloneString(line);
    job->id = ++jobId;
    job->exe = nextWord(&line);
    job->params = line;
    job->batch = batch;
    slAddHead(&batch->jobList, job);
    dlAddValTail(pendingList, job);
    }
lineFileClose(&lf);
slReverse(&batch->jobList);
}

void recycleMachine(int pid)
/* Go through running job list to find one that
 * has supPid of pid.  Move job to finished list
 * and machine to ready list. */
{
struct dlNode *node;
struct job *job;
struct machine *mach;

for (node = busyList->head; node->next != NULL; node = node->next)
    {
    mach = node->val;
    if (mach->supPid == pid)
	{
	dlRemove(node);
	dlAddTail(readyList, node);
	node = mach->jobNode;
	mach->jobNode = NULL;
	job = node->val;
	uglyf("Recycling %s after %s %s\n", mach->name, job->exe, job->params);
	dlRemove(node);
	dlAddTail(finishedList, node);
	mach->supPid = 0;
	return;
	}
    }
warn("Couldn't recycle %d", pid);
}

boolean startJob(struct machine *mach, struct job *job, struct dlNode *jobNode)
/* Fork off process to launch and monitor job. */
{
int child;
struct dyString *dy;

if (!getSocket(mach))
    return FALSE;
dy = newDyString(256);
dyStringAppend(dy, paraSig);
dyStringPrintf(dy, "exec %s", job->exe);
if (job->params != NULL)
    dyStringPrintf(dy, " %s", job->params);

/* Issue command. */
uglyf("%s %s\n", mach->name, dy->string);
if (write(mach->socket, dy->string, dy->stringSize) < 0)
    {
    perror("");
    warn("Couldn't write to %s", mach->name);
    return FALSE;
    }
mach->jobNode = jobNode;
if ((child = fork()) == 0)
    {
    /* Create command for paraNode. */
    char asciiStatus[16];
    int status;
    /* Wait and read result. */
    read(mach->socket, asciiStatus, sizeof(asciiStatus));
    status = atoi(asciiStatus);
    exit(status);
    }
closeSocket(mach);
mach->supPid = child;
freeDyString(&dy);
return TRUE;
}

void doAllJobs()
{
int pid, status;

while (!dlEmpty(pendingList) || !dlEmpty(runningList))
    {
    if (dlEmpty(busyList) && dlEmpty(readyList))
	{
	errAbort("No working machines in host list");
	}
    while (!dlEmpty(pendingList) && !dlEmpty(readyList))
	{
	struct dlNode *machNode, *jobNode;

	machNode = dlPopHead(readyList);
	jobNode = dlPopHead(pendingList);
	if (startJob(machNode->val, jobNode->val, jobNode))
	    {
	    dlAddTail(busyList, machNode);
	    dlAddTail(runningList, jobNode);
	    }
	else
	    {
	    struct machine *mach = machNode->val;
	    dlAddHead(pendingList, jobNode);
	    if (++mach->errCount >= 3)
		dlAddTail(downList, machNode);
	    else
		dlAddTail(readyList, machNode);
	    }
	}
    pid = wait(&status);
    if (pid != -1)
	recycleMachine(pid);
    }
}

void paraHub(char *machineList, char *jobList)
/* Connect with hosts on host list.  Have them 
 * execute jobs on jobList. */
{
setupMachines(machineList);
signal(SIGINT,cleanupAndExit);
signal(SIGHUP,SIG_IGN);
setupJobLists();
addBatch(getlogin(), jobList);
if (0)
    {
    struct dlNode *node;
    while (!dlEmpty(pendingList))
	{
	struct job *job;
	node = dlPopHead(pendingList);
	job = node->val;
	dlAddTail(finishedList, node);
	uglyf("-- %s %s\n", job->exe, job->params);
	}
    }
doAllJobs();
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
paraHub(argv[1], argv[2]);
}


