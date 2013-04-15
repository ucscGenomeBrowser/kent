/* edwRunner - Runs pending jobs in edwJob table.. */
#include <sys/wait.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "portable.h"
#include "obscure.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwRunner - Runs pending jobs in edwJob table.\n"
  "usage:\n"
  "   edwRunner now\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

#define maxThreadCount 5

struct runner
/* Keeps track of running process. */
    {
    int pid;	/* Process ID or 0 if none */
    char *errFileName;   /* Standard error file for this process */
    struct edwJob *job;	 /* The job we are running */
    };

struct runner runners[maxThreadCount];  /* Job table */
int curThreads = 0;

void finishRun(struct runner *run, int status)
/* Finish up job. Copy results into database */
{
/* Get job and fill in two easy fields. */
struct edwJob *job = run->job;
job->endTime = edwNow();
job->returnCode = status;

/* Read in stderr */
size_t errorMessageSize;
readInGulp(run->errFileName, &job->stderr, &errorMessageSize);

/* Update database with job results */
struct dyString *dy = dyStringNew(0);
dyStringPrintf(dy, "update edwJob set endTime=%lld, stderr='", job->endTime);
char *escaped = sqlEscapeString(trimSpaces(job->stderr));
dyStringAppend(dy, escaped);
dyStringPrintf(dy, "', returnCode=%d where id=%u", job->returnCode, job->id);
struct sqlConnection *conn = sqlConnect(edwDatabase);
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
freez(&escaped);
dyStringFree(&dy);

/* Free up runner resources. */
freez(&run->errFileName);
edwJobFree(&job);
run->pid = 0;
--curThreads;
}

struct runner *waitOnChildRunner()
/* Wait for child to finish. */
{
assert(curThreads > 0);
int status = 0;
int child = wait(&status);
if (child < 0)
    errnoAbort("Couldn't wait");
int i;
for (i=0; i<maxThreadCount; ++i)
    {
    struct runner *run = &runners[i];
    if (run->pid == child)
	{
	finishRun(run, status);
	return run;
	}
    }
internalErr();
return NULL;
}

struct runner *findFreeRunner()
/* Return free runner if there is one,  otherwise wait until there is. */
{
int i;
if (curThreads >= maxThreadCount)
    {
    return waitOnChildRunner();
    }
else
    {
    for (i=0; i<maxThreadCount; ++i)
        {
	struct runner *run = &runners[i];
	if (run->pid == 0)
	    return run;
	}
    }
internalErr();  // Should not get here
return NULL;
}

void runJob(struct runner *runner, struct edwJob *job)
/* Fork off and run job. */
{
/* Create stderr file for child  as a temp file */
char tempFileName[PATH_LEN];
safef(tempFileName, PATH_LEN, "%sedwRunnerXXXXXX", edwTempDir());
int errFd = mkstemp(tempFileName);
if (errFd < 0)
    errnoAbort("Couldn't open temp file %s", tempFileName);

++curThreads;
job->startTime = edwNow();

/* Save start time to database. */
struct sqlConnection *conn = sqlConnect(edwDatabase);
char query[256];
safef(query, sizeof(query), "update edwJob set startTime=%lld where id=%lld", 
    job->startTime, (long long)job->id);
sqlUpdate(conn, query);
sqlDisconnect(&conn);

runner->job = job;
int childId;
if ((childId = mustFork()) == 0)
    {
    /* We be child side */
    if (dup2(errFd, STDERR_FILENO) < 0)
        errnoAbort("Can't dup2 stderr to %s", tempFileName);
    int status = system(job->commandLine);
    exit(status);
    }
else
    {
    /* We be parent - just fill in job info */
    close(errFd);
    runner->pid = childId;
    runner->errFileName = cloneString(tempFileName);
    }
}

void edwRunner(char *now)
/* edwRunner - Runs pending jobs in edwJob table.. */
{
struct edwJob *jobList = NULL;
int pendingJobCount = 0;
long long timeBetweenChecks = 30;
long long lastCheck = edwNow() - timeBetweenChecks;
for (;;)
    {
    uglyf("pendingJobCount %d, slCount(jobList)=%d, curThreads %d\n", pendingJobCount, slCount(jobList), curThreads);
    if (pendingJobCount + curThreads < maxThreadCount)
        {
	if (edwNow() - lastCheck >= timeBetweenChecks)
	    {
	    struct sqlConnection *conn = sqlConnect(edwDatabase);
	    char query[256];
	    safef(query, sizeof(query), "select * from edwJob where startTime = 0 order by id");
	    struct edwJob *newJobs = edwJobLoadByQuery(conn, query);
	    int newJobCount = slCount(newJobs);
	    pendingJobCount += newJobCount;
	    uglyf("Got %d new jobs\n", newJobCount);
	    sqlDisconnect(&conn);
	    jobList = slCat(jobList, newJobs);
	    lastCheck = edwNow();
	    }
	}

    if (jobList != NULL)
	{
	struct runner *runner = findFreeRunner();
	struct edwJob *job = jobList;
	jobList = jobList->next;
	runJob(runner, job);
	--pendingJobCount;
	}
    else if (curThreads > 0)
	{
	waitOnChildRunner();
	}
    else
        {
	sleep(timeBetweenChecks);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwRunner(argv[1]);
return 0;
}
