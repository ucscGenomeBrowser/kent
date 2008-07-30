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
#include "sqlNum.h"

static char const rcsid[] = "$Id: para.c,v 1.102 2008/06/28 05:10:35 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"retries"  , OPTION_INT},
    {"maxQueue" , OPTION_INT},
    {"minPush"  , OPTION_INT},
    {"maxPush"  , OPTION_INT},
    {"warnTime" , OPTION_INT},
    {"killTime" , OPTION_INT},
    {"delayTime", OPTION_INT},
    {"eta"      , OPTION_BOOLEAN},
    {"pri"      , OPTION_STRING},
    {"priority" , OPTION_STRING},
    {"maxJob"   , OPTION_STRING},
    {"cpu"      , OPTION_FLOAT},
    {"ram"      , OPTION_STRING},
    {"batch"    , OPTION_STRING},
    {"jobCwd"   , OPTION_STRING},
    {NULL, 0}
};

char *version = PARA_VERSION;   /* Version number. */

static int numHappyDots;       /* number of happy dots written */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "para - version %s\n"
  "Manage a batch of jobs in parallel on a compute cluster.\n"
  "Normal usage is to do a 'para create' followed by 'para push' until\n"
  "job is done.  Use 'para check' to check status.\n"
  "usage:\n\n"
  "   para [options] command [command-specific arguments]\n\n"
  "The commands are:\n"
  "\n"
  "para create jobList\n"
  "   This makes the job-tracking database from a text file with the\n"
  "   command line for each job on a separate line.\n"
  "   options:\n"
  "      -cpu=N  Number of CPUs used by the jobs, default 1.\n"
  "      -ram=N  Number of bytes of RAM used by the jobs.\n"
  "         Default is RAM on node divided by number of cpus on node.\n"
  "         Shorthand expressions allow t,g,m,k for tera, giga, mega, kilo.\n"
  "         e.g. 4g = 4 Gigabytes.\n"
  "      -batch=batchDir - specify the directory path that is used to store the\n"
  "       batch control files.  The batchDir can be an absolute path or a path\n"
  "       relative to the current directory.  The resulting path is use as the\n"
  "       batch name.  The directory is created if it doesn't exist.  When\n"
  "       creating a new batch, batchDir should not have been previously used as\n"
  "       a batch name.  The batchDir must be writable by the paraHub process.\n"
  "       This does not affect the working directory assigned to jobs.  In defaults\n"
  "       to the directory where para is run.  If used, this option must be specified\n"
  "       on all para commands for the  batch.  For example to run two batches in the\n"
  "       same directory:\n"
  "          para -batch=b1 make jobs1\n"
  "          para -batch=b2 make jobs2\n"
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
  "      -killTime=N  Number of minutes hung job runs before push kills it.\n"
  "         By default kill off for backwards compatibility.\n" /* originally to be 2 weeks */
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
  "      -maxJob=x  Limit the number of jobs the batch can run.\n"
  "         Specify number of jobs, for example 10 or 'unlimited'.\n"
  "         Default unlimited displays as -1.\n"
  "      -jobCwd=dir - specify the directory path to use as the current working\n"
  "       directory for each job.  The dir can be an absolute path or a path\n"
  "       relative to the current directory. In defaults to the directory where\n"
  "       para is run.\n"
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
  "   List hung jobs in the batch (running > killTime).\n"
  "para slow\n"
  "   List slow jobs in the batch (running > warnTime).\n"
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
  "para recover jobList newJobList\n"
  "   Generate a job list by selecting jobs from an existing list where\n"
  "   the `check out' tests fail.\n"
  "para priority 999\n"
  "   Set batch priority. Values explained under 'push' options above.\n"
  "para maxJob 999\n"
  "   Set batch maxJob. Values explained under 'push' options above.\n"
  "para resetCounts\n"
  "   Set batch done and crash counters to 0.\n"
  "para freeBatch\n"
  "   Free all batch info on hub.  Works only if batch has nothing queued or running.\n"
  "para showSickNodes\n"
  "   Show sick nodes which have failed when running this batch.\n"
  "para clearSickNodes\n"
  "   Clear sick nodes statistics and consecutive crash counts of batch.\n"
  "\n"
  "Common options\n"
  "   -verbose=1 - set verbosity level.\n",
  version,
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
int killTime = 0;  /* 0 = off, was originally going to be 2 weeks or 14*24*60; */
int sleepTime = 5*60;
int delayTime = 0;
int priority = NORMAL_PRIORITY;
int maxJob  = -1;
float cpuUsage = 0;
long long ramUsage = 0;
char *batchDir = NULL;
char *jobCwd = NULL;
char resultsName[PATH_LEN];
char errDir[PATH_LEN];

