/* para - para - manage a batch of jobs in parallel on a compute cluster.. */
#include <sys/wait.h>
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "hash.h"
#include "dystring.h"
#include "obscure.h"
#include "portable.h"
#include "net.h"
#include "paraLib.h"
#include "jobDb.h"
#include "jobResult.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "para - Manage a batch of jobs in parallel on a compute cluster.\n"
  "Normal usage is to do a 'para create' followed by 'para push' until\n"
  "job is done.  Use 'para check' to check status\n"
  "usage:\n"
  "   para command [command-specific arguments]\n"
  "The commands are:\n"
  "\n"
  "para create jobList\n"
  "   This makes the job-tracking database from a text file with the\n"
  "   command line for each job on a separate line\n"
  "para push \n"
  "   This pushes forward the batch of jobs by submitting jobs to parasol\n"
  "   It will limit parasol queue size to something not too big and\n"
  "   retry failed jobs\n"
  "   options:\n"
  "      -retries=N   Number of retries per job - default 4.\n"
  "      -maxQueue=N  Number of jobs to allow on parasol queue - default 200000\n"
  "      -minPush=N  Minimum number of jobs to queue - default 1.  Overrides maxQueue\n"
  "      -maxPush=N  Maximum number of jobs to queue - default 100000\n"
  "      -warnTime=N Number of minutes job runs before hang warning - default 4320 (3 days)\n"
  "      -killTime=N Number of minutes job runs before push kills it - default 20160 (2 weeks)\n"
  "para try \n"
  "   This is like para push, but only submits up to 10 jobs\n"
  "para shove\n"
  "   Push jobs in this database until all are done or one fails after N retries\n"
  "para make jobList\n"
  "   Create database and run all jobs in it if possible.  If one job\n"
  "   failes repeatedly this will fail.  Suitable for inclusion in makefiles\n"
  "   Same as a 'create' followed by a 'shove'.\n"
  "para check\n"
  "   This checks on the progress of the jobs.\n"
  "para stop\n"
  "   This stops all the jobs in the batch\n"
  "para chill\n"
  "   Tells system to not launch more jobs in this batch, but\n"
  "   does not stop jobs that are already running.\n"
  "para finished\n"
  "   List jobs that have finished\n"
  "para hung\n"
  "   List hung jobs in the batch\n"
  "para crashed\n"
  "   List jobs that crashed or failed output checks the last time they were run.\n"
  "para failed\n"
  "   List jobs that crashed after repeated restarts.\n"
  "para problems\n"
  "   List jobs that had problems (even if successfully rerun).  Includes host info\n"
  "para running\n"
  "   Print info on currently running jobs\n"
  "para time\n"
  "   List timing information\n"
  );
}

/* Variables that can be set from command line. */

int retries = 4;
int maxQueue = 200000;
int minPush = 1;
int maxPush = 100000;
int warnTime = 3*24*60;
int killTime = 14*24*60;
int sleepTime = 5*60;

/* Some variable we might want to move to a config file someday. */
char *tempName = "para.tmp";	/* Name for temp files. */
char *resultsName = "para.results"; /* Name of results file. */
char *statusCommand = "parasol pstat";
char *killCommand = "parasol remove job";

enum jaState 
/* A job is in one of these states. */
 {
 jaUnsubmitted,
 jaQueued,
 jaRunning,
 jaHung,
 jaCrashed,
 jaFinished,
 };

enum jaState figureState(struct job *job)
/* Figure out state of job. */
{
struct submission *sub;
if ((sub = job->submissionList) == NULL)
    return jaUnsubmitted;
if (sub->inQueue)
    return jaQueued;
if (sub->running)
    return jaRunning;
if (sub->hung)
    return jaHung;
if (sub->ranOk)
    return jaFinished;
else
    return jaCrashed;
}

/* Places that can be checked. */
char *checkWhens[] = {"in", "out"};

/* Types of checks. */
char *checkTypes[] = {"exists", "exists+", "line", "line+"};

struct job *jobFromLine(struct lineFile *lf, char *line)
/* Parse out the beginnings of a job from input line. 
 * Parse out and keep checks. */
{
struct check *check;
char *pattern = "{check";
char *s, *e, *z;
struct dyString *dy = dyStringNew(1024);
struct job *job;

AllocVar(job);
job->spec = cloneString(line);
s = line;
for (;;)
    {
    e = stringIn(pattern, s);
    if (e == NULL)
	{
	dyStringAppend(dy, s);
	break;
	}
    else
        {
	char *parts[5];
	int partCount;
	dyStringAppendN(dy, s, e-s);
	z = strchr(e, '}');
	if (z == NULL)
	    errAbort("{check without } line %d of %s", lf->lineIx, lf->fileName);
	*z = 0;
	partCount = chopLine(e, parts);
	if (partCount != 4)
	    errAbort("Badly formatted check line %d of %s", lf->lineIx, lf->fileName);
	AllocVar(check);
	slAddHead(&job->checkList, check);
	job->checkCount += 1;
	if (stringIx(parts[1], checkWhens) < 0)
	    errAbort("Unrecognized word '%s' in check line %d of %s", 
	    	parts[1], lf->lineIx, lf->fileName);
	check->when = cloneString(parts[1]);
	if (stringIx(parts[2], checkTypes) < 0)
	    errAbort("Unrecognized word '%s' in check line %d of %s", 
	    	parts[2], lf->lineIx, lf->fileName);
	check->what = cloneString(parts[2]);
	check->file = cloneString(parts[3]);
	dyStringAppend(dy, check->file);
	s = z+1;
	}
    }
job->command = cloneString(dy->string);
slReverse(&job->checkList);
dyStringFree(&dy);
return job;
}

