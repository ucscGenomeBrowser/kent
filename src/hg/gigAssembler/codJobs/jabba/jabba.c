/* jabba - A program to launch,  monitor, and restart jobs via Codine. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "jobDb.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "jabba - A program to launch,  monitor, and restart jobs via Codine\n"
  "Normal usage is to do a 'jabba make' followed by 'jabba push' until\n"
  "job is done.  Use 'jabba check' to check status\n"
  "usage:\n"
  "   jabba command batch.hut [command-specific arguments]\n"
  "The commands are:\n"
  "jabba make batch.hut jobList\n"
  "   This makes the job-tracking database from a text file with the\n"
  "   command line for each job on a separate line\n"
  "jabba push batch.hut\n"
  "   This pushes forward the batch of jobs by submitting jobs to codine\n"
  "   It will try and keep the codine queue a size that is efficient for\n"
  "   codine, and retry failed jobs\n"
  "   options:\n"
  "      -retries=N   Number of retries per job - default 3.\n"
  "      -maxQueue=N  Number of jobs to allow on codine queue - default 10000\n"
  "      -minPush=N  Minimum number of jobs to queue - default 1.  Overrides maxQueue\n"
  "      -maxPush=N  Maximum numer of jobs to queue - default 10000\n"
  "      -warnTime=N Number of minutes job can run before hang warning - default 4320 (3 days)\n"
  "      -killTime=N Number of minutes job can run before push kills it - default 20160 (2 weeks)\n"
  "jabba shove batch.hut\n"
  "   Push jobs until can't push any more.  Options as with push and also:\n"
  "      -sleepTime=N  Number of seconds to sleep between pushes\n"
  "jabba try batch.hut\n"
  "      This is like jabba push, but only submits up to 10 jobs\n"
  "jabba check batch.hut\n"
  "   This checks on the progress of the jobs.\n"
  "jabba stop batch.hut\n"
  "   This stops all the jobs in the batch\n"
  "jabba finished batch.hut\n"
  "   List jobs that have finished\n"
  "jabba hung batch.hut\n"
  "   List hung jobs in the batch\n"
  "jabba crashed batch.hut\n"
  "   List jobs that crashed or failed output checks\n"
  "jabba failed batch.hut\n"
  "   List jobs that crashed or hung\n"
  "jabba problems batch.hut\n"
  "   List jobs that had problems (even if successfully rerun).  Includes host info\n"
  "jabba running batch.hut\n"
  "   Print info on currently running jobs\n"
  "jabba time batch.hut\n"
  "   List timing information\n"
  );
}

/* Variables that can be set from command line. */

int retries = 3;
int maxQueue = 10000;
int minPush = 1;
int maxPush = 20000;
int warnTime = 3*24*60;
int killTime = 14*24*60;
int sleepTime = 20*60;

/* Some variable we might want to move to a config file someday. */
char *tempName = "jabba.tmp";	/* Name for temp files. */
char *submitCommand = "/cluster/gridware/bin/glinux/qsub -cwd -o out -e err";
char *statusCommand = "/cluster/gridware/bin/glinux/qstat";
char *killCommand = "/cluster/gridware/bin/glinux/qdel";
char *runJobCommand = "/cluster/bin/scripts/runJob";

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

char *nowAsString()
/* Return current time and date in more or less above format. */
{
time_t timer;
char *s;
time(&timer);
s = ctime(&timer);
return trimSpaces(s);
}

char *cloneEvenNull(char *s)
/* Clone string.  Replace NULL with clone of "". */
{
if (s == NULL)
   return cloneString("");
else
    return cloneString(s);
}

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
    bool completeLastLine;	/* TRUE if last line ends with <lf> */
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

for (job = db->jobList; job != NULL; job = job->next)
    errCount += checkOneJob(job, when, hash);
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
carefulClose(&f);
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
return db;
}


