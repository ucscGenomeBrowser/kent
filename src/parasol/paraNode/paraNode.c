/* paraNode - parasol node server. */
#include <signal.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <pwd.h>
#include "common.h"
#include "errabort.h"
#include "dystring.h"
#include "dlist.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "subText.h"
#include "paraLib.h"
#include "net.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort("paraNode - parasol node serve.\n"
         "usage:\n"
	 "    paraNode start\n"
	 "options:\n"
	 "    log=file - file may be 'stdout' to go to console\n"
	 "    hub=host - restrict access to connections from hub\n"
	 "    umask=000 - set umask to run under, default 002\n"
	 "    userPath=bin:bin/i386 User dirs to add to path\n"
	 "    sysPath=/sbin:/local/bin System dirs to add to path\n"
	 "    cpu=N - Number of CPUs to use.  Default 1\n");
}

/* Command line overwriteable variables. */
char *hubName;			/* Name of hub machine, may be NULL. */
int umaskVal = 0002;		/* File creation mask. */
int maxProcs = 1;		/* Number of processers allowed to use. */
char *userPath = "";		/* User stuff to add to path. */
char *sysPath = "";		/* System stuff to add to path. */

/* Other globals. */
char *hostName;			/* Name of this machine. */
in_addr_t hubIp;		/* Hub IP address. */
in_addr_t localIp;		/* localhost IP address. */
int busyProcs = 0;		/* Number of processers in use. */
int socketHandle;		/* Main message queue socket. */
int connectionHandle;		/* A connection accepted. */

struct job
/* Info on one job in this node. */
    {
    struct job *next;	/* Next job. */
    int jobId;		/* Job ID for hub. */
    int pid;		/* Process ID of running job. */
    char *startMessage;	/* Full message that started this job. */
    char *doneMessage;  /* Full message that ended this job if any. */
    struct dlNode *node; /* Node for list this is on. */
    };
struct dlList *jobsRunning;		/* List of currently running jobs. */
struct dlList *jobsFinished;	/* List of recent finished jobs. */

struct job *findJobOnList(struct dlList *list, int jobId)
/* Return job if it's on list, otherwise NULL */
{
struct dlNode *node;
struct job *job;
for (node = list->head; !dlEnd(node); node = node->next)
    {
    job = node->val;
    if (job->jobId == jobId)
        return job;
    }
return NULL;
}

struct job *findRunningJob(int jobId)
/* Return job if it's on running list, otherwise NULL */
{
return findJobOnList(jobsRunning, jobId);
}

struct job *findFinishedJob(int jobId)
/* Return recently finished job or NULL if it doesn't exist */
{
return findJobOnList(jobsFinished, jobId);
}

extern char **environ;	/* The environment strings. */

char **hashToEnviron(struct hash *hash)
/* Create an environ formatted string from hash. */
{
struct dyString *dy = newDyString(512);
struct hashEl *list = hashElListHash(hash), *el;
char **newEnv;
int envCount = slCount(list), i;
AllocArray(newEnv, envCount+1);
for (i=0, el=list; i<envCount; ++i, el = el->next)
    {
    dyStringClear(dy);
    dyStringAppend(dy, el->name);
    dyStringAppend(dy, "=");
    dyStringAppend(dy, el->val);
    newEnv[i] = cloneString(dy->string);
    }
freeDyString(&dy);
return newEnv;
}

struct hash *environToHash(char **env)
/* Put environment into hash. */
{
struct hash *hash = newHash(7);
char *name, *val, *s, *e;
while ((s = *env++) != NULL)
    {
    name = cloneString(s);
    val = strchr(name, '=');
    if (val != NULL)
        {
	*val++ = 0;
	hashAdd(hash, name, cloneString(val));
	}
    freez(&name);
    }
return hash;
}

unsigned randomNumber;	/* Last random number generated. */

void initRandom()
/* Initialize random number generator */
{
/* Set up random number generator with seed depending on host name. */
unsigned seed = hashCrc(hostName)&0xfffff;
srand(seed);
}

void nextRandom()
/* Generate next random number.  This is in
 * executed main thread in response to any message
 * rather than in child threads. */
{
randomNumber = rand();
}

