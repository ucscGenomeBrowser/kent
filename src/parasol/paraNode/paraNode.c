/* paraNode - parasol node server. */
#include <signal.h>
#include <sys/wait.h>
#include <sys/times.h>
#include "common.h"
#include "errabort.h"
#include "dystring.h"
#include "dlist.h"
#include "hash.h"
#include "options.h"
#include "subText.h"
#include "paraLib.h"
#include "net.h"

char *hostName;			/* Name of this host. */
int busyProcs = 0;		/* Number of processers in use. */
int maxProcs = 1;		/* Number of processers allowed to use. */
int socketHandle;		/* Main message queue socket. */
int connectionHandle;		/* A connection accepted. */

struct job
/* Info on one job in this node. */
    {
    struct job *next;	/* Next job. */
    int jobId;		/* Job ID for hub. */
    int pid;			/* Process ID of running job. */
    };
struct job *runningJobs;

struct job *findJob(int jobId)
/* Return job if it's on list, otherwise NULL */
{
struct job *job;
for (job = runningJobs; job != NULL; job = job->next)
    if (job->jobId == jobId)
        return job;
return NULL;
}

void usage()
/* Explain usage and exit. */
{
errAbort("paraNode - parasol node serve.\n"
         "usage:\n"
	 "    paraNode start\n"
	 "options:\n"
	 "    log=file - file may be 'stdout' to go to console\n"
	 "    cpu=N - Number of CPUs to use.  Default 1\n");
}

extern char **environ;	/* The environment strings. */

int countEnv(char **env)
/* Count elements in environment type string. */
{
char *s;
int count = 0;
while ((s = *env++) != NULL)
    ++count;
return count;
}

char **envExpand(int extra)
/* Make environ have extra slots. Return first new slot. */
{
int oldSize = countEnv(environ);
int newSize = oldSize + extra + 1;
char **newEnv;
AllocArray(newEnv, newSize);
memcpy(newEnv, environ, oldSize * sizeof(newEnv[0]));
environ = newEnv;
return environ + oldSize;
}

char *envPair(char *name, char *val)
/* Return name=val in a freshly allocated string. */
{
int size = strlen(name) + strlen(val) + 2;  /* Include '=' and zero tag */
char *pair = needMem(size);
snprintf(pair, size, "%s=%s", name, val);
return pair;
}

void addEnv(char *name, char *val)
/* Add name=value pair to environment. */
{
char **env = envExpand(1);
*env = envPair(name, val);
}

void execProc(char *managingHost, char *jobIdString, char *reserved,
	char *user, char *dir, char *in, char *out, char *err,
	char *exe, char **params)