/* Some variable we might want to move to a config file someday. */
char *resultsFileName = "para.results"; /* Name of results file. */
char *statusCommand = "parasol pstat2";
char *killCommand = "parasol remove job";

boolean sickBatch = FALSE;

char *bookMarkFileName = "para.bookmark";  /* name of bookmark file */
char bookMarkName[PATH_LEN];  /* path to bookmark file */
off_t bookMark = 0;  /* faster to resume from bookmark only reading new para.results */ 
off_t resultsSize = 0;  /* where to stop reading results for the current cycle */

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


long long parseRam(char *ram)
/* Parse RAM expression like 2000000, 2t, 2g, 2m, 2k
 * Returns long long number of bytes, or -1 for error
 * The value of input variable ram may be modified. */
{
long long result = -1, factor = 1;
int len = strlen(ram);
int i;
char saveC = ' ';
if (len == 0)
    return result;
if (ram[len-1] == 't')
    factor = (long long)1024 * 1024 * 1024 * 1024;
else if (ram[len-1] == 'g')
    factor = 1024 * 1024 * 1024;
else if (ram[len-1] == 'm')
    factor = 1024 * 1024;
else if (ram[len-1] == 'k')
    factor = 1024;
if (factor != 1)
    {
    --len;
    saveC = ram[len];
    ram[len] = 0;
    }
for (i=0; i<len; ++i)
    if (!isdigit(ram[i]))
	return result;
result = factor * sqlLongLong(ram);
if (factor != 1)
    ram[len] = saveC;
return result;
}

char *useWhats[] = {"cpu", "ram"};

void parseUsage(struct job* job, struct lineFile *lf)
/* Parse out and keep CPU and RAM usage from the job command. */
{
/* allow command-line options to be overridden by spec */
if (cpuUsage != 0)
    job->cpusUsed = cpuUsage;
if (ramUsage != 0)
    job->ramUsed = ramUsage;
char *pattern = "{use";
char *s, *e, *z;
char *line = job->command;
struct dyString *dy = dyStringNew(1024);
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
	char *parts[4];
	int partCount;
	dyStringAppendN(dy, s, e-s);
	z = strchr(e, '}');
	if (z == NULL)
	    errAbort("{use without } line %d of %s", lf->lineIx, lf->fileName);
	*z = 0;
	partCount = chopLine(e, parts);
	if (partCount != 3)
	    errAbort("Badly formatted 'use' clause in line %d of %s", lf->lineIx, lf->fileName);
	if (stringIx(parts[1], useWhats) < 0)
	    errAbort("Unrecognized word '%s' in 'use' clause line %d of %s", 
	    	parts[1], lf->lineIx, lf->fileName);
	if (sameString(parts[1], "cpu"))
	    {
    	    job->cpusUsed = sqlFloat(parts[2]);
	    }
	if (sameString(parts[1], "ram"))
	    {
    	    job->ramUsed = parseRam(parts[2]);
	    if (job->ramUsed == -1)
		errAbort("Invalid RAM expression '%s' in 'use' clause line %d of %s", 
		    parts[2], lf->lineIx, lf->fileName);
	    }
	s = z+1;
	}
    }
freeMem(job->command);
job->command = cloneString(dy->string);
dyStringFree(&dy);
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
parseUsage(job, lf);
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