void randomSleep()
/* Put in a random sleep time of 0-4 seconds to help
 * prevent overloading hub with jobDone messages all
 * at once. */
{
int sleepTime = randomNumber%4;
int spinTime = randomNumber%200;
long doneSpin = clock1000() + spinTime;
while (clock1000() < doneSpin)
    ;
if (sleepTime > 0)
    sleep(sleepTime);
}


void hashUpdate(struct hash *hash, char *name, char *val)
/* Update hash with name/val pair.   If name not in hash
 * already, put it in hash, otherwise free up old value
 * associated with name and put new value in it's place. */
{
struct hashEl *hel = hashLookup(hash, name);
val = cloneString(val);
if (hel == NULL)
    hashAdd(hash, name, val);
else
    {
    freez(&hel->val);
    hel->val = val;
    }
}

void changeUid(char *name, char **retDir)
/* Try and change process user (and group) id to that of name. */
{
struct passwd *pw = getpwnam(name);
setgid(pw->pw_gid);
setuid(pw->pw_uid);
if (retDir != NULL)
   *retDir = pw->pw_dir;
}

static int grandChildId = 0;

void termHandler()
/* Handle termination signal. */
{
if (grandChildId != 0)
    {
    kill(-grandChildId, SIGTERM);
    grandChildId = 0;
    sleep(3);
    kill(-grandChildId, SIGKILL);
    }
}

void updatePath(struct hash *hash, char *userPath, 
	char *homeDir, char *sysPath)
/* Prepend userPath and system path to existing path. 
 * Add homeDir in front of all elements of user path. */
{
struct dyString *dy = newDyString(1024);
char *s, *e;
char *oldPath;

/* Go through user path - which is colon separated
 * and prepend homeDir to it. */
userPath = cloneString(userPath);
s = userPath;
for (;;)
    {
    if (s == NULL || s[0] == 0)
        break;
    e = strchr(s, ':');
    if (e != NULL)
       *e++ = 0;
    dyStringAppend(dy, homeDir);
    dyStringAppend(dy, "/");
    dyStringAppend(dy, s);
    dyStringAppend(dy, ":");
    s = e;
    }

/* Add system path next. */
if (sysPath != NULL && sysPath[0] != 0)
    {
    dyStringAppend(dy, sysPath);
    if (lastChar(sysPath) != ':')
	dyStringAppend(dy, ":");
    }

/* Add paths we inherited from root. */
oldPath = hashFindVal(hash, "PATH");
if (oldPath == NULL || oldPath[0] == 0)
    oldPath = "/bin:/usr/bin";
dyStringAppend(dy, oldPath);

hashUpdate(hash, "PATH", dy->string);
freez(&userPath);
dyStringFree(&dy);
}

void execProc(char *managingHost, char *jobIdString, char *reserved,
	char *user, char *dir, char *in, char *out, char *err,
	char *exe, char **params)
/* This routine is the child process of doExec.
 * It spawns a grandchild that actually does the
 * work and waits on it.  It sends message to the
 * main message loop here when done. */
{
if ((grandChildId = fork()) == 0)
    {
    int newStdin, newStdout, newStderr, execErr;
    char *homeDir;

    /* Change to given user and dir. */
    changeUid(user, &homeDir);
    chdir(dir);
    setpgrp();  /* Make childPid the process group leader for any of it's children 
		 * (so we can reap things spawned by user's scripts). */
    umask(umaskVal); 

    /* Update environment. */
        {
	struct hash *hash = environToHash(environ);
	hashUpdate(hash, "JOB_ID", jobIdString);
	hashUpdate(hash, "USER", user);
	hashUpdate(hash, "HOME", homeDir);
	hashUpdate(hash, "HOST", hostName);
	hashUpdate(hash, "PARASOL", "1");
	updatePath(hash, userPath, homeDir, sysPath);
	environ = hashToEnviron(hash);
	freeHashAndVals(&hash);
	}

    /* Redirect standard io.  There has to  be a less
     * cryptic way to do this. Close all open files, then
     * open in/out/err in order so they have descriptors
     * 0,1,2. */
    logClose();
    close(socketHandle);
    close(connectionHandle);
    close(0);
    close(1);
    close(2);
    open(in, O_RDONLY);
    open(out, O_WRONLY | O_CREAT, 0666);
    open(err, O_WRONLY | O_CREAT, 0666);

    
    randomSleep();	/* Sleep a random bit before executing this thing
                         * to help spread out i/o when a big batch of jobs
			 * hit idle cluster */
    if ((execErr = execvp(exe, params)) < 0)
	{
	perror("execvp");
	warn("Error execvp'ing %s %s", exe, params);
	}
    exit(execErr);
    }
else
    {
    /* Wait on executed job and send jobID and status back to 
     * main process. */
    int status;
    int cid;
    int sd;

    signal(SIGTERM, termHandler);
    cid = wait(&status);
    sd = netConnect("localhost", paraPort);
    if (sd >= 0)
        {
	char buf[256];
	struct tms tms;

	times(&tms);
	snprintf(buf, sizeof(buf), 
		"jobDone %s %s %d %lu %lu", managingHost, jobIdString, 
		status, tms.tms_cutime, tms.tms_cstime);
	sendWithSig(sd, buf);
	close(sd);
	}
    }
}

