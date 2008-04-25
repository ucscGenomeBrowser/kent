/* para - para - manage a batch of jobs in parallel on a compute cluster.. */
#include "paraCommon.h"
#include "errabort.h"
#include "linefile.h"
#include "options.h"
#include "hash.h"
#include "localmem.h"
#include "dystring.h"
#include "obscure.h"
#include "portable.h"
#include "net.h"
#include "paraLib.h"
#include "paraMessage.h"
#include "jobDb.h"
#include "jobResult.h"
#include "verbose.h"

static char const rcsid[] = "$Id: para.c,v 1.70 2008/04/25 23:29:06 galt Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"retries"  , OPTION_INT},
    {"maxQueue" , OPTION_INT},
    {"minPush"  , OPTION_INT},
    {"maxPush"  , OPTION_INT},
    {"warnTime" , OPTION_INT},   // warn - not completely implemented
    {"killTime" , OPTION_INT},   // hung - not completely implemented
    {"delayTime", OPTION_INT},
    {"eta"      , OPTION_BOOLEAN},
    {"pri"      , OPTION_STRING},
    {"priority" , OPTION_STRING},
    {"maxNode"  , OPTION_STRING},
    {NULL, 0}
};

static int numHappyDots;       /* number of happy dots written */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "para - Manage a batch of jobs in parallel on a compute cluster.\n"
  "Normal usage is to do a 'para create' followed by 'para push' until\n"
  "job is done.  Use 'para check' to check status.\n"
  "usage:\n\n"
  "   para [options] command [command-specific arguments]\n\n"
  "The commands are:\n"
  "\n"
  "para create jobList\n"
  "   This makes the job-tracking database from a text file with the\n"
  "   command line for each job on a separate line.\n"
  "para push \n"
  "   This pushes forward the batch of jobs by submitting jobs to parasol\n"
  "   It will limit parasol queue size to something not too big and\n"
  "   retry failed jobs.\n"
  "   options:\n"
  "      -retries=N  Number of retries per job - default 4.\n"
  "      -maxQueue=N  Number of jobs to allow on parasol queue. \n"
  "         Default 1000000.\n"
  "      -minPush=N  Minimum number of jobs to queue. \n"
  "         Default 1.  Overrides maxQueue.\n"
  "      -maxPush=N  Maximum number of jobs to queue - default 100000.\n"
  "      -warnTime=N  Number of minutes job runs before hang warning. \n"
  "         Default 4320 (3 days).\n"
  "      -killTime=N  Number of minutes job runs before push kills it.\n"
  "         Default 20160 (2 weeks).\n"
  "      -delayTime=N  Number of seconds to delay before submitting next job \n"
  "         to minimize i/o load at startup - default 0.\n"
  "      -priority=x  Set batch priority to high, medium, or low.\n"
  "         Default medium (use high only with approval).\n"
  "         If needed, use with make, push, create, shove, or try.\n"
  "         Or, set batch priority to a specific numeric value - default %d.\n"
  "         1 is emergency high priority, \n"
  "         %d is normal medium, \n"
  "         %d is low for bottomfeeders.\n"
  "         Setting priority higher than normal (1-%d) will be logged.\n"
  "         Please keep low priority jobs short, they won't be pre-empted.\n"
  "      -maxNode=x  Limit the number of nodes the batch can use.\n"
  "         Specify number of nodes, for example 10 or 'unlimited'.\n"
  "         Default unlimited displays as -1.\n"
  "para try \n"
  "   This is like para push, but only submits up to 10 jobs.\n"
  "para shove\n"
  "   Push jobs in this database until all are done or one fails after N retries.\n"
  "para make jobList\n"
  "   Create database and run all jobs in it if possible.  If one job\n"
  "   fails repeatedly this will fail.  Suitable for inclusion in makefiles.\n"
  "   Same as a 'create' followed by a 'shove'.\n"
  "para check\n"
  "   This checks on the progress of the jobs.\n"
  "para stop\n"
  "   This stops all the jobs in the batch.\n"
  "para chill\n"
  "   Tells system to not launch more jobs in this batch, but\n"
  "   does not stop jobs that are already running.\n"
  "para finished\n"
  "   List jobs that have finished.\n"
  "para hung\n"
  "   List hung jobs in the batch.\n"
  "para crashed\n"
  "   List jobs that crashed or failed output checks the last time they were run.\n"
  "para failed\n"
  "   List jobs that crashed after repeated restarts.\n"
  "para status\n"
  "   List individual job status, including times.\n"
  "para problems\n"
  "   List jobs that had problems (even if successfully rerun).\n"
  "   Includes host info.\n"
  "para running\n"
  "   Print info on currently running jobs.\n"
  "para hippos time\n"
  "   Print info on currently running jobs taking > 'time' (minutes) to run.\n"
  "para time\n"
  "   List timing information.\n"
  "   with option: -eta also show estimated time to completion.\n"
  "para recover jobList newJobList\n"
  "   Generate a job list by selecting jobs from an existing list where\n"
  "   the `check out' tests fail.\n"
  "para priority 999\n"
  "   Set batch priority. Values explained under 'push' options above.\n"
  "para maxNode 999\n"
  "   Set batch maxNode. Values explained under 'push' options above.\n"
  "para showSickNodes\n"
  "   Show sick nodes which have failed when running this batch.\n"
  "para clearSickNodes\n"
  "   Clear sick nodes statistics of batch.\n"
  "\n"
  "Common options\n"
  "   -verbose=1 - set verbosity level.\n",
  NORMAL_PRIORITY,
  NORMAL_PRIORITY,
  NORMAL_PRIORITY * NORMAL_PRIORITY,
  NORMAL_PRIORITY-1
  );
}