void jabbaMake(char *batch, char *jobList)
/* Make a batch database from a job list. */
{
struct lineFile *lf = lineFileOpen(jobList, TRUE);
char *line;
int jobCount = 0;
struct jobDb *db;
struct job *job;
char backup[512];

AllocVar(db);
while (lineFileNext(lf, &line, NULL))
    {
    line = trimSpaces(line);
    if (line[0] == '#')
        continue;
    ++db->jobCount;
    job = jobFromLine(lf, line);
    slAddHead(&db->jobList, job);
    }
lineFileClose(&lf);
slReverse(&db->jobList);

doChecks(db, "in");
writeBatch(db, batch);
sprintf(backup, "%s.bak", batch);
writeBatch(db, backup);
printf("%d jobs written to %s\n", db->jobCount, batch);
}

void fillInSub(char *fileName, struct submission *sub)
/* Fill in submission from output file produced by qsub. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *words[8];
int wordCount;
char buf[256];

if (!lineFileNext(lf, &line, NULL))
    errAbort("Empty qsub output, sorry can't cope.");
wordCount = chopLine(line, words);
if (wordCount < 3 || !sameString("your", words[0]) || !isdigit(words[2][0]))
    errAbort("qsub output seems to have changed, you'll have to update fillInSub");
sub->id = cloneString(words[2]);
sprintf(buf, "out/runJob.o%s", sub->id);
sub->outFile = cloneString(buf);
sprintf(buf, "err/runJob.e%s", sub->id);
sub->errFile = cloneString(buf);
lineFileClose(&lf);
}

void submitJob(struct job *job)
/* Attempt to submit job. */
{
struct dyString *cmd = dyStringNew(1024);
struct submission *sub;
int err;

dyStringAppend(cmd, submitCommand);
dyStringAppend(cmd, " ");
dyStringAppend(cmd, runJobCommand);
dyStringAppend(cmd, " ");
dyStringAppend(cmd, job->command);
dyStringPrintf(cmd, " > %s", tempName);

err = system(cmd->string);
AllocVar(sub);
slAddHead(&job->submissionList, sub);
job->submissionCount += 1;
sub->submitTime = cloneString(nowAsString());
sub->startTime = cloneString("");
sub->endTime = cloneString("");
if (err != 0)
    {
    sub->submitError = TRUE;
    sub->id = cloneString("n/a");
    sub->errFile = cloneString("n/a");
    sub->outFile = cloneString("n/a");
    }
else
    {
    fillInSub(tempName, sub);
    }
dyStringFree(&cmd);
}

void statusOutputChanged()
/* Complain about status output format change and die. */
{
errAbort("\n%s output format changed, please update markQueuedJobs in jabba.c", 
	statusCommand);
}

int markQueuedJobs(struct jobDb *db)
/* Mark jobs that are queued up. Return total number of jobs in queue. */
{
struct dyString *cmd = dyStringNew(1024);
int err;
struct lineFile *lf;
struct hash *hash = newHash(0);
struct job *job;
struct submission *sub;
char *line, *words[10];
int wordCount;
int queueSize = 0;

/* Execute qstat system call. */
printf("jobs (everybody's) in Codine queue: ");
fflush(stdout);
dyStringAppend(cmd, statusCommand);
dyStringPrintf(cmd, " > %s", tempName);
err = system(cmd->string);
if (err != 0)
    errAbort("\nCouldn't execute '%s'", cmd->string);

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

/* Read status output. */
lf = lineFileOpen(tempName, TRUE);
if (lineFileNext(lf, &line, NULL))	/* Empty is ok. */
    {
    if (!startsWith("job-ID", line))
	statusOutputChanged();
    if (!lineFileNext(lf, &line, NULL) || !startsWith("-----", line))
	statusOutputChanged();
    while (lineFileNext(lf, &line, NULL))
        {
	wordCount = chopLine(line, words);
	if (wordCount < 7)
	    statusOutputChanged();
	++queueSize;
	if ((sub = hashFindVal(hash, words[0])) != NULL)
	    {
	    char *state = words[4];
	    if (state[0] == 'E')
		sub->queueError = TRUE;
	    else if (state[0] == 'd')
	        ;	/* Externally deleted? */
	    else 
	        {
		if (sameString(state, "r"))
		    {
		    sub->running = TRUE;
		    }
		else
		    {
		    sub->inQueue = TRUE;
		    }
		}
	    }
	}
    }
lineFileClose(&lf);
freeHash(&hash);
dyStringFree(&cmd);
printf("%d\n", queueSize);
return queueSize;
}