void clearZombies()
/* Clear any zombie processes */
{
int stat;
for (;;)
    {
    if (waitpid(-1, &stat, WNOHANG) <= 0)
        break;
    }
}

void tellManagerJobIsDone(char *managingHost, char *jobIdString, char *line)
/* Try and send message to host saying job is done. */
{
int sd = -1;
int i;

/* Keep trying every 5 seconds for a minute to connect.... */
for (i=0; i<12; ++i)
    {
    if ((sd = netConnect(managingHost, paraPort)) >= 0)
        break;
    sleep(5);
    }

if (sd >= 0)
    {
    char buf[256];
    snprintf(buf, sizeof(buf), "jobDone %s %s", jobIdString, line);
    sendWithSig(sd, buf);
    close(sd);
    }
}

void jobFree(struct job **pJob)
/* Free up memory associated with job */
{
struct job *job = *pJob;
if (job != NULL)
    {
    freeMem(job->startMessage);
    freeMem(job->doneMessage);
    freeMem(job->node);
    freez(pJob);
    }
}

void jobDone(char *line)
/* Handle job-done message - forward it to managing host. */
{
char *managingHost = nextWord(&line);
char *jobIdString = nextWord(&line);

clearZombies();
if (jobIdString != NULL && line != NULL && line[0] != 0)
    {
    /* Remove job from list running list and put on recently finished list. */
    struct job *job = findRunningJob(atoi(jobIdString));
    if (job != NULL)
        {
	job->doneMessage = cloneString(line);
	dlRemove(job->node);
	if (dlCount(jobsFinished) >= 4*maxProcs)
	    {
	    struct dlNode *node = dlPopTail(jobsFinished);
	    struct job *oldJob = node->val;
	    jobFree(&oldJob);
	    }
	dlAddHead(jobsFinished, job->node);
	--busyProcs;
	}

    /* Tell managing host that job is done. */
    if (fork() == 0)
	{
	tellManagerJobIsDone(managingHost, jobIdString, line);
	exit(0);
	}
    }
}

void doCheck(char *line)
/* Send back check result - either a check in message or
 * jobDone. */
{
char *managingHost = nextWord(&line);
char *jobIdString = nextWord(&line);
if (jobIdString != NULL)
    {
    int jobId = atoi(jobIdString);
    struct job *job = findRunningJob(jobId);
    int sd = netConnect(managingHost, paraPort);
    if (sd >= 0)
	{
	char buf[256];
	if (job != NULL)
	    snprintf(buf, sizeof(buf), "checkIn %s %s running", hostName, jobIdString);
	else
	    {
	    struct job *job = findFinishedJob(jobId);
	    if (job == NULL)
		snprintf(buf, sizeof(buf), "checkIn %s %s free", hostName, jobIdString);
	    else
		snprintf(buf, sizeof(buf), "jobDone %s %s", jobIdString, job->doneMessage);
	    }
	sendWithSig(sd, buf);
	close(sd);
	}
    }
}