/* Variables that can be set from command line. */

int retries = 4;
int maxQueue = 1000000;
int minPush = 1;
int maxPush = 100000;
int warnTime = 3*24*60;
int killTime = 14*24*60;
int sleepTime = 5*60;
int delayTime = 0;
int priority = NORMAL_PRIORITY;
int maxNode  = -1;

/* Some variable we might want to move to a config file someday. */
char *tempName = "para.tmp";	/* Name for temp files. */
char *resultsName = "para.results"; /* Name of results file. */
char *statusCommand = "parasol pstat";
char *killCommand = "parasol remove job";

void checkPrioritySetting(); /* fwd reference */
void  checkMaxNodeSetting(); /* fwd reference */

void beginHappy()
/* Call before a loop where happy dots maybe written */
{
numHappyDots = 0;
}

void endHappy()
/* Call after a loop where happy dots may have been written.  There maybe more
 * than one call to endHappy() for a beginHappy(). It's a good idea to call
 * this before outputing low-level error messages to keep messages readable
 * if dots are being written.
 */
{
if (numHappyDots > 0)
    {
    verbose(1, "\n");
    fflush(stderr);
    fflush(stdout);
    numHappyDots = 0;
    }
}

void vpprintf(FILE *f, char *format, va_list args)
/* para printf; ensures happy dots are flushed */
{
endHappy();
vfprintf(f, format, args);
}

void pprintf(FILE *f, char *format, ...)
/* para printf; ensures happy dots are flushed */
{
va_list args;
va_start(args, format);
vpprintf(f, format, args);
va_end(args);
}

void paraVaWarn(char *format, va_list args)
/* warn handler, flushes happyDots if needed. */
{
endHappy();
if (format != NULL) {
    fflush(stdout);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    fflush(stderr);  /* forces to log files */
    }
}

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

char *jaStateShortDesc[] =
/* short description of job state; indexed by enum jaState */
    {
    "unsub",
    "queue",
    "run",
    "hung",
    "crash",
    "done"
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
    bool checkedLastLine;   /* TRUE if done completeLastLine check */
    bool reported;	/* TRUE if reported error. */
    };

struct fileStatus *basicFileCheck(char *file, struct hash *hash)
/* Do quick file checks caching result */
{
struct fileStatus *fi = hashFindVal(hash, file);
struct stat statBuf;
if (fi != NULL)
    return fi;
lmAllocVar(hash->lm, fi);
if (stat(file, &statBuf) == 0)
    {
    fi->exists = TRUE;
    fi->hasData = (statBuf.st_size > 0);
    }
return fi;
}

struct fileStatus *completeLastLineCheck(char *file, struct hash *hash)
/* Do slower checks for complete last line, caching result */
{
struct fileStatus *fi = hashFindVal(hash, file);
FILE *f;

if ((fi != NULL) && (fi->checkedLastLine))
    return fi;
if (fi == NULL)
    lmAllocVar(hash->lm, fi);
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
fi->checkedLastLine = TRUE;
return fi;
}

