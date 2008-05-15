/* paraNode - parasol node server. */

#include "paraCommon.h"
#include <grp.h>
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
#include "rudp.h"
#include "paraMessage.h"
#include "internet.h"
#include "log.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"logFacility", OPTION_STRING},
    {"log", OPTION_STRING},
    {"debug", OPTION_BOOLEAN},
    {"hub", OPTION_STRING},
    {"umask", OPTION_INT},
    {"userPath", OPTION_STRING},
    {"sysPath", OPTION_STRING},
    {"randomDelay", OPTION_INT},
    {"cpu", OPTION_INT},
    {"localhost", OPTION_STRING},
    {NULL, 0}
};

char *version = PARA_VERSION;   /* Version number. */

void usage()
/* Explain usage and exit. */
{
errAbort("paraNode - version %s\n"
	 "Parasol node server.\n"
         "usage:\n"
	 "    paraNode start\n"
	 "options:\n"
	 "    -logFacility=facility  Log to the specified syslog facility - default local0.\n"
         "    -log=file  Log to file instead of syslog.\n"
         "    -debug  Don't daemonize\n"
	 "    -hub=host  Restrict access to connections from hub.\n"
	 "    -umask=000  Set umask to run under - default 002.\n"
	 "    -userPath=bin:bin/i386  User dirs to add to path.\n"
	 "    -sysPath=/sbin:/local/bin  System dirs to add to path.\n"
	 "    -randomDelay=N  Up to this many milliseconds of random delay before\n"
	 "        starting a job.  This is mostly to avoid swamping NFS with\n"
	 "        file opens when loading up an idle cluster.  Also it limits\n"
	 "        the impact on the hub of very short jobs. Default 5000.\n"
	 "    -cpu=N  Number of CPUs to use - default 1.\n"
	, version
	);
}

static char const rcsid[] = "$Id: paraNode.c,v 1.80 2008/05/15 00:42:27 galt Exp $";

/* Command line overwriteable variables. */
char *hubName;			/* Name of hub machine, may be NULL. */
int umaskVal = 0002;		/* File creation mask. */
int maxProcs = 1;		/* Number of processers allowed to use. */
char *userPath = "";		/* User stuff to add to path. */
char *sysPath = "";		/* System stuff to add to path. */
int randomDelay = 5000;		/* How much to delay job startup. */

/* Other globals. */
char *hostName;			/* Name of this machine. */
in_addr_t hubIp;		/* Hub IP address. */
in_addr_t localIp;		/* localhost IP address. */
int busyProcs = 0;		/* Number of processers in use. */
struct rudp *mainRudp;		/* Rudp wrapper around main socket. */ 
struct paraMessage pmIn;	/* Input message */
double ticksToHundreths;	/* Conversion factor from system ticks
                                 * to 100ths of second. */

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
hashElFreeList(&list);
return newEnv;
}