void doResurrect(char *line)
/* Send back I'm alive message */
{
char *managingHost = nextWord(&line);
if (managingHost != NULL)
    {
    int sd = netConnect(managingHost, paraPort);
    char buf[256];
    snprintf(buf, sizeof(buf), "alive %s", hostName);
    sendWithSig(sd, buf);
    close(sd);
    }
}

void doRun(char *line)
/* Execute command. */
{
static char *args[1024];
int argCount;
nextRandom();
if (line == NULL)
    warn("Executing nothing...");
else if (busyProcs < maxProcs)
    {
    char *exe;
    int childPid;
    char *jobMessage = cloneString(line);
    struct runJobMessage rjm;

    if (!parseRunJobMessage(line, &rjm))
	{
	freez(&jobMessage);
	return;
	}
    argCount = chopLine(rjm.command, args);
    if (argCount >= ArraySize(args))
	warn("Too many arguments to run");
    else
	{
	args[argCount] = NULL;
	if ((childPid = fork()) == 0)
	    {
	    /* Do JOB_ID substitutions */
	    struct subText *st = subTextNew("$JOB_ID", rjm.jobIdString);
	    int i;
	    rjm.in = subTextString(st, rjm.in);
	    rjm.out = subTextString(st, rjm.out);
	    rjm.err = subTextString(st, rjm.err);
	    for (i=0; i<argCount; ++i)
	        args[i] = subTextString(st, args[i]);

	    execProc(rjm.managingHost, rjm.jobIdString, rjm.reserved,
		rjm.user, rjm.dir, rjm.in, rjm.out, rjm.err, args[0], args);
	    exit(0);
	    }
	else
	    {
	    struct job *job;
	    AllocVar(job);
	    job->jobId = atoi(rjm.jobIdString);
	    job->pid = childPid;
	    job->startMessage = jobMessage;
	    job->node = dlAddValTail(jobsRunning, job);
	    ++busyProcs;
	    }
	}
     }
else
    {
    warn("Trying to run when busy.");
    }
}

void doFetch(char *line)
/* Fetch a file. */
{
char *user = nextWord(&line);
char *fileName = nextWord(&line);
if (fileName != NULL)
    {
    if (fork() == 0)
        {
	FILE *f = fopen(fileName, "r");
	if (f == NULL)
	    warn("Couldn't open %s", fileName);
	else
	    {
	    char buf[4*1024];
	    int size;
	    while ((size = fread(buf, 1, sizeof(buf), f)) > 0)
		write(connectionHandle, buf, size);
	    fclose(f);
	    }
	exit(0);
	}
    }
}

void doKill(char *line)
/* Kill a specific job. */
{
char *jobIdString = nextWord(&line);
int jobId = atoi(jobIdString);
if (jobId != 0)
    {
    struct job *job = findRunningJob(jobId);
    if (job != NULL)
        {
	kill(job->pid, SIGTERM);
	dlRemove(job->node);
	jobFree(&job);
	--busyProcs;
	}
    }
else
    {
    warn("Nothing to kill\n");
    }
}


void doStatus()
/* Report status. */
{
struct dyString *dy = newDyString(256);
dyStringPrintf(dy, "%d of %d CPUs busy", busyProcs, maxProcs);
if (busyProcs > 0)
    {
    struct dlNode *node;
    dyStringPrintf(dy, ". Jobs:");
    for (node = jobsRunning->head; !dlEnd(node); node = node->next)
        {
	struct job *job = node->val;
        dyStringPrintf(dy, " %d", job->jobId);
	}
    }
write(connectionHandle, dy->string, dy->stringSize);
dyStringFree(&dy);
}

void listJobs()
/* Report jobs running and recently finished. */
{
char buf[64];
struct job *job;
struct dlNode *node;

sprintf(buf, "%d running", dlCount(jobsRunning));
netSendLongString(connectionHandle, buf);
for (node = jobsRunning->head; !dlEnd(node); node=node->next)
    {
    job = node->val;
    netSendLongString(connectionHandle, job->startMessage);
    }
sprintf(buf, "%d recent", dlCount(jobsFinished));
netSendLongString(connectionHandle, buf);
for (node = jobsFinished->head; !dlEnd(node); node=node->next)
    {
    job = node->val;
    netSendLongString(connectionHandle, job->startMessage);
    netSendLongString(connectionHandle, job->doneMessage);
    }
}