int doOneCheck(struct check *check, struct hash *hash, FILE *f)
/* Do one check.  Return error count from check. f maybe NULL to discard
 * errors. */
{
struct fileStatus *fi;
char *file = check->file;
char *what = check->what;

/* only do slower completeLastLine check if needed */
if (startsWith("line", what))
    fi = completeLastLineCheck(file, hash);
else
    fi = basicFileCheck(file, hash);

if (!fi->reported)
    {
    if (!fi->exists)
	{
        if (f != NULL)
            pprintf(f, "%s does not exist\n", file);
	fi->reported = TRUE;
	return 1;
	}
    if (sameWord(what, "exists+"))
	{
	if (!fi->hasData)
	    {
            if (f != NULL)
                pprintf(f, "%s is empty\n", file);
	    fi->reported = TRUE;
	    return 1;
	    }
	}
    else if (sameWord(what, "line"))
	{
	if (fi->hasData && !fi->completeLastLine)
	    {
            if (f != NULL)
                pprintf(f, "%s has an incomplete last line\n", file);
	    fi->reported = TRUE;
	    return 1;
	    }
	}
    else if (sameWord(what, "line+"))
	{
	if (!fi->hasData)
	    {
            if (f != NULL)
                pprintf(f, "%s is empty\n", file);
	    fi->reported = TRUE;
	    return 1;
	    }
	else if (!fi->completeLastLine)
	    {
            if (f != NULL)
                pprintf(f, "%s has an incomplete last line\n", file);
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
return fi->reported ? 1 : 0;
}

void occassionalDot()
/* Write out a dot every 20 times this is called. */
{
static int dotMod = 20;
static int dot = 20;
if ((--dot <= 0) && verboseDotsEnabled())
    {
    verboseDot();
    dot = dotMod;
    numHappyDots++;
    }
}

void occassionalSleep()
/* Sleep every 1000 times this is called. */
{
static int dotMod = 600;
static int dot = 600;
if (--dot <= 0)
    {
    fflush(stdout);
    fflush(stderr);
    sleep(1);
    dot = dotMod;
    }
}


int checkOneJob(struct job *job, char *when, struct hash *hash, FILE *f)
/* Perform checks on one job if checks not already in hash. 
 * Returns number of errors. */
{
int errCount = 0;
struct check *check;

for (check = job->checkList; check != NULL; check = check->next)
    {
    if (sameWord(when, check->when))
	{
	errCount += doOneCheck(check, hash, f);
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
struct hash *hash = newHash(19);

verbose(1, "Checking %sput files\n", when);
beginHappy();
for (job = db->jobList; job != NULL; job = job->next)
    errCount += checkOneJob(job, when, hash, stderr);
endHappy();
if (errCount > 0)
    errAbort("%d total errors in file check", errCount);
freeHashAndVals(&hash);
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
if (ferror(f))
    errAbort("error writing %s", fileName);
carefulClose(&f);
}

void atomicWriteBatch(struct jobDb *db, char *fileName)
/* Wrapper to avoid corruption file by two para process being run in the same
 * directory. */
{
char hostName[128];
char tmpName[PATH_LEN];

/* generate a unique name for tmp file */
if (gethostname(hostName, sizeof(hostName)) < 0)
    errnoAbort("can't get host name");
safef(tmpName, sizeof(tmpName), "%s.%d.%s.tmp", fileName, getpid(), 
      hostName);

writeBatch(db, tmpName);

/* now rename (which is attomic) */
if (rename(tmpName, fileName) < 0)
    errnoAbort("can't rename %s to %s", tmpName, fileName);
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
verbose(1, "%d jobs in batch\n", db->jobCount);
return db;
}

char *hubSingleLineQuery(char *query)
/* Send message to hub and get single line response.
 * This should be freeMem'd when done. */
{
struct rudp *ru = rudpMustOpen();
struct paraMessage pm;

pmInitFromName(&pm, "localhost", paraHubPort);
if (!pmSendString(&pm, ru, query))
    noWarnAbort();
if (!pmReceive(&pm, ru))
    noWarnAbort();
rudpClose(&ru);
return cloneString(pm.data);
}

struct slRef *hubMultilineQuery(char *query)
/* Send a command with a multiline response to hub,
 * and return response as a list of strings. */
{
struct slRef *list = NULL;
struct rudp *ru = rudpMustOpen();
struct paraMessage pm;
pmInitFromName(&pm, "localhost", paraHubPort);
if (!pmSendString(&pm, ru, query))
    noWarnAbort();
for (;;)
    {
    if (!pmReceive(&pm, ru))
	break;
    if (pm.size == 0)
	break;
    refAdd(&list, cloneString(pm.data));
    }
rudpClose(&ru);
slReverse(&list);
return list;
}

boolean batchRunning(char *batchName)
/* Return TRUE if a batch is running. */
{
struct slRef *lineList = hubMultilineQuery("listBatches"), *lineEl;
boolean ret = FALSE;
for (lineEl = lineList; lineEl != NULL; lineEl = lineEl->next)
    {
    int wordCount;
    char *line = lineEl->val;
    char *row[8];
    if (line[0] != '#')
	{
	char *b;
	wordCount = chopLine(line, row);
	b = row[7];
	if (wordCount < 8 || b[0] != '/')
	    errAbort("paraHub and para out of sync on listBatches");
	if (sameString(b, batchName))
	    ret = TRUE;
	}
    freez(&lineEl->val);
    }
slFreeList(&lineList);
return ret;
}

boolean thisBatchRunning()
/* Return true if this batch is running */
{
char batchDir[512];

if (getcwd(batchDir, sizeof(batchDir)) == NULL)
    errAbort("Couldn't get current directory");
strcat(batchDir, "/");
return batchRunning(batchDir);
}

struct jobDb *parseJobList(char *jobList)
/* parse a job list */
{
struct lineFile *lf = lineFileOpen(jobList, TRUE);
char *line;
struct jobDb *db;
struct job *job;
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
return db;
}



void paraCreate(char *batch, char *jobList)
/* Create a batch database from a job list. */
{
char backup[512];
struct jobDb *db;

if (thisBatchRunning())
    errAbort("This batch is currently running.  Please para stop first.");
makeDir("err");
db = parseJobList(jobList);

doChecks(db, "in");
sprintf(backup, "%s.bak", batch);
atomicWriteBatch(db, backup);
atomicWriteBatch(db, batch);
verbose(1, "%d jobs written to %s\n", db->jobCount, batch);
checkPrioritySetting();
checkMaxNodeSetting();
}

void paraRecover(char *batch, char *jobList, char *newJobList)
/* Create a new job list from an existing list based on check outs that
 * failed. */
{
struct jobDb *db;
struct job *job;
struct hash *hash = newHash(19);
FILE *newFh = mustOpen(newJobList, "w");
db = parseJobList(jobList);

beginHappy();
for (job = db->jobList; job != NULL; job = job->next)
    {
    if (checkOneJob(job, "out", hash, NULL))
        fprintf(newFh, "%s\n", job->spec);
    }
endHappy();
if (ferror(newFh))
    errAbort("write failed: %s", newJobList);
carefulClose(&newFh);
jobDbFree(&db);
freeHash(&hash);
}

boolean submitJob(struct job *job, char *curDir)
/* Attempt to submit job. */
{
struct dyString *cmd = dyStringNew(1024);
struct submission *sub;
char *jobId = NULL;

dyStringPrintf(cmd, "addJob %s %s /dev/null /dev/null %s/para.results %s", getUser(), curDir, curDir, job->command);
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

int markQueuedJobs(struct jobDb *db)
/* Mark jobs that are queued up. Return total number of jobs in queue. */
{
struct hash *hash = newHash(0);
struct job *job;
struct submission *sub;
int queueSize = 0;
char curDir[512];

if (getcwd(curDir, sizeof(curDir)) == NULL)
    errAbort("Couldn't get current directory");

struct dyString *dy = newDyString(1024);
dyStringPrintf(dy, "pstat %s %s/para.results", getUser(), curDir);
struct slRef *lineList = hubMultilineQuery(dy->string), *lineEl;
dyStringFree(&dy);

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
verbose(1, "%d jobs (including everybody's) in Parasol queue.\n", queueSize);

/* Read status output. */
for (lineEl = lineList; lineEl != NULL; lineEl = lineEl->next)
    {
    int wordCount;
    char *line = lineEl->val;
    char *row[6];
    wordCount = chopLine(line, row);
    if (wordCount < 6)
	{
	warn("Expecting at least 6 words in pstat output. paraHub and para out of sync.");
	statusOutputChanged();
	}
    else
	{
	char *state = row[0], *jobId = row[1], *ticks = row[4], *host = row[5];
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


boolean isStatusOk(int status)
/* Convert wait() return status to return value. */
{
return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
}

double jrCpuTime(struct jobResult *jr)
/* Get CPU time in seconds for job. */
{
return 0.01 * (jr->usrTicks + jr->sysTicks);
}

double jrRealTime(struct jobResult *jr)
/* Get real time in seconds for job. */
{
/* note jr->*Time are unsigned, so we need to convert to double
 * before subtracting or time moving backwards is not detected
 */
return ((double)jr->endTime) - ((double)jr->startTime);
}

struct hash *markRunJobStatus(struct jobDb *db)
/* Mark jobs based on results output file. 
 * Returns hash of results. */
{
struct job *job;
struct submission *sub;
struct hash *checkHash = newHash(0);
struct hash *resultsHash = hashResults(resultsName);

verbose(1, "Checking finished jobs\n");
beginHappy();
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
		sub->status = jr->status;
		sub->gotStatus = TRUE;
		sub->host = cloneString(jr->host);
		sub->inQueue = FALSE;
		sub->running = FALSE;
		if (isStatusOk(sub->status) && checkOneJob(job, "out", checkHash, stderr) == 0)
		    sub->ranOk = TRUE;
		else
		    sub->crashed = TRUE;
		}
	    }
	}
    }
endHappy();
freeHash(&checkHash);
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

beginHappy();
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
            if (delayTime > 0)
                {
                atomicWriteBatch(db, batch);
                sleep(delayTime);
                }
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
endHappy();
atomicWriteBatch(db, batch);
verbose(1, "updated job database on disk\n");
if (pushCount > 0)
    verbose(1, "Pushed Jobs: %d\n", pushCount);
if (retryCount > 0)
    verbose(1, "Retried jobs: %d\n", retryCount);
freeResults(&resultsHash);
return db;
}

void paraPush(char *batch)
/* Push forward batch one time. */
{
struct jobDb *db = paraCycle(batch);
jobDbFree(&db);
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
    fflush(stdout);
    fflush(stderr);
    sleep(curSleep);
    if (curSleep < maxSleep)
        curSleep += 15;
    now = time(NULL);
    verbose(1, "Checking job status %d minutes after launch\n",  round((now-start)/60.0));
    }
verbose(1, "Successful batch!\n");
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
    slow = 0, hung = 0, ranOk = 0, unsubmitted = 0, total = 0, failed = 0;
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
	if ((job->submissionCount >= retries && needsRerun(sub)) || hung)
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
   {
   if (!fileExists(resultsName))
       printf("para.results: file not found.  paraHub can't write to this dir?\n");
   else
       printf("tracking errors: %d\n", trackingError);
   }
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

markQueuedJobs(db);
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

struct submission* getLastSubmission(struct job *job)
/* get the last submission for a job, or NULL if not submitted */
{
struct submission* sub = job->submissionList;
while ((sub != NULL) && (sub->next != NULL))
    sub = sub->next;
return sub;
}

/* header for job status output */
static char *jobStatusHdr =
    "#state\ttries\treal\tcpu\thost\tjobid\tcmd\n";

void paraJobStatus(struct job *job, struct hash *resultsHash, time_t now)
/* Print status of a job. */
{
enum jaState state = figureState(job);
char *stateStr = jaStateShortDesc[state];
double realTime = 0.0;
double cpuTime = 0.0;
struct submission *sub = getLastSubmission(job);
struct jobResult *jr = NULL;
char *jobId = "";
char *host = "";

if (sub != NULL)
    {
    jr = hashFindVal(resultsHash, sub->id);
    if (sub->trackingError)
        stateStr = "track";
    if (state == jaRunning)
        realTime = (now - sub->startTime);
    else if (jr != NULL)
        {
        realTime = jrRealTime(jr);
        cpuTime = jrCpuTime(jr);
        jobId = jr->jobId;
        }
    host = sub->host;
    }

subChar(job->command, '\t', ' ');  /* tabs not allowed in column */

printf("%s\t%d\t%0.2f\t%0.2f\t%s\t%s\t%s\n",
       stateStr,
       job->submissionCount,
       realTime, cpuTime, host, jobId,
       job->command);
}

void paraStatus(char *batch)
/* Print status of all jobs. */
{
struct jobDb *db = readBatch(batch);
struct job *job;
time_t now = time(NULL);
struct hash *resultsHash;

markQueuedJobs(db);
resultsHash = markRunJobStatus(db);
printf(jobStatusHdr);
for (job = db->jobList; job != NULL; job = job->next)
    paraJobStatus(job, resultsHash, now);
}

void fetchOpenFile(struct paraMessage *pm, struct rudp *ru, char *fileName)
/* Read everything you can from socket and output to file. */
{
FILE *f = mustOpen(fileName, "w");
while (pmReceive(pm, ru))
    {
    if (pm->size == 0)
	break;
    mustWrite(f, pm->data, pm->size);
    }
carefulClose(&f);
}

void fetchFile(char *host, char *sourceName, char *destName)
/* Fetch small file. */
{
struct rudp *ru = rudpOpen();
struct paraMessage pm;
if (ru != NULL)
    {
    pmInitFromName(&pm, host, paraNodePort);
    pmPrintf(&pm, "fetch %s %s", getUser(), sourceName);
    if (pmSend(&pm, ru))
	fetchOpenFile(&pm, ru, destName);
    rudpClose(&ru);
    }
}

void printErrFile(struct submission *sub, struct jobResult *jr)
/* Print error file if it exists. */
{
char localName[64];
sprintf(localName, "err/%s", jr->jobId);
if (!fileExists(localName))
    {
    fetchFile(jr->host, jr->errFile, localName);
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
    printf("start time: %s", ctime(&startTime)); /* ctime adds \n */
    printf("return: ");
    if (WIFEXITED(jr->status))
        printf("%d\n", WEXITSTATUS(jr->status));
    else if (WIFSIGNALED(jr->status))
        printf("signal %d\n", WTERMSIG(jr->status));
    else if (WIFSTOPPED(jr->status))
        printf("stopped %d\n", WSTOPSIG(jr->status));
    else 
        printf("unknow wait status %d\n", jr->status);
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
time_t startTime = sub->startTime;
int duration = time(NULL) - startTime;

printf("command: %s\n", job->command);
printf("jobId: %s\n", sub->id);
printf("host: %s\n", sub->host);
printf("start time: %s", ctime(&startTime)); /* ctime adds \n */
printf("run time so far: %d sec,  %4.2f min, %4.2f hours,  %4.2f days\n", 
	duration, duration/60.0, duration/3600.0,  duration/(3600.0*24.0));
printf("\n");
}

void paraRunning(char *batch, char *minTimeS)
/*  List jobs that are running.  Includes host and time info */
{
struct jobDb *db = readBatch(batch);
struct job *job;
struct submission *sub;
int runCount = 0;
int minTime = 0;

if (minTimeS)
    minTime = atoi(minTimeS);
markQueuedJobs(db);
markRunJobStatus(db);
for (job = db->jobList; job != NULL; job = job->next)
    {
    if ((sub = job->submissionList) != NULL && sub->running)
	{
	time_t startTime = sub->startTime;
	int duration = time(NULL) - startTime;
	if (duration > (minTime * 60))
	    {
	    runningReport(job, sub);
	    ++runCount;
	    }
	}
    }
printf("total %s running: %d\n", !minTimeS ? "jobs" : "hippos", runCount);
}


void sendChillMessage()
/* Tell hub to chill out on job */
{
struct dyString *dy = newDyString(1024);
char curDir[512];
char *result;
if (getcwd(curDir, sizeof(curDir)) == NULL)
    errAbort("Couldn't get current directory");
dyStringPrintf(dy, "chill %s %s/para.results", getUser(), curDir);
result = hubSingleLineQuery(dy->string);
dyStringFree(&dy);
if (result == NULL || !sameString(result, "ok"))
    errAbort("Couldn't chill %s\n", curDir);
freez(&result);
verbose(1, "Told hub to chill out\n");
}

void sendSetMaxNodeMessage(int maxNode)
/* Tell hub to change maxNode on batch */
{
struct dyString *dy = newDyString(1024);
char curDir[512];
char *result;
if (maxNode <-1) 
    errAbort("maxNode %d out of range, should be >=-1",maxNode);
if (getcwd(curDir, sizeof(curDir)) == NULL)
    errAbort("Couldn't get current directory");
dyStringPrintf(dy, "setMaxNode %s %s/para.results %d", getUser(), curDir, maxNode);
result = hubSingleLineQuery(dy->string);
dyStringFree(&dy);
if (result == NULL || sameString(result, "-2"))
    errAbort("Couldn't set maxNode %d for %s\n", maxNode, curDir);
freez(&result);
verbose(1, "Told hub to set maxNode %d\n",maxNode);
}

void paraMaxNode(char *val)
/* Tell hub to change maxNode on batch */
{
if (sameWord(val,"unlimited"))
    maxNode = -1;
else
    maxNode = atoi(val);
sendSetMaxNodeMessage(maxNode);
}

void checkMaxNodeSetting()
/* see if we can and need to set maxNode */
{
if (optionVal("maxNode",NULL)!=NULL)
    paraMaxNode(optionVal("maxNode","unlimited"));
}   



void sendSetPriorityMessage(int priority)
/* Tell hub to change priority on batch */
{
struct dyString *dy = newDyString(1024);
char curDir[512];
char *result;
if ((priority < 1) || (priority > MAX_PRIORITY))
    errAbort("Priority %d out of range, should be 1 to %d",priority,MAX_PRIORITY);
if (getcwd(curDir, sizeof(curDir)) == NULL)
    errAbort("Couldn't get current directory");
dyStringPrintf(dy, "setPriority %s %s/para.results %d", getUser(), curDir, priority);
result = hubSingleLineQuery(dy->string);
dyStringFree(&dy);
if (result == NULL || sameString(result, "0"))
    errAbort("Couldn't set priority for %s\n", curDir);
freez(&result);
verbose(1, "Told hub to set priority %d\n",priority);
}

void paraPriority(char *val)
/* Tell hub to change priority on batch */
{
if (sameWord(val,"high"))
    priority = 1;
else if (sameWord(val,"medium"))
    priority = NORMAL_PRIORITY;
else if (sameWord(val,"low"))
    priority = NORMAL_PRIORITY * NORMAL_PRIORITY;
else
    priority = atoi(val);
sendSetPriorityMessage(priority);
}

void checkPrioritySetting()
/* see if we can and need to set the priority */
{
if (optionVal("pri",NULL)!=NULL)
    paraPriority(optionVal("pri","medium"));
if (optionVal("priority",NULL)!=NULL)
    paraPriority(optionVal("priority","medium"));
}   


void clearSickNodes()
/* Tell hub to show sick nodes on batch */
{
struct dyString *dy = newDyString(1024);
char curDir[512];
char *result;
if (getcwd(curDir, sizeof(curDir)) == NULL)
    errAbort("Couldn't get current directory");
dyStringPrintf(dy, "clearSickNodes %s %s/para.results", getUser(), curDir);
result = hubSingleLineQuery(dy->string);
dyStringFree(&dy);
if (!sameString(result, "0"))
    errAbort("Couldn't clear sick nodes for %s\n", curDir);
freez(&result);
verbose(1, "Told hub to clear sick nodes\n");
}


void showSickNodes()
/* Tell hub to show sick nodes on batch */
{
struct dyString *dy = newDyString(1024);
char curDir[512];
if (getcwd(curDir, sizeof(curDir)) == NULL)
    errAbort("Couldn't get current directory");
dyStringPrintf(dy, "showSickNodes %s %s/para.results", getUser(), curDir);
struct slRef *lineList = hubMultilineQuery(dy->string), *lineEl;
for (lineEl = lineList; lineEl != NULL; lineEl = lineEl->next)
    {
    char *line = lineEl->val;
    printf("%s\n", line);
    freez(&lineEl->val);
    }
slFreeList(&lineList);
dyStringFree(&dy);
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
verbose(1, "Chilled %d jobs\n", count);
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

boolean killJob(char *jobId)
/* Tell hub to kill a job.  Return TRUE on
 * success. */
{
char buf[256];
char *result = NULL;
boolean ok;
snprintf(buf, sizeof(buf), "removeJob %s", jobId);
result = hubSingleLineQuery(buf);
ok = (result != NULL && sameString(result, "ok"));
freez(&result);
return ok;
}

void paraStopAll(char *batch)
/* Stop batch of jobs. */
{
struct jobDb *db = readBatch(batch);
struct job *job;
struct submission *sub;
int killCount = 0, missCount = 0;

markQueuedJobs(db);
markRunJobStatus(db);
cleanTrackingErrors(db);
for (job = db->jobList; job != NULL; job = job->next)
    {
    sub = job->submissionList;
    if (sub != NULL)
        {
	if (sub->inQueue || sub->running)
	    {
	    if (killJob(sub->id))
		{
		job->submissionCount -= 1;
		job->submissionList = sub->next;
		killCount += 1;
		}
	    else
	        {
		missCount += 1;
		}
	    }
	}
    }
verbose(1, "%d running jobs stopped\n", killCount);
if (missCount > 0)
    printf("%d jobs not stopped - try another para stop in a little while\n",
    	missCount);
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
printf("%-27s %9llds %10.2fm %8.2fh %7.2fd", 
   title, roundll(seconds), seconds/60, seconds/3600, seconds/(3600*24));
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
double runTime = 0, longestRun = 0;
int otherCount = 0;
struct hash *resultsHash;
long now = time(NULL);
double ioTime;

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
	       warn("Strange start time in %s: %u", batch, sub->startTime);
	   else
	       runTime += oneTime;
	   if (oneTime > longestRun)
	     longestRun = oneTime;
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
	       oneWall = jrRealTime(jr);
	       if (oneWall < 0)	/* Protect against clock reset. */
		   {
		   warn("End before start job %s host %s", sub->id, jr->host);
		   warn("Start %u,  End %u", jr->startTime, jr->endTime);
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
    {
    if (!fileExists(resultsName))
	printf("para.results: file not found.  paraHub can't write to this dir?\n");
    else
	printf("Other count: %d jobs\n", otherCount);
    }
if (queueCount > 0)
    printf("In queue waiting: %d jobs\n", queueCount);
printTimes("CPU time in finished jobs:", totalCpu, TRUE);
ioTime = totalWall - totalCpu;
if (ioTime < 0) ioTime = 0;
printTimes("IO & Wait Time:", ioTime, TRUE);
if (runningCount > 0)
    {
    printTimes("Time in running jobs:", runTime, TRUE);
    }
if (timedCount > 0)
    {
    printTimes("Average job time:", totalWall/timedCount, FALSE);
    if (runningCount > 0)
        printTimes("Longest running job:", longestRun, FALSE);
    printTimes("Longest finished job:", longestWall, FALSE);
    printTimes("Submission to last job:", calcFirstToLast(db), FALSE);
    if (runningCount < 1)
        printTimes("Estimated complete:", 0.0, FALSE);
    else
        {
        int jobsInBatch = db->jobCount;
        int jobsToRun = jobsInBatch - timedCount;
        double timeMultiple = (double)jobsToRun / (double)runningCount;
        verbose(2, "inBatch: %d, toRun: %d, multiple: %.3f\n",
		jobsInBatch, jobsToRun, timeMultiple);
        printTimes("Estimated complete:",
                   timeMultiple*totalWall/timedCount, FALSE);
	}
    }
atomicWriteBatch(db, batch);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;
char *batch;

optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
retries = optionInt("retries",  retries);
maxQueue = optionInt("maxQueue",  maxQueue);
minPush = optionInt("minPush",  minPush);
maxPush = optionInt("maxPush",  maxPush);
warnTime = optionInt("warnTime", warnTime);
killTime = optionInt("killTime", killTime);
delayTime = optionInt("delayTime", delayTime);
if (optionExists("eta"))
    fprintf(stderr, "note: the -eta option is no longer required\n");
command = argv[1];
batch = "batch";

if (!sameWord(command,"create") && !sameWord(command,"make"))
    {
    checkPrioritySetting();
    checkMaxNodeSetting();
    }

pushWarnHandler(paraVaWarn);

if (strchr(batch, '/') != NULL)
    errAbort("para needs to be run in the same directory as the batch file.");
if (sameWord(command, "create") || sameWord(command, "creat"))
    {
    if (argc != 3)
        usage();
    paraCreate(batch, argv[2]);
    }
else if (sameWord(command, "recover"))
    {
    if (argc != 4)
        usage();
    paraRecover(batch, argv[2], argv[3]);
    }
else if (sameWord(command, "check"))
    {
    paraCheck(batch);
    }
else if (sameWord(command, "push"))
    {
    paraPush(batch);
    }
else if (sameWord(command, "shove"))
    {
    paraShove(batch);
    }
else if (sameWord(command, "make"))
    {
    if (argc != 3)
        usage();
    paraMake(batch, argv[2]);
    }
else if (sameWord(command, "try"))
    {
    maxPush = 10;
    paraPush(batch);
    }
else if (sameWord(command, "stop"))
    {
    paraStop(batch);
    }
else if (sameWord(command, "chill"))
    {
    paraChill(batch);
    }
else if (sameWord(command, "hung"))
    {
    paraListState(batch, jaHung);
    }
else if (sameWord(command, "crashed"))
    {
    paraListState(batch, jaCrashed);
    }
else if (sameWord(command, "failed"))
    {
    paraListFailed(batch);
    }
else if (sameWord(command, "finished"))
    {
    paraListState(batch, jaFinished);
    }
else if (sameWord(command, "status"))
    {
    paraStatus(batch);
    }
else if (sameWord(command, "problems") || sameWord(command, "problem"))
    {
    paraProblems(batch);
    }
else if (sameWord(command, "running") || sameWord(command, "hippos") || sameWord(command, "hippo"))
    {
    paraRunning(batch, argv[2]);
    }
else if (sameWord(command, "time") || sameWord(command, "times"))
    {
    paraTimes(batch);
    }
else if (sameWord(command, "priority"))
    {
    if (argc != 3)
        usage();
    paraPriority(argv[2]);
    }
else if (sameWord(command, "maxNode"))
    {
    if (argc != 3)
        usage();
    paraMaxNode(argv[2]);
    }
else if (sameWord(command, "clearSickNodes"))
    {
    clearSickNodes();
    }
else if (sameWord(command, "showSickNodes"))
    {
    showSickNodes();
    }
else
    {
    errAbort("Unrecognized command '%s'.  Run para with no arguments for usage summary", 
    	command);
    }
return 0;
}