struct fileStatus
/* Some info on a file. */
    {
    bool exists;	/* TRUE if file exists. */
    bool hasData;	/* TRUE if nonempty. */
    bool completeLastLine; /* TRUE if last line ends with <lf> */
    bool reported;	/* TRUE if reported error. */
    };

struct fileStatus *getFileStatus(char *file)
/* Get information on file. */
{
struct fileStatus *fi;
FILE *f;

AllocVar(fi);
if ((f = fopen(file, "rb")) != NULL)
    {
    fi->exists = TRUE;
    if (fseek(f, -1, SEEK_END) == 0)
        {
	int c = fgetc(f);
	if (c >= 0)
	    {
	    fi->hasData = TRUE;
	    if (c == '\n')
	        fi->completeLastLine = TRUE;
	    }
	}
    fclose(f);
    }
return fi;
}

int doOneCheck(struct check *check, struct hash *hash, FILE *f)
/* Do one check.  Return error count from check. */
{
struct fileStatus *fi;
char *file = check->file;
char *what = check->what;

if ((fi = hashFindVal(hash, file)) == NULL)
    {
    fi = getFileStatus(file);
    hashAdd(hash, file, fi);
    }
if (!fi->reported)
    {
    if (!fi->exists)
	{
	fprintf(f, "%s does not exist\n", file);
	fi->reported = TRUE;
	return 1;
	}
    if (sameWord(what, "exists+"))
	{
	if (!fi->hasData)
	    {
	    fprintf(f, "%s is empty\n", file);
	    fi->reported = TRUE;
	    return 1;
	    }
	}
    else if (sameWord(what, "line"))
	{
	if (fi->hasData && !fi->completeLastLine)
	    {
	    fprintf(f, "%s has an incomplete last line\n", file);
	    fi->reported = TRUE;
	    return 1;
	    }
	}
    else if (sameWord(what, "line+"))
	{
	if (!fi->hasData)
	    {
	    fprintf(f, "%s is empty\n", file);
	    fi->reported = TRUE;
	    return 1;
	    }
	else if (!fi->completeLastLine)
	    {
	    fprintf(f, "%s has an incomplete last line\n", file);
	    fi->reported = TRUE;
	    return 1;
	    }
	}
    else if (sameString(what, "exists"))
	{
	/* Check already made. */
	}
    else
	{
	warn("Unknown check '%s'", what);
	}
    }
return 0;
}

void occassionalDot()
/* Write out a dot every ten times this is called. */
{
static int dotMod = 10;
static int dot = 10;
if (--dot <= 0)
    {
    putc('.', stdout);
    fflush(stdout);
    dot = dotMod;
    }
}

void occassionalSleep()
/* Sleep every 1000 times this is called. */
{
static int dotMod = 600;
static int dot = 600;
if (--dot <= 0)
    {
    sleep(1);
    dot = dotMod;
    }
}


int checkOneJob(struct job *job, char *when, struct hash *hash)
/* Perform checks on one job if checks not already in hash. 
 * Returns number of errors. */
{
int errCount = 0;
struct check *check;

for (check = job->checkList; check != NULL; check = check->next)
    {
    if (sameWord(when, check->when))
	{
	errCount += doOneCheck(check, hash, stderr);
	occassionalDot();
	}
    }
return errCount;
}

void doChecks(struct jobDb *db, char *when)
/* Do checks on files where check->when matches when. */
{
int errCount = 0;
struct job *job;
struct hash *hash = newHash(0);

printf("Checking %sput files", when);
for (job = db->jobList; job != NULL; job = job->next)
    errCount += checkOneJob(job, when, hash);
if (errCount > 0)
    errAbort("%d total errors in file check", errCount);
freeHashAndVals(&hash);
printf("\n");
}

void writeBatch(struct jobDb *db, char *fileName)
/* Write out batch file. */
{
FILE *f = mustOpen(fileName, "w");
struct job *job;
for (job = db->jobList; job != NULL; job = job->next)
    {
    jobCommaOut(job, f);
    fprintf(f, "\n");
    }
carefulClose(&f);
}