struct hash *environToHash(char **env)
/* Put environment into hash. */
{
struct hash *hash = newHash(7);
char *name, *val, *s;
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
/* Put in a random sleep time of 0-5 seconds to help
 * prevent overloading hub with jobDone messages all
 * at once, and file server with file open requests
 * all at once. */
{
if (randomDelay > 0)
    {
    int sleepTime = randomNumber%randomDelay;
    sleep1000(sleepTime);
    }
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
/* Try and change process user (and groups) id to that of name. if 
 * we are root.  Get home dir no matter what. */
{
struct passwd *pw = getpwnam(name);
if (pw == NULL)
    errnoAbort("can't obtain user passwd entry for user %s", name);
if (geteuid() == 0)
    {
    if (initgroups(name, pw->pw_gid) < 0)
        errnoAbort("initgroups failed");
    if (setgid(pw->pw_gid) < 0)
        errnoAbort("setgid failed");
    if (setuid(pw->pw_uid) < 0)
        errnoAbort("setuid failed");
    }
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
    // sleep(3);
    // kill(-grandChildId, SIGKILL);
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

void getTicksToHundreths()
/* Return number of hundreths of seconds per system tick.
 * It used to be CLK_TCK would work for this, but
 * under recent Linux's it doesn't. */
{
#ifdef CLK_TCK
    ticksToHundreths = 100.0/CLK_TCK;
#else
    ticksToHundreths = 100.0/sysconf(_SC_CLK_TCK);
#endif /* CLK_TCK */
}

void setupProcStdio(char *in, char *out, char *err)
/* setup stdio for process (after fork) */
{
int newStdin, newStdout, newStderr;

close(mainRudp->socket);

/* do stderr first */
newStderr = open(err, O_WRONLY | O_CREAT, 0666);
if (newStderr < 0)
    errnoAbort("can't open job stderr file %s", err);
if (dup2(newStderr, STDERR_FILENO) < 0)
    errnoAbort("can't dup2 stderr");

newStdin = open(in, O_RDONLY);
if (newStdin < 0)
    errnoAbort("can't open job stdin file %s", in);
if (dup2(newStdin, STDIN_FILENO) < 0)
    errnoAbort("can't dup2 stdin");

newStdout = open(out, O_WRONLY | O_CREAT, 0666);
if (newStdout < 0)
    errnoAbort("can't open job stdout file %s", out);
if (dup2(newStdout, STDOUT_FILENO) < 0)
    errnoAbort("can't dup2 stdout");
}

void execProc(char *managingHost, char *jobIdString, char *reserved,
	char *user, char *dir, char *in, char *out, char *err,
	char *exe, char **params)
/* This routine is the child process of doExec.
 * It spawns a grandchild that actually does the
 * work and waits on it.  It sends message to the
 * main message loop here when done. */
{
if ((grandChildId = forkOrDie()) == 0)
    {
    char *homeDir = "";

    /* Change to given user (if root) */
    changeUid(user, &homeDir);

    /* create output files just after becoming user so that errors in the rest
     * of this proc will go to the err file and be available via para
     * problems */
    setupProcStdio(in, out, err);

    if (chdir(dir) < 0)
        errnoAbort("can't chdir to %s", dir);
    setsid();
    // setpgid(0,0);
    umask(umaskVal); 

    /* Update environment. */
        {
	struct hash *hash = environToHash(environ);
	hashUpdate(hash, "JOB_ID", jobIdString);
	hashUpdate(hash, "USER", user);
	hashUpdate(hash, "HOME", homeDir);
	hashUpdate(hash, "HOST", hostName);
	hashUpdate(hash, "PARASOL", "7");
	updatePath(hash, userPath, homeDir, sysPath);
	environ = hashToEnviron(hash);
	freeHashAndVals(&hash);
	}

    randomSleep();	/* Sleep a random bit before executing this thing
                         * to help spread out i/o when a big batch of jobs
			 * hit idle cluster */
    execvp(exe, params);
    errnoAbort("execvp'ing %s", exe);
    }
else
    {
    /* Wait on executed job and send jobID and status back to 
     * main process. */
    int status = -1;
    int cid;
    struct paraMessage pm;
    struct rudp *ru = NULL;
    struct tms tms;
    unsigned long uTime = 0;
    unsigned long sTime = 0;

    if (grandChildId >= 0)
	{
	signal(SIGTERM, termHandler);
	cid = waitpid(grandChildId, &status, 0);
        if (cid < 0)
            errnoAbort("wait on grandchild failed");
	times(&tms);
	uTime = ticksToHundreths*tms.tms_cutime;
	sTime = ticksToHundreths*tms.tms_cstime;
	}
    ru = rudpOpen();
    if (ru != NULL)
	{
	ru->maxRetries = 20;
	pmInit(&pm, localIp, paraNodePort);
	pmPrintf(&pm, "jobDone %s %s %d %lu %lu", managingHost, 
	    jobIdString, status, uTime, sTime);
	pmSend(&pm, ru);
	rudpClose(&ru);
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

in_addr_t lookupIp(char *host)
/* Return IP address of host. */
{
static char *lastHost = NULL;
static in_addr_t lastAddress;

if (lastHost != NULL && sameString(lastHost, host))
    return lastAddress;
freez(&lastHost);
lastHost = cloneString(host);
lastAddress = internetHostIp(host);
return lastAddress;
}


void tellManagerJobIsDone(char *managingHost, char *jobIdString, char *line)
/* Try and send message to host saying job is done. */
{
struct paraMessage pm;
bits32 ip;
if (!internetDottedQuadToIp(managingHost, &ip))
    {
    warn("%s doesn't seem to be in dotted quad form\n", managingHost);
    return;
    }
pmInit(&pm, ip, paraHubPort);
pmPrintf(&pm, "jobDone %s %s", jobIdString, line);
if (!pmSend(&pm, mainRudp))
    warn("Couldn't send message to %s to say %s is done\n", managingHost, jobIdString);
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

// clearZombies();
if (jobIdString != NULL && line != NULL && line[0] != 0)
    {
    /* Remove job from list running list and put on recently finished list. */
    struct job *job = findRunningJob(atoi(jobIdString));
    if (job != NULL)
        {
	int status, err;
	err = waitpid(job->pid, &status, 0);
	if (err == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
	    {
	    logDebug("paraNode sheparding %s pid %d status %d err %d errno %d", 
                     jobIdString, job->pid, status, err, errno);
	    }
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
    tellManagerJobIsDone(managingHost, jobIdString, line);
    }
}

void doCheck(char *line, struct sockaddr_in *hubIp)
/* Send back check result - either a check in message or
 * jobDone. */
{
char *jobIdString = nextWord(&line);
if (jobIdString != NULL)
    {
    int jobId = atoi(jobIdString);
    struct job *job = findRunningJob(jobId);
    struct paraMessage pm;
    pmInit(&pm, ntohl(hubIp->sin_addr.s_addr), paraHubPort);
    if (job != NULL)
	pmPrintf(&pm, "checkIn %s %s running", hostName, jobIdString);
    else
	{
	struct job *job = findFinishedJob(jobId);
	if (job == NULL)
	    pmPrintf(&pm, "checkIn %s %s free", hostName, jobIdString);
	else
	    pmPrintf(&pm, "jobDone %s %s", jobIdString, job->doneMessage);
	}
    pmSend(&pm, mainRudp);
    }
}

void doResurrect(char *line, struct sockaddr_in *hubIp)
/* Send back I'm alive message */
{
struct paraMessage pm;
struct dlNode *node;
int jobsReported = 0;
pmInit(&pm, ntohl(hubIp->sin_addr.s_addr), paraHubPort);
pmPrintf(&pm, "alive %s", hostName);
for (node = jobsRunning->head; !dlEnd(node); node = node->next)
    {
    struct job *job = node->val;
    pmPrintf(&pm, " %d", job->jobId);
    ++jobsReported;
    }
for (node = jobsFinished->head; !dlEnd(node); node = node->next)
    {
    struct job *job = node->val;
    if (jobsReported >= maxProcs)
	break;
    pmPrintf(&pm, " %d", job->jobId);
    ++jobsReported;
    }
pmSend(&pm, mainRudp);
}

void doRun(char *line, struct sockaddr_in *hubIp)
/* Execute command. */
{
char *jobMessage = cloneString(line);
static char *args[1024];
int argCount;
char hubDottedQuad[17];

nextRandom();
if (line == NULL)
    warn("Executing nothing...");
else if (!internetIpToDottedQuad(ntohl(hubIp->sin_addr.s_addr), hubDottedQuad))
    warn("Can't convert ipToDottedQuad");
else
    {
    struct runJobMessage rjm;
    if (parseRunJobMessage(line, &rjm))
	{
	int jobId = atoi(rjm.jobIdString);
	if (findRunningJob(jobId) == NULL && findFinishedJob(jobId) == NULL)
	    {
	    if (busyProcs < maxProcs)
		{
		int childPid;
		argCount = chopLine(rjm.command, args);
		if (argCount >= ArraySize(args))
		    warn("Too many arguments to run");
		else
		    {
		    args[argCount] = NULL;
		    if ((childPid = forkOrDie()) == 0)
			{
			/* Do JOB_ID substitutions */
			struct subText *st = subTextNew("$JOB_ID", rjm.jobIdString);
			int i;
			rjm.in = subTextString(st, rjm.in);
			rjm.out = subTextString(st, rjm.out);
			rjm.err = subTextString(st, rjm.err);
			for (i=0; i<argCount; ++i)
			    args[i] = subTextString(st, args[i]);

			execProc(hubDottedQuad, rjm.jobIdString, rjm.reserved,
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
			jobMessage = NULL;	/* No longer own memory. */
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
	else
	    {
	    warn("Duplicate run-job %d\n", jobId);
	    }
	}
    }
freez(&jobMessage);
}

void doFetch(char *line)
/* Fetch first part of file.  Protocol is to send the
 * file one UDP packet at a time.  A zero length packet
 * indicates end of file. */
{
char *user = cloneString(nextWord(&line));
char *fileName = cloneString(nextWord(&line));
if ((user == NULL) || (fileName != NULL))
    {
    FILE *f = fopen(fileName, "r");
    pmClear(&pmIn);
    if (f == NULL)
	{
        if (user == NULL)
            user = "<null>";
	pmPrintf(&pmIn, "Couldn't open fetch file: \"%s\" %s for user %s",
                 fileName, strerror(errno), user);
	warn("Couldn't open fetch file: \"%s\" %s for user %s",
             fileName, strerror(errno), user);
	pmSend(&pmIn, mainRudp);
	pmClear(&pmIn);
	pmSend(&pmIn, mainRudp);
	}
    else
	{
	int size;
	for (;;)
	    {
	    size = fread(pmIn.data,1,  sizeof(pmIn.data)-1, f);
	    if (size < 0)
		{
		size = 0;
		warn("Couldn't read fetch file: \"%s\" %s",
                     fileName, strerror(errno));
		}
	    pmIn.size = size;
	    pmSend(&pmIn, mainRudp);
	    if (size == 0)
		break;
	    }
	fclose(f);
	}
    }
freez(&user);
freez(&fileName);
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
pmClear(&pmIn);
pmPrintf(&pmIn, "%d of %d CPUs busy", busyProcs, maxProcs);
if (busyProcs > 0)
    {
    struct dlNode *node;
    pmPrintf(&pmIn, ". Jobs:");
    for (node = jobsRunning->head; !dlEnd(node); node = node->next)
        {
	struct job *job = node->val;
        pmPrintf(&pmIn, " %d", job->jobId);
	}
    }
pmSend(&pmIn, mainRudp);
}

void listJobs()
/* Report jobs running and recently finished. */
{
struct job *job;
struct dlNode *node;

pmClear(&pmIn);
pmPrintf(&pmIn, "%d running", dlCount(jobsRunning));
if (!pmSend(&pmIn, mainRudp))
    return;
for (node = jobsRunning->head; !dlEnd(node); node=node->next)
    {
    job = node->val;
    pmClear(&pmIn);
    pmPrintf(&pmIn, "%s", job->startMessage);
    if (!pmSend(&pmIn, mainRudp))
        return;
    }
pmClear(&pmIn);
pmPrintf(&pmIn, "%d recent", dlCount(jobsFinished));
if (!pmSend(&pmIn, mainRudp))
    return;
for (node = jobsFinished->head; !dlEnd(node); node=node->next)
    {
    job = node->val;
    pmClear(&pmIn);
    pmPrintf(&pmIn, "%s", job->startMessage);
    if (!pmSend(&pmIn, mainRudp))
        return;
    pmClear(&pmIn);
    pmPrintf(&pmIn, "%s", job->doneMessage);
    if (!pmSend(&pmIn, mainRudp))
        return;
    }
}

void paraNode()
/* paraNode - a net server. */
{
char *line;
char *command;
struct sockaddr_in sai;

/* We have to know who we are... */
hostName = getMachine();
initRandom();
getTicksToHundreths();

/* Make job lists. */
jobsRunning = newDlList();
jobsFinished = newDlList();

/* Set up socket and self to listen to it. */
ZeroVar(&sai);
sai.sin_family = AF_INET;
sai.sin_port = htons(paraNodePort);
sai.sin_addr.s_addr = INADDR_ANY;
mainRudp = rudpMustOpenBound(&sai);
mainRudp->maxRetries = 12;

/* Event loop. */
findNow();
logInfo("starting %s", hostName);
for (;;)
    {
    /* Get next incoming message and optionally check to make
     * sure that it's from a host we trust, and check signature
     * on first bit of incoming data. */
    if (pmReceive(&pmIn, mainRudp))
	{
	findNow();
	if (hubName == NULL || ntohl(pmIn.ipAddress.sin_addr.s_addr) == hubIp 
		|| ntohl(pmIn.ipAddress.sin_addr.s_addr) == localIp)
	    {
	    /* Host and signature look ok,  read a string and
	     * parse out first word as command. */
	    line = pmIn.data;
	    logDebug("message from %s: \"%s\"",
                     paraFormatIp(ntohl(pmIn.ipAddress.sin_addr.s_addr)),
                     line);
	    command = nextWord(&line);
	    if (command != NULL)
		{
		if (sameString("quit", command))
		    break;
		else if (sameString("run", command))
		    doRun(line, &pmIn.ipAddress);
		else if (sameString("jobDone", command))
		    jobDone(line);
		else if (sameString("status", command))
		    doStatus();
		else if (sameString("kill", command))
		    doKill(line);
		else if (sameString("check", command))
		    doCheck(line, &pmIn.ipAddress);
		else if (sameString("resurrect", command))
		    doResurrect(line, &pmIn.ipAddress);
		else if (sameString("listJobs", command))
		    listJobs();
		else if (sameString("fetch", command))
		    doFetch(line);
                else
                    logWarn("invalid command: \"%s\"", command);
		}
	    logDebug("done command");
	    }
	else
	    {
	    logWarn("command from unauthorized host %s",
                    paraFormatIp(ntohl(pmIn.ipAddress.sin_addr.s_addr)));
	    }
	}
    }
rudpClose(&mainRudp);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 2)
    usage();
maxProcs = optionInt("cpu", 1);
umaskVal = optionInt("umask", 0002);
userPath = optionVal("userPath", userPath);
sysPath = optionVal("sysPath", sysPath);
randomDelay = optionInt("randomDelay", randomDelay);

/* Look up IP addresses. */
localIp = lookupIp("localhost");
hubName = optionVal("hub", NULL);
if (hubName != NULL)
    hubIp = lookupIp(hubName);
paraDaemonize("paraNode");
paraNode();
return 0;
}