void readBookMark()
/* Read the value of the bookMark, an offset into the results file.
 * This should be called before reading the results file. */
{
/* a bookmark should not exist if results file does not exist */
if ((!fileExists(resultsName)) && fileExists(bookMarkName))
    unlink(bookMarkName);
/* read bookmark position */
struct lineFile *lf = lineFileMayOpen(bookMarkName, TRUE);
char *line;
if (!lf)
    return;
if (lineFileNext(lf, &line, NULL))
    {
    bookMark = sqlLongLong(line);
    }
lineFileClose(&lf);
}

void writeBookMark()
/* Save the value of the bookMark, an offset into the results file.
 * This should be called after the batch has been safely updated. */
{
FILE *f = mustOpen(bookMarkName, "w");
fprintf(f, "%llu\n", (unsigned long long) bookMark);
carefulClose(&f);
}

void atomicWriteBatch(struct jobDb *db, char *fileName)
/* Wrapper to avoid corruption file by two para process being run in the same
 * directory. */
{
char hostName[128];
char tmpName[PATH_LEN];
long time = clock1000();

/* generate a unique name for tmp file */
if (gethostname(hostName, sizeof(hostName)) < 0)
    errnoAbort("can't get host name");
safef(tmpName, sizeof(tmpName), "%s.%d.%s.tmp", fileName, getpid(), 
      hostName);

writeBatch(db, tmpName);

/* now rename (which is attomic) */
if (rename(tmpName, fileName) < 0)
    errnoAbort("can't rename %s to %s", tmpName, fileName);

writeBookMark();
verbose(2, "atomicWriteBatch time: %.2f seconds\n", (clock1000() - time) / 1000.0);
}

struct jobDb *readBatch(char *batch)
/* Read a batch file. */
{
struct jobDb *db;
struct job *job;
struct lineFile *lf = lineFileOpen(batch, TRUE);
char *line;
long time = clock1000();

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
verbose(2, "readBatch time: %.2f seconds\n", (clock1000() - time) / 1000.0);
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
struct paraMultiMessage pmm;
char *row[256];
int count = 0;
pmInitFromName(&pm, "localhost", paraHubPort);
/* ensure the multi-message response comes from the correct ip and has no duplicate msgs*/
pmmInit(&pmm, &pm, pm.ipAddress.sin_addr);
if (!pmSendString(&pm, ru, query))
    noWarnAbort();
for (;;)
    {
    if (!pmmReceive(&pmm, ru))
	break;
    if (pm.size == 0)
	break;
    count = chopByChar(pm.data, '\n', row, sizeof(row));
    if (count > 1) --count;  /* for multiline, count is inflated by one */

    int i;
    for(i=0;i<count;++i)
        refAdd(&list, cloneString(row[i]));
    }
rudpClose(&ru);
slReverse(&list);
return list;
}