boolean gotSig = FALSE;	/* Set to true if got signal. */
int sigVal;		/* Which signal we got. */

void catchSig(int sig)
/* Catch termination signal. */
{
gotSig = TRUE;
sigVal = sig;
}

void atomicWriteBatch(struct jobDb *db, char *fileName)
/* Wrapper to avoid termination in the middle of
 * writing a batch file out. */
{
void (*oldHup)(int) = signal(SIGHUP, catchSig);
void (*oldTerm)(int) = signal(SIGTERM, catchSig);
void (*oldInt)(int) = signal(SIGINT, catchSig);
void (*oldQuit)(int) = signal(SIGQUIT, catchSig);
writeBatch(db, fileName);
signal(SIGHUP, oldHup);
signal(SIGTERM, oldTerm);
signal(SIGINT, oldInt);
signal(SIGQUIT, oldQuit);
if (gotSig)
    errAbort("Exiting from signal %d", sigVal);
}


struct jobDb *readBatch(char *batch)
/* Read a batch file. */
{
struct jobDb *db;
struct job *job;
struct lineFile *lf = lineFileOpen(batch, TRUE);
char *line;

AllocVar(db);
while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);
    if (line[0] == '#' || line[0] == 0)
       continue;
    job = jobCommaIn(&line, NULL);
    slAddHead(&db->jobList, job);
    ++db->jobCount;
    }
lineFileClose(&lf);
slReverse(&db->jobList);
printf("%d jobs in batch\n", db->jobCount);
return db;
}


void paraCreate(char *batch, char *jobList)
/* Create a batch database from a job list. */
{
struct lineFile *lf = lineFileOpen(jobList, TRUE);
char *line;
int jobCount = 0;
struct jobDb *db;
struct job *job;
char backup[512];

makeDir("err");
AllocVar(db);
while (lineFileNext(lf, &line, NULL))
    {
    line = trimSpaces(line);
    if (line == NULL || line[0] == '#' || line[0] == 0)
        continue;
    ++db->jobCount;
    job = jobFromLine(lf, line);
    slAddHead(&db->jobList, job);
    }
lineFileClose(&lf);
slReverse(&db->jobList);

doChecks(db, "in");
sprintf(backup, "%s.bak", batch);
writeBatch(db, backup);
atomicWriteBatch(db, batch);
printf("%d jobs written to %s\n", db->jobCount, batch);
}

char *hubSingleLineQuery(char *query)
/* Send message to hub and get single line response.
 * This should be freeMem'd when done. */
{
int hubFd = netConnect("localhost", paraPort);
char *result;

if (hubFd < 0)
    return NULL;
if (!sendWithSig(hubFd, query))
    {
    close(hubFd);
    return NULL;
    }
result = netRecieveLongString(hubFd);
close(hubFd);
return result;
}

boolean submitJob(struct job *job, char *curDir)
/* Attempt to submit job. */
{
struct dyString *cmd = dyStringNew(1024);
struct submission *sub;
char *jobId = NULL;

dyStringPrintf(cmd, "addJob %s %s /dev/null /dev/null %s/para.results %s", cuserid(NULL), curDir, curDir, job->command);
jobId = hubSingleLineQuery(cmd->string);
if (jobId != NULL)
    {
    AllocVar(sub);
    slAddHead(&job->submissionList, sub);
    job->submissionCount += 1;
    sub->submitTime = time(NULL);
    sub->host = cloneString("n/a");
    sub->id = jobId;
    sub->inQueue = TRUE;
    }
dyStringFree(&cmd);
return jobId != NULL;
}

void statusOutputChanged()
/* Complain about status output format change and die. */
{
errAbort("\n%s output format changed, please update markQueuedJobs in para.c", 
	statusCommand);
}

struct slRef *hubMultilineQuery(char *query)
/* Send a command with a multiline response to hub,
 * and return response as a list of strings. */
{
struct slRef *list = NULL, *el;
char *line;
int hubFd = netConnect("localhost", paraPort);
if (hubFd > 0)
    {
    if (sendWithSig(hubFd, query))
        {
	for (;;)
	    {
	    line = netRecieveLongString(hubFd);
	    if (line == NULL || line[0] == 0)
		break;
	    refAdd(&list, line);
	    }
	}
    close(hubFd);
    }
slReverse(&list);
return list;
}