void paraNode()
/* paraNode - a net server. */
{
char *buf = NULL, *line;
int fromLen, readSize;
char *command;
char signature[20];
int sigLen = strlen(paraSig);

/* We have to know who we are... */
hostName = getMachine();
initRandom();

/* Precompute some signature stuff. */
assert(sigLen < sizeof(signature));

/* Make job lists. */
jobsRunning = newDlList();
jobsFinished = newDlList();

/* Set up socket and self to listen to it. */
socketHandle = netAcceptingSocket(paraPort, maxProcs*4);
if (socketHandle < 0)
    errAbort("I'm dead without my socket, sorry");

/* Event loop. */
for (;;)
    {
    /* Get next incoming connection and optionally check to make
     * sure that it's from a host we trust, and check signature
     * on first bit of incoming data. */
    struct sockaddr_in hubAddress;
    int len_inet = sizeof(hubAddress);
    connectionHandle = accept(socketHandle, 
    	(struct sockaddr *)&hubAddress, &len_inet);
    if (connectionHandle >= 0)
	{
	findNow();
	if (hubName == NULL || hubAddress.sin_addr.s_addr == hubIp
	    || hubAddress.sin_addr.s_addr == localIp)
	    {
	    if (netReadAll(connectionHandle, signature, sigLen) == sigLen)
		{
		signature[sigLen] = 0;
		if (sameString(paraSig, signature))
		    {
		    /* Host and signature look ok,  read a string and
		     * parse out first word as command. */
		    line = buf = netGetLongString(connectionHandle);
		    if (line != NULL)
			{
			logIt("node  %s: %s\n", hostName, line);
			command = nextWord(&line);
			if (command != NULL)
			    {
			    if (sameString("quit", command))
				break;
			    else if (sameString("run", command))
				doRun(line);
			    else if (sameString("jobDone", command))
				jobDone(line);
			    else if (sameString("status", command))
				doStatus();
			    else if (sameString("kill", command))
				doKill(line);
			    else if (sameString("check", command))
				doCheck(line);
			    else if (sameString("resurrect", command))
				doResurrect(line);
			    else if (sameString("listJobs", command))
			        listJobs();
			    else if (sameString("fetch", command))
			        doFetch(line);
			    }
			freez(&buf);
			}
		    logIt("node  %s: done command\n", hostName, line);
		    }
		}
	    }
	if (connectionHandle != 0)
	    {
	    close(connectionHandle);
	    connectionHandle = 0;
	    }
	}
    }
close(socketHandle);
socketHandle = 0;
}

void paraFork()
/* Fork off real daemon and exit.  This effectively
 * removes dependence of paraNode daemon on terminal. 
 * Set up log file if any here as well. */
{
char *log = optionVal("log", NULL);

/* Close standard file handles. */
close(0);
if (log == NULL || !sameString(log, "stdout"))
    close(1);
close(2);

if (fork() == 0)
    {
    /* Set up log handler. */
    setupDaemonLog(log);

    /* Execute daemon. */
    paraNode();
    }
}

in_addr_t lookupIp(char *host)
/* Return IP address of host. */
{
struct hostent *hostent = gethostbyname(host);
struct sockaddr_in address;	
if (hostent == NULL)
errAbort("Couldn't find hub %s", host);
memcpy(&address.sin_addr.s_addr, hostent->h_addr_list[0], 
sizeof(address.sin_addr.s_addr));
return address.sin_addr.s_addr;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
maxProcs = optionInt("cpu", 1);
umaskVal = optionInt("umask", 0002);
userPath = optionVal("userPath", userPath);
sysPath = optionVal("sysPath", sysPath);

/* Look up IP addresses. */
localIp = lookupIp("localhost");
hubName = optionVal("hub", NULL);
if (hubName != NULL)
    hubIp = lookupIp(hubName);

paraFork();
return 0;
}