long dateToSeconds(char *date)
/* Convert from format like:
 *   'Wed Nov 7 13:35:11 PST 2001' to seconds since Jan. 1 2001.
 * This should be in a library somewhere, but I can't find it. 
 * This function is not totally perfect.  It'll add a leap year in 2200
 * when it shouldn't for instance. */
{
char *dupe = cloneString(skipLeadingSpaces(date));
char *words[8], *parts[4];
static char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static int daysInMonths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
int wordCount;
int leapDiv = 4;
int x;
int leapCount;
long secondsInDay = 24*60*60;
int year, month, day, hour, minute, second;
char *yearString;
long dayCount;
long result;

/* Parse string into various integer variables. */
wordCount = chopLine(dupe, words);
if (wordCount < 5)
    errAbort("Badly formatted(1) date '%s'", date);
if (wordCount == 5)
    yearString = words[4];
else
    yearString = words[5];
if (!isdigit(yearString[0]))
    errAbort("Badly formatted(2) date '%s'", date);
year = atoi(yearString);
if ((month = stringIx(words[1], months)) < 0)
    errAbort("Unrecognized month '%s'", date);
day = atoi(words[2]);
wordCount = chopString(words[3], ":", parts, ArraySize(parts));
if (wordCount != 3)
    errAbort("Badly formated time in '%s'", date);
hour = atoi(parts[0]);
minute = atoi(parts[1]);
second = atoi(parts[2]);
freez(&dupe);

/* Figure out elapsed days with leap-years. */
x = year - 1 - 2000;	/* 1972 is nearest leap year. */
leapCount = x/4 + 1;
dayCount = (year - 2001) * 365 + leapCount;
for (x=0; x<month; ++x)
    dayCount += daysInMonths[x];
dayCount += day-1;
if (year%4 == 0 && month >= 2)
    ++dayCount;
result = secondsInDay*dayCount + hour*3600 + minute*60 + second;
return result;
}

long nowInSeconds()
/* Return current date in above format. */
{
return dateToSeconds(nowAsString());
}

struct runJobOutput
/* Info about a run job. */
    {
    char *startTime;
    char *endTime;
    float cpuTime;
    int retVal;
    boolean gotRet;
    boolean trackingError;
    char host[128];
    };

struct runJobOutput *parseRunJobOutput(char *fileName)
/* Parse a run job output file.  Might have trouble if the program output
 * is horribly complex. */
{
static struct runJobOutput ret;
struct lineFile *lf;
char *line, *words[20], *s;
int wordCount;
char *startPattern = "Start time: ";
char *endPattern = "Finish time: ";
char *returnPattern = "Return value = ";
char *hostPattern = "Executing host: ";
boolean gotStart = FALSE, gotEnd = FALSE;
boolean gotCpu = FALSE, gotReturn = FALSE;

/* Set up default return values.  Free old strings. */
freez(&ret.startTime);
freez(&ret.endTime);
ZeroVar(&ret);

lf = lineFileMayOpen(fileName, TRUE);
if (lf == NULL)
    {
    ret.trackingError = 1;
    return &ret;
    }
while (lineFileNext(lf, &line, NULL))
    {
    if (startsWith(startPattern, line))
        {
	line += strlen(startPattern);
	ret.startTime = cloneString(trimSpaces(line));
	gotStart = TRUE;
	}
    else if (startsWith(endPattern, line))
	{
	line += strlen(endPattern);
	ret.endTime = cloneString(trimSpaces(line));
	gotEnd = TRUE;
	break;
	}
    else if (startsWith(hostPattern, line))
        {
	line += strlen(hostPattern);
	trimSpaces(line);
	strcpy(ret.host, line);
	}
    else if (isdigit(line[0]) )
	{
	wordCount = chopLine(line, words);
	if (wordCount >= 3 && lastChar(words[0]) == 'u'
	    && lastChar(words[1]) == 's' && isdigit(words[1][0]))
	    {
	    ret.cpuTime = atof(words[0]) + atof(words[1]);
	    gotCpu = TRUE;
	    }
	}
    else if (startsWith(returnPattern, line))
	{
	line += strlen(returnPattern);
	line = skipLeadingSpaces(line);
	ret.retVal = atoi(line);
	ret.gotRet = TRUE;
	gotReturn = TRUE;
	}
    }
if (!gotStart)
    {
    ret.trackingError = 2;
    }
if (gotEnd)
    {
    if (!gotCpu || !gotReturn)
       errAbort("%s is not in a runJob format jabba can parse", fileName);
    }
lineFileClose(&lf);
return  &ret;
}