int markQueuedJobs(struct jobDb *db)
/* Mark jobs that are queued up. Return total number of jobs in queue. */
{
struct hash *hash = newHash(0);
struct job *job;
struct submission *sub;
int queueSize = 0;
struct slRef *lineList = hubMultilineQuery("pstat"), *lineEl;

/* Make hash of submissions based on id and clear flags. */
for (job = db->jobList; job != NULL; job = job->next)
    {
    for (sub = job->submissionList; sub != NULL; sub = sub->next)
        {
	hashAdd(hash, sub->id, sub);
	sub->running = FALSE;
	sub->inQueue = FALSE;
	}
    }

/* Get job list from paraHub. */
queueSize = slCount(lineList);
printf("%d jobs (including everybody's) in Parasol queue.\n", queueSize);

/* Read status output. */
for (lineEl = lineList; lineEl != NULL; lineEl = lineEl->next)
    {
    int wordCount;
    char *line = lineEl->val;
    char *row[6];
    wordCount = chopLine(line, row);
    if (wordCount < 6)
	errAbort("Expecting at least 6 words in pstat output. paraHub and para out of sync");
    else
	{
	char *state = row[0], *jobId = row[1], *user = row[2],
	     *exe = row[3], *ticks = row[4], *host = row[5];
	time_t t = atol(ticks);
	if ((sub = hashFindVal(hash, jobId)) != NULL)
	    {
	    if (sameString(state, "r"))
		{
		sub->running = TRUE;
		sub->startTime = t;
		sub->host = cloneString(host);
		}
	    else
		{
		sub->inQueue = TRUE;
		}
	    }
	}
    freez(&lineEl->val);
    }
slFreeList(&lineList);
freeHash(&hash);
return queueSize;
}

struct hash *hashResults(char *fileName)
/* Return hash of job results keyed on id as string. */
{
struct hash *hash = newHash(0);
if (fileExists(fileName))
    {
    struct jobResult *el, *list = jobResultLoadAll(fileName);
    for (el = list; el != NULL; el = el->next)
	hashAdd(hash, el->jobId, el);
    }
return hash;
}

void freeResults(struct hash **pHash)
/* Free up results hash and elements in it. */
{
struct hash *hash = *pHash;
struct hashEl *list, *el;
struct jobResult *jr;
if ((hash = *pHash) != NULL)
    {
    list = hashElListHash(hash);
    for (el = list; el != NULL; el = el->next)
	{
	jr = el->val;
	jobResultFree(&jr);
	}
    hashElFreeList(&list);
    hashFree(pHash);
    }
}


int statusToRetVal(int status)
/* Convert wait() return status to return value. */
{
if (WIFEXITED(status))
    return WEXITSTATUS(status);
else 
    return -666;
}

double jrCpuTime(struct jobResult *jr)
/* Get CPU time in seconds for job. */
{
return 1.0/(double)CLK_TCK * (jr->usrTicks + jr->sysTicks);
}

struct hash *markRunJobStatus(struct jobDb *db)
/* Mark jobs based on results output file. 
 * Returns hash of results. */
{
struct job *job;
struct submission *sub;
char *line, *words[11];
int wordCount;
long killSeconds = killTime*60;
long warnSeconds = warnTime*60;
long duration;
struct hash *checkHash = newHash(0);
struct hash *resultsHash = hashResults(resultsName);
char host[128];
time_t now = time(NULL);

printf("Checking finished jobs");
for (job=db->jobList; job != NULL; job = job->next)
    {
    if ((sub = job->submissionList) != NULL)
        {
	/* Look for hitherto unclassified jobs that are either running or
	 * possibly finished. */
	if (!sub->queueError && !sub->inQueue && !sub->crashed && 
		!sub->hung && !sub->running && !sub->ranOk)
	    {
	    struct jobResult *jr = hashFindVal(resultsHash, sub->id);
	    if (jr == NULL)
	        {
		sub->trackingError = 3;
		}
	    else
	        {
		sub->startTime = jr->startTime;
		sub->endTime = jr->endTime;
		sub->cpuTime = jrCpuTime(jr);
		sub->retVal = statusToRetVal(jr->status);
		sub->gotRetVal = TRUE;
		sub->host = cloneString(jr->host);
		sub->inQueue = FALSE;
		sub->running = FALSE;
		if (sub->retVal == 0 && checkOneJob(job, "out", checkHash) == 0)
		    sub->ranOk = TRUE;
		else
		    sub->crashed = TRUE;
		}
	    }
	}
    }
freeHash(&checkHash);
printf("\n");
return resultsHash;
}

boolean needsRerun(struct submission *sub)
/* Return TRUE if submission needs to be rerun. */
{
if (sub == NULL)
    return TRUE;
return sub->submitError || sub->queueError || sub->crashed || sub->trackingError;
}