/* This routine is the child process of doExec.
 * It spawns a grandchild that actually does the
 * work and waits on it.  It sends message to the
 * main message loop here when done. */
{
if (fork() == 0)
    {
    int newStdin, newStdout, newStderr, execErr;

    /* Change to given dir. */
    chdir(dir);

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

    /* Add jobId to environment. */
    addEnv("JOB_ID", jobIdString);

    if ((execErr = execvp(exe, params)) < 0)
	{
	perror("");
	warn("Error execlp'ing %s %s", exe, params);
	}
    exit(execErr);
    }
else
    {
    /* Wait on executed job and send jobID and status back to whoever
     * started the job. */
    int status;
    int sd;

    wait(&status);
    sd = netConnect(hostName, paraPort);
    if (sd > 0)
        {
	char buf[256];
	struct tms tms;

	times(&tms);
	snprintf(buf, sizeof(buf), 
		"jobDone %s %s %d %lu %lu", managingHost, jobIdString, 
		status, tms.tms_cutime, tms.tms_cstime);
	write(sd, paraSig, strlen(paraSig));
	netSendLongString(sd, buf);
	close(sd);
	}
    exit(0);
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

void jobDone(char *line)
/* Handle job-done message - forward it to managing host. */
{
char *managingHost = nextWord(&line);
char *jobIdString = nextWord(&line);

clearZombies();
if (jobIdString != NULL && line != NULL && line[0] != 0)
    {
    int sd;

    /* Remove job from list. */
    struct job *job = findJob(atoi(jobIdString));
    if (job != NULL)
        {
	slRemoveEl(&runningJobs, job);
	freez(&job);
	--busyProcs;
	}

    /* Tell managing host that job is done. */
    sd = netConnect(managingHost, paraPort);
    if (sd > 0)
	{
	char buf[256];
	snprintf(buf, sizeof(buf), "jobDone %s %s", jobIdString, line);
	write(sd, paraSig, strlen(paraSig));
	netSendLongString(sd, buf);
	}
    }
}

void doCheck(char *line)
/* Send back check result */
{
char *managingHost = nextWord(&line);
char *jobIdString = nextWord(&line);
if (jobIdString != NULL)
    {
    int jobId = atoi(jobIdString);
    struct job *job = findJob(jobId);
    int sd = netConnect(managingHost, paraPort);
    if (sd > 0)
	{
	char *status = (job != NULL  ? "busy" : "free");
	char buf[256];
	snprintf(buf, sizeof(buf), "checkIn %s %s %s", hostName, jobIdString, status);
	write(sd, paraSig, strlen(paraSig));
	netSendLongString(sd, buf);
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
    write(sd, paraSig, strlen(paraSig));
    netSendLongString(sd, buf);
    close(sd);
    }
}

void doRun(char *line)
/* Execute command. */
{
static char *args[1024];
char *managingHost, *jobIdString, *reserved, *user, *dir, *in, 
	*out, *err, *cmd;
int argCount;
if (line == NULL)
    warn("Executing nothing...");
else if (busyProcs < maxProcs)
    {
    char *exe;
    int childPid;

    managingHost = nextWord(&line);
    jobIdString = nextWord(&line);
    reserved = nextWord(&line);
    user = nextWord(&line);
    dir = nextWord(&line);
    in = nextWord(&line);
    out = nextWord(&line);
    err = nextWord(&line);

    if (line == NULL || (argCount = chopLine(line, args)) < 1)
	warn("Not enough parameters to run");
    else if (argCount >= ArraySize(args))
	warn("Too many arguments to run");
    else
	{
	args[argCount] = NULL;
	if ((childPid = fork()) == 0)
	    {
	    /* Do JOB_ID substitutions */
	    struct subText *st = subTextNew("$JOB_ID", jobIdString);
	    int i;
	    in = subTextString(st, in);
	    out = subTextString(st, out);
	    err = subTextString(st, err);
	    for (i=0; i<argCount; ++i)
	        args[i] = subTextString(st, args[i]);

	    execProc(managingHost, jobIdString, reserved,
		user, dir, in, out, err, args[0], args);
	    }
	else
	    {
	    struct job *job;
	    AllocVar(job);
	    job->jobId = atoi(jobIdString);
	    job->pid = childPid;
	    slAddHead(&runningJobs, job);
	    ++busyProcs;
	    }
	}
     }
else
    {
    warn("Trying to run when busy.");
    }
}

void doKill(char *line)
/* Kill current job if any. */
{
char *jobIdString = nextWord(&line);
int jobId = atoi(jobIdString);
if (jobId != 0)
    {
    struct job *job = findJob(jobId);
    if (job != NULL)
        {
	kill(job->pid, SIGTERM);
	slRemoveEl(&runningJobs, job);
	freez(&job);
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
write(connectionHandle, dy->string, dy->stringSize);
dyStringFree(&dy);
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
hostName = getHost();

/* Precompute some signature stuff. */
assert(sigLen < sizeof(signature));
signature[sigLen] = 0;

/* Set up socket and self to listen to it. */
socketHandle = netAcceptingSocket(paraPort, 10);
if (socketHandle < 0)
    errAbort("I'm dead without my socket, sorry");

/* Event loop. */
for (;;)
    {
    connectionHandle = netAccept(socketHandle);
    if (connectionHandle >= 0)
	{
	if (netReadAll(connectionHandle, signature, sigLen) == sigLen)
	    {
	    if (sameString(paraSig, signature))
		{
		line = buf = netGetLongString(connectionHandle);
		if (line != NULL)
		    {
		    logIt("node  %s: %s\n", hostName, line);
		    command = nextWord(&line);
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
		    freez(&buf);
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

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
maxProcs = optionInt("cpu", 1);
paraFork();
return 0;
}