void killSubmission(struct submission *sub)
/* Kill a submission. */
{
struct dyString *cmd = newDyString(256);
int err;
dyStringPrintf(cmd, "%s %s", killCommand, sub->id);
err = system(cmd->string);
if (err != 0)
   warn("Couldn't kill job id %s", sub->id);
freeDyString(&cmd);
}

void markRunJobStatus(struct jobDb *db)
/* Mark jobs based on runJob output file. */
{
struct job *job;
struct submission *sub;
char *line, *words[10];
int wordCount;
long killSeconds = killTime*60;
long warnSeconds = warnTime*60;
long duration;
struct hash *checkHash = newHash(0);
char host[128];

for (job=db->jobList; job != NULL; job = job->next)
    {
    if ((sub = job->submissionList) != NULL)
        {
	/* Look for hitherto unclassified jobs that are either running or
	 * possibly finished. */
	if (!sub->queueError && !sub->inQueue && !sub->crashed && !sub->hung && !sub->ranOk)
	    {
	    struct runJobOutput *rjo = parseRunJobOutput(sub->outFile);
	    sub->startTime = cloneEvenNull(rjo->startTime);
	    sub->endTime = cloneEvenNull(rjo->endTime);
	    if (rjo->trackingError)
	        {
		long subTime, curTime;
		subTime = dateToSeconds(sub->submitTime);
		curTime = nowInSeconds();
		duration = curTime - subTime;
		if (duration > 60*20)	/* Give it up to 20 minutes to show up. */
		    sub->trackingError = 3;
		else
		    sub->inQueue = TRUE;
		}
	    else
	        {
		sub->cpuTime = rjo->cpuTime;
		sub->retVal = rjo->retVal;
		sub->gotRetVal = rjo->gotRet;
		if (rjo->gotRet)
		    {
		    if (sub->retVal == 0 && checkOneJob(job, "out", checkHash) == 0)
			sub->ranOk = TRUE;
		    else
			sub->crashed = TRUE;
		    }
		else
		    {
		    if (sub->running)
			{
			duration = nowInSeconds() - dateToSeconds(sub->startTime);
			if (duration >= killSeconds)
			    {
			    sub->hung = TRUE;
			    killSubmission(sub);
			    }
			else if (duration >= warnSeconds)
			    sub->slow = TRUE;
			}
		    else
		        {
			warn("Codine seems to have lost track of %s.  It's not running but hasn't returned", sub->id);
			sub->trackingError = 4;
			}
		    }
		}
	    }
	}
    }
freeHash(&checkHash);
}

boolean needsRerun(struct submission *sub)
/* Return TRUE if submission needs to be rerun. */
{
if (sub == NULL)
    return TRUE;
return sub->submitError || sub->queueError || sub->crashed || sub->trackingError;
}