struct jobDb *paraCycle(char *batch)
/* Cycle forward through batch.  Return database. */
{
struct jobDb *db = readBatch(batch);
struct job *job;
int queueSize;
int pushCount = 0, retryCount = 0;
int tryCount;
boolean finished = FALSE;
struct hash *resultsHash;
char curDir[512];

if (getcwd(curDir, sizeof(curDir)) == NULL)
    errAbort("Couldn't get current directory");
queueSize = markQueuedJobs(db);
resultsHash = markRunJobStatus(db);

for (tryCount=1; tryCount<=retries && !finished; ++tryCount)
    {
    for (job = db->jobList; job != NULL; job = job->next)
        {
	if (job->submissionCount < tryCount && 
	   (job->submissionList == NULL || needsRerun(job->submissionList)))
	    {
	    if (!submitJob(job, curDir))
		{
		finished = TRUE;
	        break;
		}
	    occassionalDot();
	    // occassionalSleep();
	    ++pushCount;
	    if (tryCount > 1)
	        ++retryCount;
	    if (pushCount >= maxPush)
	        {
		finished = TRUE;
		break;
		}
	    if (pushCount + queueSize >= maxQueue && pushCount >= minPush)
	        {
		finished = TRUE;
		break;
		}
	    }
	}
    }
if (pushCount > 0)
    printf("\n");
atomicWriteBatch(db, batch);
uglyf("(updated job database on disk)\n");
if (pushCount > 0)
    {
    printf("\n");
    printf("Pushed Jobs: %d\n", pushCount);
    }
if (retryCount > 0)
    printf("Retried jobs: %d\n", retryCount);
freeResults(&resultsHash);
uglyf("(freed results)\n");
return db;
}

void paraPush(char *batch)
/* Push forward batch one time. */
{
struct jobDb *db = paraCycle(batch);
jobDbFree(&db);
uglyf("(freed database)\n");
}

void paraShove(char *batch)
/* Push batch of jobs and keep pushing until it's finished, polling
 * parasol every 5 minutes. */
{
struct jobDb *db;
struct job *job;
struct submission *sub;
int maxSleep = 5*60;
int curSleep = 15;
time_t start = time(NULL), now;

for (;;)
    {
    boolean anyUnfinished = FALSE;
    db = paraCycle(batch);
    for (job = db->jobList; job != NULL; job = job->next)
        {
	if ((sub = job->submissionList) == NULL)
	    anyUnfinished = TRUE;
	else
	    {
	    enum jaState state = figureState(job);
	    if (job->submissionCount >= retries)
	        {
		if (state != jaUnsubmitted && state != jaQueued && state != jaRunning)
		    errAbort("Batch failed after %d tries on %s", retries, job->command);
		}
	    if (state != jaFinished)
	        anyUnfinished = TRUE;
	    }
	}
    jobDbFree(&db);
    if (!anyUnfinished)
        break;
    sleep(curSleep);
    if (curSleep < maxSleep)
        curSleep += 15;
    now = time(NULL);
    printf("Checking job status %d minutes after launch\n",  round((now-start)/60.0));
    }
printf("Successful batch!\n");
}

void paraMake(char *batch, char *spec)
/* Do a create and then a shove. */
{
paraCreate(batch, spec);
paraShove(batch);
}

void reportOnJobs(struct jobDb *db)
/* Report on status of jobs. */
{
int submitError = 0, inQueue = 0, queueError = 0, trackingError = 0, running = 0, crashed = 0,
    slow = 0, hung = 0, ranOk = 0, jobCount = 0, unsubmitted = 0, total = 0, failed = 0;
struct job *job;
struct submission *sub;

for (job = db->jobList; job != NULL; job = job->next)
    {
    if ((sub = job->submissionList) != NULL)	/* Get most recent submission if any. */
        {
	if (sub->submitError) ++submitError;
	if (sub->queueError) ++queueError;
	if (sub->trackingError) ++trackingError;
	if (sub->inQueue) ++inQueue;
	if (sub->crashed) ++crashed;
	if (sub->slow) ++slow;
	if (sub->hung) ++hung;
	if (sub->running) ++running;
	if (sub->ranOk) ++ranOk;
	if (job->submissionCount >= retries && needsRerun(sub) || hung)
	     ++failed;
	}
    else
        ++unsubmitted;
    ++total;
    }
if (unsubmitted > 0)
   printf("unsubmitted jobs: %d\n", unsubmitted);
if (submitError > 0)
   printf("submission errors: %d\n", submitError);
if (queueError > 0)
   printf("queue errors: %d\n", queueError);
if (trackingError > 0)
   printf("tracking errors: %d\n", trackingError);
if (inQueue > 0)
   printf("queued and waiting: %d\n", inQueue);
if (crashed > 0)
   printf("crashed: %d\n", crashed);
if (slow > 0)
   printf("slow (> %d minutes): %d\n", warnTime, slow);
if (hung > 0)
   printf("hung (> %d minutes): %d\n", killTime, hung);
if (running > 0)
   printf("running: %d\n", running);
if (ranOk > 0)
   printf("ranOk: %d\n", ranOk);
if (failed > 0)
   printf("failed %d times: %d\n", retries, failed);
printf("total jobs in batch: %d\n", total);
}

void paraCheck(char *batch)
/* Check on progress of a batch. */
{
struct jobDb *db = readBatch(batch);
int queueSize;

queueSize = markQueuedJobs(db);
markRunJobStatus(db);
reportOnJobs(db);

atomicWriteBatch(db, batch);
}