boolean batchRunning(char *batchName)
/* Return TRUE if a batch is running. */
{
#define NUMLISTBATCHCOLUMNS 12
struct slRef *lineList = hubMultilineQuery("listBatches"), *lineEl;
boolean ret = FALSE;
for (lineEl = lineList; lineEl != NULL; lineEl = lineEl->next)
    {
    int wordCount;
    char *line = lineEl->val;
    char *row[NUMLISTBATCHCOLUMNS];
    if (line[0] != '#')
	{
	char *b;
	wordCount = chopLine(line, row);
	b = row[NUMLISTBATCHCOLUMNS-1];
	if (wordCount < NUMLISTBATCHCOLUMNS || b[0] != '/')
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

void sendSetPriorityMessage(int priority)
/* Tell hub to change priority on batch */
{
struct dyString *dy = newDyString(1024);
char *result;
if ((priority < 1) || (priority > MAX_PRIORITY))
    errAbort("Priority %d out of range, should be 1 to %d",priority,MAX_PRIORITY);
dyStringPrintf(dy, "setPriority %s %s %d", getUser(), resultsName, priority);
result = hubSingleLineQuery(dy->string);
dyStringFree(&dy);
if (result == NULL || sameString(result, "0"))
    errAbort("Couldn't set priority for %s\n", batchDir);
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

void sendSetMaxJobMessage(int maxJob)
/* Tell hub to change maxJob on batch */
{
struct dyString *dy = newDyString(1024);
char *result;
if (maxJob <-1) 
    errAbort("maxJob %d out of range, should be >=-1", maxJob);
dyStringPrintf(dy, "setMaxJob %s %s %d", getUser(), resultsName, maxJob);
result = hubSingleLineQuery(dy->string);
dyStringFree(&dy);
if (result == NULL || sameString(result, "-2"))
    errAbort("Couldn't set maxJob %d for %s\n", maxJob, batchDir);
freez(&result);
verbose(1, "Told hub to set maxJob %d\n",maxJob);
}

void paraMaxJob(char *val)
/* Tell hub to change maxJob on batch */
{
if (sameWord(val,"unlimited"))
    maxJob = -1;
else
    maxJob = atoi(val);
sendSetMaxJobMessage(maxJob);
}

void checkMaxJobSetting()
/* see if we can and need to set maxJob */
{
if (optionVal("maxJob",NULL)!=NULL)
    paraMaxJob(optionVal("maxJob","unlimited"));
/* backwards compatibility */
if (optionVal("maxNode",NULL)!=NULL)
    paraMaxJob(optionVal("maxNode","unlimited"));
}   


void paraCreate(char *batch, char *jobList)
/* Create a batch database from a job list. */
{
char backup[PATH_LEN];
struct jobDb *db;

if (thisBatchRunning())
    errAbort("This batch is currently running.  Please para stop first.");
makeDir(batchDir);
makeDir(errDir);
db = parseJobList(jobList);

doChecks(db, "in");
safef(backup, sizeof(backup), "%s.bak", batch);
atomicWriteBatch(db, backup);
atomicWriteBatch(db, batch);
verbose(1, "%d jobs written to %s\n", db->jobCount, batch);
checkPrioritySetting();
checkMaxJobSetting();
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

boolean submitJob(struct job *job)
/* Attempt to submit job. */
{
struct dyString *cmd = dyStringNew(1024);
struct submission *sub;
char *jobId = NULL;

dyStringPrintf(cmd, "addJob2 %s %s /dev/null /dev/null %s %f %lld %s",
               getUser(), jobCwd, resultsName, job->cpusUsed, job->ramUsed, job->command);
if (cmd->stringSize > rudpMaxSize)
    errAbort("The following string has %d bytes, but can only be %d:\n%s\n"
             "Please either shorten the current directory or the command line\n"
             "possibly by making a shell script that encapsulates a long command.\n"
             ,  cmd->stringSize, (int)rudpMaxSize, cmd->string);
jobId = hubSingleLineQuery(cmd->string);
if (sameString(jobId,"0"))
    {
    warn("addJob failed - if batch is bad, correct problem and run para clearSickNodes.");
    freez(&jobId);
    }
if (jobId != NULL)
    {
    AllocVar(sub);
    slAddHead(&job->submissionList, sub);
    job->submissionCount += 1;
    sub->submitTime = time(NULL);
    sub->host = cloneString("n/a");
    sub->id = jobId;
    sub->inQueue = TRUE;
    sub->errFile = cloneString("n/a");
    }
dyStringFree(&cmd);
return jobId != NULL;
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


void statusOutputChanged()
/* Complain about status output format change and die. */
{
errAbort("\n%s output format changed, please update markQueuedJobs in para.c", 
	statusCommand);
}

int markQueuedJobs(struct jobDb *db)
/* Mark jobs that are queued up. Return total number of jobs in queue. */
{
struct hash *hash = newHash(max(12,digitsBaseTwo(db->jobCount)-3));

struct job *job;
struct submission *sub;
int queueSize = 0;

long killSeconds = killTime*60;
long warnSeconds = warnTime*60;
long duration;
time_t now = time(NULL);

long time = clock1000();

/* Get job list from paraHub. */
struct dyString *dy = newDyString(1024);
dyStringPrintf(dy, "pstat2 %s %s", getUser(), resultsName);
struct slRef *lineList = hubMultilineQuery(dy->string), *lineEl;
dyStringFree(&dy);

verbose(2, "pstat2 time: %.2f seconds\n", (clock1000() - time) / 1000.0);

long hashTime = clock1000();

verbose(2, "submission hash size: %d\n", hash->size);

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

verbose(2, "submission hash time: %.2f seconds\n", (clock1000() - hashTime) / 1000.0);

long pstatListTime = clock1000();

/* Read status output. */
for (lineEl = lineList; lineEl != NULL; lineEl = lineEl->next)
    {
    int wordCount;
    char *line = lineEl->val;
    char *row[6];
    if (startsWith("Total Jobs:", line))
	{
        wordCount = chopLine(line, row);
	queueSize = sqlSigned(row[2]);
	verbose(1, "%d jobs (including everybody's) in Parasol queue or running.\n", queueSize);
	freez(&lineEl->val);
	continue;
	}
    if (startsWith("Sick Batch:", line))
	{
	sickBatch = TRUE;
	warn(line);
	freez(&lineEl->val);
	continue;
	}
    if (startsWith("Results Size:", line))
	{
        wordCount = chopLine(line, row);
	resultsSize = sqlLongLong(row[2]);
	freez(&lineEl->val);
	continue;
	}
    wordCount = chopLine(line, row);
    if (wordCount < 6 && wordCount != 2)
	{
	warn("Expecting at least 6 words in pstat2 output,"
	     " found %d. paraHub and para out of sync.", wordCount);
	statusOutputChanged();
	}
    else
	{
	char *state = row[0], *jobId = row[1], *ticks = NULL, *host = NULL;
	time_t t = 0;
	if (wordCount > 2)
	    {
    	    ticks = row[4], host = row[5];
    	    t = atol(ticks);
	    }
	if ((sub = hashFindVal(hash, jobId)) != NULL)
	    {
	    if (sameString(state, "r"))
		{
		sub->running = TRUE;
		sub->startTime = t;
		if (sub->host)
		    freeMem(sub->host);
		sub->host = cloneString(host);
		duration = now - sub->startTime;
                if (duration < 0)
		    warn("Strange start time in jobId %s: %u", jobId, sub->startTime);
                else
		    {
		    if (duration > killSeconds && killSeconds > 0)
			{
    			sub->hung = TRUE;
			sub->slow = FALSE;
			sub->endTime = now;
			killJob(sub->id);
                        verbose(1, "killed hung jobId: %s\n", sub->id);
			}
		    else if (duration > warnSeconds)
			sub->slow = TRUE;
		    }
		}
	    else
		{
		sub->inQueue = TRUE;
		}
	    }
	}
    freez(&lineEl->val);
    }
verbose(2, "pstat list time: %.2f seconds\n", (clock1000() - pstatListTime) / 1000.0);

slFreeList(&lineList);

freeHash(&hash);
verbose(2, "markQueuedJobs time (includes pstat2, hash, list): %.2f seconds\n", (clock1000() - time) / 1000.0);
return queueSize;
}

struct hash *hashResults(char *fileName)
/* Return hash of job results keyed on id as string. */
{
long time = clock1000();
struct hash *hash = newHash(0);
if (fileExists(fileName))
    {
    struct jobResult *el, *list = jobResultLoadAll(fileName, &bookMark, resultsSize);
    for (el = list; el != NULL; el = el->next)
	hashAdd(hash, el->jobId, el);
    }
verbose(2, "hashResults time: %.2f seconds\n", (clock1000() - time) / 1000.0);
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

double subRealTime(struct submission *sub)
/* Get real time in seconds for job. */
{
/* note sub->*Time are unsigned, so we need to convert to double
 * before subtracting or time moving backwards is not detected
 */
return ((double)sub->endTime) - ((double)sub->startTime);
}

void showSickNodes(boolean showSummary)
/* Tell hub to show sick nodes on batch. */
{
int count = 0;
struct dyString *dy = newDyString(1024);
dyStringPrintf(dy, "showSickNodes %s %s", getUser(), resultsName);
struct slRef *lineList = hubMultilineQuery(dy->string), *lineEl;
for (lineEl = lineList; lineEl != NULL; lineEl = lineEl->next)
    {
    ++count;
    char *line = lineEl->val;
    /* In show summary mode, only print the last line, 
     * which contains the totals.  Only print this one
     * if there's more than one line (the total is greater than zero). */
    if (!showSummary || !(lineEl->next || count == 1))
	printf("%s\n", line);
    freez(&lineEl->val);
    }
slFreeList(&lineList);
dyStringFree(&dy);
}


void markRunJobStatus(struct jobDb *db)
/* Mark jobs based on results output file. 
 * Returns hash of results. */
{
struct job *job;
struct submission *sub;
struct hash *checkHash = newHash(0);
struct hash *resultsHash = hashResults(resultsName);
long time = clock1000();

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
		if (!sickBatch)
    		    sub->trackingError = 3;
		}
	    else
	        {
		sub->startTime = jr->startTime;
		sub->endTime = jr->endTime;
		sub->cpuTime = jrCpuTime(jr);
		sub->status = jr->status;
		sub->gotStatus = TRUE;
		if (sub->host)
		    freeMem(sub->host);
		sub->host = cloneString(jr->host);
		sub->inQueue = FALSE;
		sub->running = FALSE;
		sub->hung = FALSE;
		sub->slow = FALSE;
		if (sub->errFile)
		    freeMem(sub->errFile);
		sub->errFile = cloneString(jr->errFile);
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
freeResults(&resultsHash);
verbose(2, "markRunJobStatus time (includes hashResults): %.2f seconds\n", (clock1000() - time) / 1000.0);
}

boolean needsRerun(struct submission *sub)
/* Return TRUE if submission needs to be rerun. */
{
if (sub == NULL)
    return TRUE;
return sub->submitError || sub->queueError || sub->crashed || sub->trackingError || sub->hung;
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
long time = clock1000();

queueSize = markQueuedJobs(db);

markRunJobStatus(db);

beginHappy();
if (!sickBatch)
    for (tryCount=1; tryCount<=retries && !finished; ++tryCount)
	{
	for (job = db->jobList; job != NULL; job = job->next)
	    {
	    if (job->submissionCount < tryCount && 
	       (job->submissionList == NULL || needsRerun(job->submissionList)))
		{
		if (!submitJob(job))
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
verbose(2, "paraCycle time: %.2f seconds\n", (clock1000() - time) / 1000.0);
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
    showSickNodes(TRUE);
    fflush(stdout);
    fflush(stderr);
    if (sickBatch)
	break;
    sleep(curSleep);
    if (curSleep < maxSleep)
        curSleep += 15;
    now = time(NULL);
    verbose(1, "================\n");
    verbose(1, "Checking job status %d minutes after launch\n",  round((now-start)/60.0));
    }
if (sickBatch)
    errAbort("Sick batch! Correct problem and then run para clearSickNodes.");
else
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
	if (job->submissionCount >= retries && needsRerun(sub))
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
       printf("%s: file not found.  paraHub can't write to this dir?\n", resultsName);
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

void paraJobStatus(struct job *job, time_t now)
/* Print status of a job. */
{
enum jaState state = figureState(job);
char *stateStr = jaStateShortDesc[state];
double realTime = 0.0;
double cpuTime = 0.0;
struct submission *sub = getLastSubmission(job);
char *jobId = "";
char *host = "";

if (sub != NULL)
    {
    if (sub->trackingError)
        stateStr = "track";
    if (state == jaRunning)
        realTime = (now - sub->startTime);
    else
        {
        realTime = subRealTime(sub);
        cpuTime = sub->cpuTime;
        jobId = sub->id;
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

markQueuedJobs(db);
markRunJobStatus(db);
printf(jobStatusHdr);
for (job = db->jobList; job != NULL; job = job->next)
    paraJobStatus(job, now);
}

void fetchOpenFile(struct paraMessage *pm, struct rudp *ru, char *fileName)
/* Read everything you can from socket and output to file. */
{
struct paraMultiMessage pmm;
FILE *f = mustOpen(fileName, "w");
/* ensure the multi-message response comes from the correct ip and has no duplicate msgs*/
pmmInit(&pmm, pm, pm->ipAddress.sin_addr);
while (pmmReceive(&pmm, ru))
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

void printErrFile(struct submission *sub)
/* Print error file if it exists. */
{
char localName[PATH_LEN];
safef(localName, sizeof(localName), "%s/%s", errDir, sub->id);
if (!fileExists(localName))
    {
    fetchFile(sub->host, sub->errFile, localName);
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

void problemReport(struct job *job, struct submission *sub, char *type)
/* Print report on one problem. */
{
struct check *check;
struct hash *hash = newHash(0);

printf("job: %s\n", job->command);
printf("id: %s\n", sub->id);
printf("failure type: %s\n", type);
if (sameString(type, "crash"))
    {
    time_t startTime = sub->startTime;
    printf("host: %s\n", sub->host);
    printf("start time: %s", ctime(&startTime)); /* ctime adds \n */
    printf("return: ");
    if (WIFEXITED(sub->status))
	printf("%d\n", WEXITSTATUS(sub->status));
    else if (WIFSIGNALED(sub->status))
	printf("signal %d\n", WTERMSIG(sub->status));
    else if (WIFSTOPPED(sub->status))
	printf("stopped %d\n", WSTOPSIG(sub->status));
    else 
	printf("unknown wait status %d\n", sub->status);
    for (check = job->checkList; check != NULL; check = check->next)
	doOneCheck(check, hash, stdout);
    printErrFile(sub);
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

markQueuedJobs(db);
markRunJobStatus(db);
for (job = db->jobList; job != NULL; job = job->next)
    {
    for (sub = job->submissionList; sub != NULL; sub = sub->next)
	{
	if (sub->hung)
	    {
	    problemReport(job, sub, "hung");
	    ++problemCount;
	    }
	else if (sub->slow)
	    {
	    problemReport(job, sub, "slow");
	    ++problemCount;
	    }
	else if (sub->trackingError)
	    {
	    problemReport(job, sub, "tracking error");
	    ++problemCount;
	    }
	else if (needsRerun(sub))
	    {
	    problemReport(job, sub, "crash");
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
if (sub->slow)
  printf("This is a slow job, running time > %d minutes.\n", warnTime);
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
char *result;
dyStringPrintf(dy, "chill %s %s", getUser(), resultsName);
result = hubSingleLineQuery(dy->string);
dyStringFree(&dy);
if (result == NULL || !sameString(result, "ok"))
    errAbort("Couldn't chill %s\n", batchDir);
freez(&result);
verbose(1, "Told hub to chill out\n");
}

void paraResetCounts()
/* Send msg to hub to reset done and crashed counts on batch */
{
struct dyString *dy = newDyString(1024);
char *result;
dyStringPrintf(dy, "resetCounts %s %s", getUser(), resultsName);
result = hubSingleLineQuery(dy->string);
dyStringFree(&dy);
if (result == NULL || sameString(result, "-2"))
    errAbort("Couldn't reset done and crashed counts on batch %s\n", batchDir);
freez(&result);
verbose(1, "Told hub to reset done and crashed counts on batch %s\n", batchDir);
}


void freeBatch()
/* Send msg to hub to reset done and crashed counts on batch */
{
struct dyString *dy = newDyString(1024);
char *result;
dyStringPrintf(dy, "freeBatch %s %s", getUser(), resultsName);
result = hubSingleLineQuery(dy->string);
dyStringFree(&dy);
verbose(1, "Told hub to free all batch-related resources\n");
if (result == NULL)
    errAbort("result == NULL\n");
if (sameOk(result, "-3"))
    errAbort("User not found.\n");
if (sameOk(result, "-2"))
    errAbort("Batch not found.\n");
if (sameOk(result, "-1"))
    warn("Unable to free batch.  Jobs are queued or running.\n");
if (sameOk(result, "0"))
    verbose(1, "Batch freed.\n");
freez(&result);
}



void clearSickNodes()
/* Tell hub to clear sick nodes on batch */
{
struct dyString *dy = newDyString(1024);
char *result;
dyStringPrintf(dy, "clearSickNodes %s %s", getUser(), resultsName);
result = hubSingleLineQuery(dy->string);
dyStringFree(&dy);
if (!sameString(result, "0"))
    errAbort("Couldn't clear sick nodes for %s\n", batchDir);
freez(&result);
verbose(1, "Told hub to clear sick nodes\n");
}


void paraCheck(char *batch)
/* Check on progress of a batch. */
{
struct jobDb *db = readBatch(batch);

markQueuedJobs(db);
markRunJobStatus(db);
reportOnJobs(db);

atomicWriteBatch(db, batch);
showSickNodes(TRUE);
if (sickBatch)
    errAbort("Sick batch! Correct problem and then run para clearSickNodes.");
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
long now = time(NULL);
double ioTime;

markQueuedJobs(db);
markRunJobStatus(db);
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
	else if (sub->ranOk)
	   {
	   ++timedCount;
	   totalCpu += sub->cpuTime;
	   oneWall = subRealTime(sub);
	   if (oneWall < 0)	/* Protect against clock reset. */
	       {
	       warn("End before start job %s host %s", sub->id, sub->host);
	       warn("Start %u,  End %u", sub->startTime, sub->endTime);
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

printf("Completed: %d of %d jobs\n", timedCount, jobCount);
if (runningCount > 0)
    printf("Jobs currently running: %d\n", runningCount);
if (crashCount > 0)
    printf("Crashed: %d jobs\n", crashCount);
if (otherCount > 0)
    {
    if (!fileExists(resultsName))
	printf("%s: file not found.  paraHub can't write to this dir?\n", resultsName);
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

static char *determineCwdOptDir(char *optName)
/* determine batch or job directory from cwd and option */
{
char path[PATH_LEN];
char *opt = optionVal(optName, NULL);
if (opt == NULL)
    safecpy(path, sizeof(path), getCurrentDir());
else if (opt[0] != '/')
    {
    safecpy(path, sizeof(path), getCurrentDir());
    safecat(path, sizeof(path), "/");
    safecat(path, sizeof(path), opt);
    }
else
    safecpy(path, sizeof(path), opt);
return cloneString(path);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;
char batch[PATH_LEN];
long startTime = clock1000();

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
cpuUsage = optionFloat("cpu", cpuUsage);
if (cpuUsage < 0)
    usage();
ramUsage = parseRam(optionVal("ram","0"));
if (ramUsage == -1)
    usage();
batchDir = determineCwdOptDir("batch");
jobCwd = determineCwdOptDir("jobCwd");
safef(resultsName, sizeof(resultsName), "%s/%s", batchDir, resultsFileName);
safef(errDir, sizeof(errDir), "%s/err", batchDir);
safef(bookMarkName, sizeof(bookMarkName), "%s/%s", batchDir, bookMarkFileName);

command = argv[1];
safef(batch, sizeof(batch), "%s/batch", batchDir);

if (!sameWord(command,"create") && !sameWord(command,"make"))
    {
    checkPrioritySetting();
    checkMaxJobSetting();
    }

pushWarnHandler(paraVaWarn);

readBookMark();  /* read the para.bookmark file to initialize bookmark */

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
else if (sameWord(command, "slow"))
    {
    char temp[256];
    safef(temp, sizeof(temp), "%d", warnTime);
    paraRunning(batch, temp);
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
else if (sameWord(command, "maxJob") || sameWord(command, "maxNode"))
    {
    if (argc != 3)
        usage();
    /* backwards compatibility */
    if (sameWord(command, "maxNode"))
	warn("maxNode deprecated, use maxJob");
    paraMaxJob(argv[2]);
    }
else if (sameWord(command, "resetCounts"))
    {
    if (argc != 2)
        usage();
    paraResetCounts();
    }
else if (sameWord(command, "freeBatch"))
    {
    if (argc != 2)
        usage();
    freeBatch();
    }
else if (sameWord(command, "clearSickNodes"))
    {
    clearSickNodes();
    }
else if (sameWord(command, "showSickNodes"))
    {
    showSickNodes(FALSE);
    }
else
    {
    errAbort("Unrecognized command '%s'.  Run para with no arguments for usage summary", 
    	command);
    }

verbose(2, "Total para time: %.2f seconds\n", (clock1000() - startTime) / 1000.0);

return 0;
}