int jabbaPush(char *batch)
/* Push a batch of jobs forward - submit jobs.  Return number of jobs
 * pushed. */
{
struct jobDb *db = readBatch(batch);
struct job *job;
int queueSize;
int pushCount = 0, retryCount = 0;
int tryCount;
boolean finished = FALSE;

makeDir("err");
makeDir("out");
queueSize = markQueuedJobs(db);
markRunJobStatus(db);

for (tryCount=1; tryCount<=retries && !finished; ++tryCount)
    {
    for (job = db->jobList; job != NULL; job = job->next)
        {
	if (job->submissionCount < tryCount && 
	   (job->submissionList == NULL || needsRerun(job->submissionList)))
	    {
	    submitJob(job);    
	    printf(".");
	    fflush(stdout);
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
writeBatch(db, batch);
jobDbFree(&db);
if (pushCount > 0)
   printf("\n");
printf("Pushed Jobs: %d\n", pushCount);
if (retryCount > 0)
    printf("Retried jobs: %d\n", retryCount);
return pushCount;
}

void jabbaShove(char *batch)
/* Keep pushing jobs until finished. */
{
while (jabbaPush(batch) > 0)
    {
    printf("Sleeping until next push - hit <control>C to quit shoving\n");
    sleep(sleepTime);
    }
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

void jabbaCheck(char *batch)
/* Check on progress of a batch. */
{
struct jobDb *db = readBatch(batch);
int queueSize;

queueSize = markQueuedJobs(db);
markRunJobStatus(db);
reportOnJobs(db);

writeBatch(db, batch);
}

void jabbaListFailed(char *batch)
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

void jabbaListState(char *batch, enum jaState targetState)
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

void printErrFile(struct submission *sub)
/* Print error file if it exists. */
{
if (fileExists(sub->errFile))
    {
    char *buf;
    size_t size;
    printf("stderr:\n");
    readInGulp(sub->errFile, &buf, &size);
    mustWrite(stdout, buf, size);
    freez(&buf);
    }
else
    {
    printf("stderr file doesn't exist\n");
    }
}

void problemReport(struct job *job, struct submission *sub, char *type)
/* Print report on one problem. */
{
struct check *check;
struct hash *hash = newHash(0);
struct runJobOutput *rjo = parseRunJobOutput(sub->outFile);

printf("job: %s\n", job->command);
printf("id: %s\n", sub->id);
printf("host: %s\n", rjo->host);
printf("failure type: %s\n", type);
if (sub->trackingError)
     printf("tracking error: %d\n", sub->trackingError);
if (rjo->startTime)
    printf("start time: %s\n", rjo->startTime);
if (rjo->gotRet)
    {
    printf("return: %d\n", rjo->retVal);
    for (check = job->checkList; check != NULL; check = check->next)
	{
	doOneCheck(check, hash, stdout);
	}
    }
printErrFile(sub);
printf("\n");
hashFree(&hash);
}

void jabbaProblems(char *batch)
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
/* Print report on one problem. */
{
struct check *check;
struct hash *hash = newHash(0);
struct runJobOutput *rjo = parseRunJobOutput(sub->outFile);
int duration = nowInSeconds() - dateToSeconds(rjo->startTime);

printf("command: %s\n", job->command);
printf("jobId: %s\n", sub->id);
printf("host: %s\n", rjo->host);
printf("start time: %s\n", rjo->startTime);
printf("run time so far: %d sec,  %4.2f min, %4.2f hours,  %4.2f days\n", 
	duration, duration/60.0, duration/3600.0,  duration/(3600.0*24.0));
printErrFile(sub);
printf("\n");
hashFree(&hash);
}

void jabbaRunning(char *batch)
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



void jabbaStop(char *batch)
/* Stop batch of jobs. */
{
struct jobDb *db = readBatch(batch);
struct job *job;
struct submission *sub;
markQueuedJobs(db);
for (job = db->jobList; job != NULL; job = job->next)
    {
    sub = job->submissionList;
    if (sub != NULL)
        {
	if (sub->inQueue || sub->running)
	   killSubmission(sub);
	sub->crashed = TRUE;
	}
    }
writeBatch(db, batch);
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
long subTime, firstSub = BIGNUM, endTime, lastEnd = 0;
boolean first = TRUE;
struct job *job;
struct submission *sub;
char *firstString = NULL, *endString = NULL;
long now = nowInSeconds();

for (job = db->jobList; job != NULL; job = job->next)
    {
    if ((sub = job->submissionList) != NULL)
        {
	subTime = dateToSeconds(sub->submitTime);
	if (subTime < now)	/* Protect against wacked out clocks. */
	    {
	    if (first)
		{
		firstString = sub->submitTime;
		firstSub = subTime;
		first = FALSE;
		}
	    else
		{
		if (subTime < firstSub) 
		    {
		    firstString = sub->submitTime;
		    firstSub = subTime;
		    }
		}
	    }
	if (sub->endTime != NULL && sub->endTime[0] != 0)
	    {
	    endTime = dateToSeconds(sub->endTime);
	    if (endTime < now)	/* Protect against wacked out clocks. */
		{
		if (endTime > lastEnd) 
		    {
		    endString = sub->endTime;
		    lastEnd = endTime;
		    }
		}
	    }
	}
    }
return lastEnd - firstSub;
}

void jabbaTimes(char *batch)
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

markQueuedJobs(db);
markRunJobStatus(db);
for (job = db->jobList; job != NULL; job = job->next)
    {
    ++jobCount;
    if ((sub = job->submissionList) != NULL)
        {
	if (sub->running)
	   {
	   struct runJobOutput *rjo = parseRunJobOutput(sub->outFile);
	   int oneTime = nowInSeconds() - dateToSeconds(rjo->startTime);
	   if (oneTime < 0)
	       {
	       warn("Strange start time in %s: %s", rjo->host, rjo->startTime);
	       }
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
	   struct runJobOutput *rjo = parseRunJobOutput(sub->outFile);
	   if (rjo->gotRet && rjo->endTime != NULL)
	       {
	       ++timedCount;
	       totalCpu += rjo->cpuTime;
	       oneWall = dateToSeconds(rjo->endTime) - dateToSeconds(rjo->startTime);
	       if (oneWall < 0)	/* Protect against clock reset. */
		   {
		   warn("End before start job %s host %s", sub->id, rjo->host);
		   warn("Start %s,  End %s", rjo->startTime, rjo->endTime);
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
}

void jabbaFix(char *batch)
/* Fix a batch somehow. */
{
struct jobDb *db = readBatch(batch);
struct job *job;
struct submission *sub;

for (job = db->jobList; job != NULL; job = job->next)
    {
    for (sub = job->submissionList; sub != NULL; sub = sub->next)
        {
	if (sub->trackingError)
	    sub->crashed = FALSE;
	}
    }


writeBatch(db, batch);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;
char *batch;

cgiSpoof(&argc, argv);
if (argc < 3)
    usage();
retries = cgiUsualInt("retries",  retries);
maxQueue = cgiUsualInt("maxQueue",  maxQueue);
minPush = cgiUsualInt("minPush",  minPush);
maxPush = cgiUsualInt("maxPush",  maxPush);
warnTime = cgiUsualInt("warnTime", warnTime);
killTime = cgiUsualInt("killTime", killTime);
command = argv[1];
batch = argv[2];
if (strchr(batch, '/') != NULL)
    errAbort("Jabba has to be run in the same directory as %s", batch);
if (sameString(command, "make"))
    {
    if (argc != 4)
        usage();
    jabbaMake(batch, argv[3]);
    }
else if (sameString(command, "check"))
    {
    jabbaCheck(batch);
    }
else if (sameString(command, "push"))
    {
    jabbaPush(batch);
    }
else if (sameString(command, "shove"))
    {
    jabbaShove(batch);
    }
else if (sameString(command, "try"))
    {
    maxPush = 20;
    jabbaPush(batch);
    }
else if (sameString(command, "stop"))
    {
    jabbaStop(batch);
    }
else if (sameString(command, "hung"))
    {
    jabbaListState(batch, jaHung);
    }
else if (sameString(command, "crashed"))
    {
    jabbaListState(batch, jaCrashed);
    }
else if (sameString(command, "failed"))
    {
    jabbaListFailed(batch);
    }
else if (sameString(command, "finished"))
    {
    jabbaListState(batch, jaFinished);
    }
else if (sameString(command, "problems") || sameString(command, "problem"))
    {
    jabbaProblems(batch);
    }
else if (sameString(command, "running"))
    {
    jabbaRunning(batch);
    }
else if (sameString(command, "time") || sameString(command, "times"))
    {
    jabbaTimes(batch);
    }
else if (sameString(command, "fix"))
    {
    jabbaFix(batch);
    }
else
    {
    errAbort("Unrecognized command '%s'.  Run jabba with no arguments for usage summary", 
    	command);
    }
return 0;
}