void paraListFailed(char *batch)
/* List all jobs that failed. */
{
struct jobDb *db = readBatch(batch);
struct job *job;
struct submission *sub;

markQueuedJobs(db);
markRunJobStatus(db);
for (job = db->jobList; job != NULL; job = job->next)
    {
    sub = job->submissionList;
    if (sub != NULL)
        {
	if (job->submissionCount >= retries && needsRerun(sub))
	    printf("%s\n", job->spec);
	}
    }
}

void paraListState(char *batch, enum jaState targetState)
/* List all jobs that match target state. */
{
struct jobDb *db = readBatch(batch);
struct job *job;
markQueuedJobs(db);
markRunJobStatus(db);
for (job = db->jobList; job != NULL; job = job->next)
    {
    enum jaState state = figureState(job);
    if (state == targetState)
	printf("%s\n", job->spec);
    }
}

void printErrFile(struct submission *sub, struct jobResult *jr)
/* Print error file if it exists. */
{
char localName[64];
sprintf(localName, "err/%s", jr->jobId);
if (!fileExists(localName))
    {
    struct dyString *dy = newDyString(256);
    dyStringPrintf(dy, "rcp %s:%s %s", jr->host, jr->errFile, localName);
    if (system(dy->string) != 0)
        warn("'%s' failed", dy->string);
    freeDyString(&dy);
    }
if (fileExists(localName))
    {
    char *buf;
    size_t size;
    printf("stderr:\n");
    readInGulp(localName, &buf, &size);
    mustWrite(stdout, buf, size);
    freez(&buf);
    }
}

void problemReport(struct job *job, struct submission *sub, char *type, struct hash *resultsHash)
/* Print report on one problem. */
{
struct check *check;
struct hash *hash = newHash(0);
struct jobResult *jr = hashFindVal(resultsHash, sub->id);

printf("job: %s\n", job->command);
printf("id: %s\n", sub->id);
printf("failure type: %s\n", type);
if (jr == NULL)
     printf("tracking error: %d\n", sub->trackingError);
else
    {
    time_t startTime = jr->startTime;
    printf("host: %s\n", jr->host);
    printf("start time: %s", ctime(&startTime));
    printf("return: %d\n", statusToRetVal(jr->status));
    for (check = job->checkList; check != NULL; check = check->next)
	doOneCheck(check, hash, stdout);
    printErrFile(sub, jr);
    }
printf("\n");
hashFree(&hash);
}

void paraProblems(char *batch)
/*  List jobs that had problems (even if successfully rerun).  Includes host info */
{
struct jobDb *db = readBatch(batch);
struct job *job;
struct submission *sub;
int problemCount = 0;
struct hash *resultsHash;

markQueuedJobs(db);
resultsHash = markRunJobStatus(db);
for (job = db->jobList; job != NULL; job = job->next)
    {
    for (sub = job->submissionList; sub != NULL; sub = sub->next)
	{
	if (sub->hung)
	    {
	    problemReport(job, sub, "hung", resultsHash);
	    ++problemCount;
	    }
	else if (sub->slow)
	    {
	    problemReport(job, sub, "slow", resultsHash);
	    ++problemCount;
	    }
	else if (sub->trackingError)
	    {
	    problemReport(job, sub, "tracking error", resultsHash);
	    ++problemCount;
	    }
	else if (needsRerun(sub))
	    {
	    problemReport(job, sub, "crash", resultsHash);
	    ++problemCount;
	    }
	}
    }
printf("%d problems total\n", problemCount);
}

void runningReport(struct job *job, struct submission *sub)
/* Print report on one running job. */
{
struct check *check;
time_t startTime = sub->startTime;
int duration = time(NULL) - startTime;

printf("command: %s\n", job->command);
printf("jobId: %s\n", sub->id);
printf("host: %s\n", sub->host);
printf("start time: %s", ctime(&startTime));
printf("run time so far: %d sec,  %4.2f min, %4.2f hours,  %4.2f days\n", 
	duration, duration/60.0, duration/3600.0,  duration/(3600.0*24.0));
printf("\n");
}

void paraRunning(char *batch)
/*  List jobs that are running.  Includes host and time info */
{
struct jobDb *db = readBatch(batch);
struct job *job;
struct submission *sub;
int runCount = 0;

markQueuedJobs(db);
markRunJobStatus(db);
for (job = db->jobList; job != NULL; job = job->next)
    {
    if ((sub = job->submissionList) != NULL && sub->running)
	{
	runningReport(job, sub);
	++runCount;
	}
    }
printf("total jobs running: %d\n", runCount);
}


void sendChillMessage()
/* Tell hub to chill out on job */
{
struct dyString *dy = newDyString(1024);
char curDir[512];
char *result;
if (getcwd(curDir, sizeof(curDir)) == NULL)
    errAbort("Couldn't get current directory");
dyStringPrintf(dy, "chill %s %s/para.results", cuserid(NULL), curDir);
result = hubSingleLineQuery(dy->string);
dyStringFree(&dy);
if (result == NULL || !sameString(result, "ok"))
    errAbort("Couldn't chill %s\n", curDir);
freez(&result);
printf("Told hub to chill out\n");
}


int cleanTrackingErrors(struct jobDb *db)
/* Remove submissions with tracking errors. 
 * Returns count of these submissions */
{
int count = 0;
struct job *job;
struct submission *sub;
for (job = db->jobList; job != NULL; job = job->next)
    {
    sub = job->submissionList;
    if (sub != NULL)
        {
	if (sub->trackingError)
	    {
	    job->submissionCount -= 1;
	    job->submissionList = sub->next;
	    ++count;
	    }
	}
    }
printf("Chilled %d jobs\n", count);
return count;
}

void removeChilledSubmissions(char *batch)
/* Remove submissions from job database if we
 * asked them to chill out. */
{
struct jobDb *db = readBatch(batch);
int chillCount;

markQueuedJobs(db);
markRunJobStatus(db);
chillCount = cleanTrackingErrors(db);
atomicWriteBatch(db, batch);
}

void paraChill(char *batch)
/*  Tells system to not launch more jobs in this batch, but
 *  does not stop jobs that are already running.\n */
{
sendChillMessage();
removeChilledSubmissions(batch);
}

/* Death row is where jobs to be killed get
 * bundled together to send to server in 
 * batches of 256 or so. */

static struct dyString *deathRow = NULL;
static int deathRowSize = 0;
static int deathRowMaxSize = 10;

void deathRowStart()
/* Start up death row */
{
deathRow = newDyString(16*1024);
deathRowSize = 0;
dyStringAppend(deathRow, "removeJob");
}


void deathRowExecute()
/* Send list of jobs to kill to server. */
{
int hubFd = netConnect("localhost", paraPort);
char *ok;
mustSendWithSig(hubFd, deathRow->string);
ok = netRecieveLongString(hubFd);
if (ok == NULL || !sameString(ok, "ok"))
    errAbort("No reciept in deathRowExecute");
freez(&ok);
close(hubFd);
dyStringClear(deathRow);
dyStringAppend(deathRow, "removeJob");
deathRowSize = 0;
}

void deathRowEnd()
/* Close out death row. */
{
deathRowExecute();
dyStringFree(&deathRow);
}

void deathRowAdd(char *jobId)
/* Add job to death row. */
{
dyStringPrintf(deathRow, " %s", jobId);
++deathRowSize;
if (deathRowSize >= deathRowMaxSize)
    deathRowExecute();
}

void paraStopAll(char *batch)
/* Stop batch of jobs. */
{
struct jobDb *db = readBatch(batch);
struct job *job;
struct submission *sub;
int chillCount;

markQueuedJobs(db);
markRunJobStatus(db);
chillCount = cleanTrackingErrors(db);
deathRowStart();
for (job = db->jobList; job != NULL; job = job->next)
    {
    sub = job->submissionList;
    if (sub != NULL)
        {
	if (sub->inQueue || sub->running)
	    {
	    deathRowAdd(sub->id);
	    job->submissionCount -= 1;
	    job->submissionList = sub->next;
	    }
	}
    }
deathRowEnd();
atomicWriteBatch(db, batch);
}

void paraStop(char *batch)
/* Stop batch of jobs. */
{
sendChillMessage();	/* Remove waiting jobs first, it's faster. */
paraStopAll(batch);
}

void printTimes(char *title, double seconds,  boolean showYears)
/* Print out times in seconds, hours, days, maybe years. */
{
printf("%-27s %9ds %10.2fm %8.2fh %7.2fd", 
   title, round(seconds), seconds/60, seconds/3600, seconds/(3600*24));
if (showYears)
     printf(" %6.3f y", seconds/(3600*24*365));
printf("\n");
}

long calcFirstToLast(struct jobDb *db)
/* Calculate time between first submission and last job finish. */
{
boolean first = TRUE;
struct job *job;
struct submission *sub;
long now = time(NULL);
long subTime, firstSub = BIGNUM, endTime, lastEnd = 0;
boolean gotEnd = FALSE;

for (job = db->jobList; job != NULL; job = job->next)
    {
    if ((sub = job->submissionList) != NULL)
        {
	subTime = sub->submitTime;
	if (subTime != 0 && subTime < now)	/* Protect against wacked out clocks. */
	    {
	    if (first)
		{
		firstSub = subTime;
		first = FALSE;
		}
	    else
		{
		if (subTime < firstSub) 
		    firstSub = subTime;
		}
	    }
	if (sub->endTime != 0)
	    {
	    endTime = sub->endTime;
	    if (endTime < now)	/* Protect against wacked out clocks. */
		{
		if (endTime > lastEnd) 
		    {
		    lastEnd = endTime;
		    gotEnd = TRUE;
		    }
		}
	    }
	}
    }
if (gotEnd)
    return lastEnd - firstSub;
else
    return now - firstSub;
}

void paraTimes(char *batch)
/* Report times of run. */
{
struct jobDb *db = readBatch(batch);
double totalCpu = 0, totalWall = 0;
double oneWall, longestWall = 0;
struct job *job;
char *longestWallId = NULL;
struct submission *sub;
int jobCount = 0;
int runningCount = 0;
int timedCount = 0;
int crashCount = 0;
int queueCount = 0;
int runTime = 0;
int otherCount = 0;
struct hash *resultsHash;
long now = time(NULL);

markQueuedJobs(db);
resultsHash = markRunJobStatus(db);
for (job = db->jobList; job != NULL; job = job->next)
    {
    ++jobCount;
    if ((sub = job->submissionList) != NULL)
        {
	if (sub->running)
	   {
	   int oneTime = now - sub->startTime;
	   if (oneTime < 0)
	       warn("Strange start time in %s: %s", sub->startTime);
	   else
	       runTime += oneTime;
	   ++runningCount;
	   }
	else if (sub->inQueue)
	   {
	   ++queueCount;
	   }
	else  if (sub->crashed || sub->hung)
	   {
	   ++crashCount;
	   }
	else
	   {
	   struct jobResult *jr = hashFindVal(resultsHash, sub->id);
	   if (jr != NULL)
	       {
	       ++timedCount;
	       totalCpu += jrCpuTime(jr);
	       oneWall = jr->endTime - jr->startTime;
	       if (oneWall < 0)	/* Protect against clock reset. */
		   {
		   warn("End before start job %s host %s", sub->id, jr->host);
		   warn("Start %lu,  End %lu", jr->startTime, jr->endTime);
	           oneWall = totalCpu;	
		   }
	       totalWall += oneWall;
	       if (oneWall > longestWall) 
		   {
	           longestWall = oneWall;
		   longestWallId = sub->id;
		   }
	       }
	   else
	       {
	       ++otherCount;
	       }
	   }
	}
    }
printf("Completed: %d of %d jobs\n", timedCount, jobCount);
if (runningCount > 0)
    printf("Jobs currently running: %d\n", runningCount);
if (crashCount > 0)
    printf("Crashed: %d jobs\n", crashCount);
if (otherCount > 0)
    printf("Other count: %d jobs\n", otherCount);
if (queueCount > 0)
    printf("In queue waiting: %d jobs\n", queueCount);
printTimes("CPU time in finished jobs:", totalCpu, TRUE);
printTimes("IO & Wait Time:", totalWall-totalCpu, TRUE);
if (runningCount > 0)
    {
    printTimes("Time in running jobs:", runTime, TRUE);
    }
if (timedCount > 0)
    {
    printTimes("Average job time:", totalWall/timedCount, FALSE);
    printTimes("Longest job:", longestWall, FALSE);
    printTimes("Submission to last job:", calcFirstToLast(db), FALSE);
    }
atomicWriteBatch(db, batch);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;
char *batch;

optionHash(&argc, argv);
if (argc < 2)
    usage();
retries = optionInt("retries",  retries);
maxQueue = optionInt("maxQueue",  maxQueue);
minPush = optionInt("minPush",  minPush);
maxPush = optionInt("maxPush",  maxPush);
warnTime = optionInt("warnTime", warnTime);
killTime = optionInt("killTime", killTime);
command = argv[1];
batch = "batch";
if (strchr(batch, '/') != NULL)
    errAbort("para needs to be run in the same directory as the batch file.");
if (sameString(command, "create"))
    {
    if (argc != 3)
        usage();
    paraCreate(batch, argv[2]);
    }
else if (sameString(command, "check"))
    {
    paraCheck(batch);
    }
else if (sameString(command, "push"))
    {
    paraPush(batch);
    }
else if (sameString(command, "shove"))
    {
    paraShove(batch);
    }
else if (sameString(command, "make"))
    {
    if (argc != 3)
        usage();
    paraMake(batch, argv[2]);
    }
else if (sameString(command, "try"))
    {
    maxPush = 10;
    paraPush(batch);
    }
else if (sameString(command, "stop"))
    {
    paraStop(batch);
    }
else if (sameString(command, "chill"))
    {
    paraChill(batch);
    }
else if (sameString(command, "hung"))
    {
    paraListState(batch, jaHung);
    }
else if (sameString(command, "crashed"))
    {
    paraListState(batch, jaCrashed);
    }
else if (sameString(command, "failed"))
    {
    paraListFailed(batch);
    }
else if (sameString(command, "finished"))
    {
    paraListState(batch, jaFinished);
    }
else if (sameString(command, "problems") || sameString(command, "problem"))
    {
    paraProblems(batch);
    }
else if (sameString(command, "running"))
    {
    paraRunning(batch);
    }
else if (sameString(command, "time") || sameString(command, "times"))
    {
    paraTimes(batch);
    }
else
    {
    errAbort("Unrecognized command '%s'.  Run para with no arguments for usage summary", 
    	command);
    }
return 0;
}
