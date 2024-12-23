/* cdwRunDaemon - Run jobs on multiple processers in background.  
 * This is done with polling of the database */

/* Copyright (C) 2024 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

/* Version history -
 *  v2 - making it so daemon only keeps current job in memory consulting db table always
 *       for next job.  This simplifies code and allows the daemon to respond to changes 
 *       the job table at the expense of spinning the DB daemon a little more often.  Also
 *       maintaining a new column, pid, that contains unix process id for the job. 
 *  v3 - put delay to default to 1, since seems to be needed when restarting if there are
 *       jobs in the queue. 
 *  v4 - the fifo has been problematic enough, and the polling is low key enough I just 
 *       removed the fifo. */

#include <sys/wait.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "obscure.h"
#include "net.h"
#include "log.h"
#include "sqlNum.h"
#include "cdw.h"
#include "cdwLib.h"

char *clDatabase, *clTable;
int clDelay = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwRunDaemon v4 - Run jobs on multiple processers in background.\n"  
  "usage:\n"
  "   cdwRunDaemon database table count\n"
  "where:\n"
  "   database - mySQL database where cdwRun table lives\n"
  "   table - table with same six fields as cdwRun table\n"
  "   count - number of simultanious jobs to run\n"
  "options:\n"
  "   -debug - don't fork, close stdout, and daemonize self\n"
  "   -log=logFile - send error messages and warnings of daemon itself to logFile\n"
  "        There are not many of these.  Error mesages from jobs daemon runs end up\n"
  "        in errorMessage fields of database.\n"
  "   -logFacility - sends error messages and such to system log facility instead.\n"
  "   -delay=N - delay this many seconds before starting a job, default %d\n"
  , clDelay
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"log", OPTION_STRING},
   {"logFacility", OPTION_STRING},
   {"debug", OPTION_BOOLEAN},
   {"delay", OPTION_INT},
   {NULL, 0},
};

struct runner
/* Keeps track of running process. */
    {
    int pid;	/* Process ID or 0 if none */
    char *errFileName;   /* Standard error file for this process */
    struct cdwJob *job;	 /* The job we are running */
    };

int curThreads = 0, maxThreadCount;
struct runner *runners;

void finishRun(struct runner *run, int status)
/* Finish up job. Copy results into database */
{
/* Clean up wait status so will show 0 for nice successful jobs */
if (!WIFEXITED(status))
    status = -1;
else
    status = WEXITSTATUS(status);

/* Get job and fill in two easy fields. */
struct cdwJob *job = run->job;
job->endTime = cdwNow();
job->returnCode = status;

/* Read in stderr */
size_t errorMessageSize;
char *errMessage;
readInGulp(run->errFileName, &errMessage, &errorMessageSize);
remove(run->errFileName);

/* Clean up stderr a bit before putting it into SQL - first making sure it is not too big. */
int maxStderr = 100000;
if (errorMessageSize >= maxStderr)
    {
    errorMessageSize = maxStderr;
    errMessage[maxStderr] = 0;
    }
/* And then cleaning up nonprintable characters converting non-ascii to spaces */
int i;
for (i=0; i<errorMessageSize; ++i)
    {
    char c = errMessage[i];
    if (c < '\n')
        errMessage[i] = ' ' ;
    }
stripString(errMessage, "NOSQLINJ");
job->stderr = errMessage;


/* Update database with job results */
struct dyString *dy = dyStringNew(0);
sqlDyStringPrintf(dy, "update %s set endTime=%lld, stderr='%s', returnCode=%d where id=%u",
    clTable, job->endTime, trimSpaces(job->stderr), job->returnCode, job->id);
struct sqlConnection *conn = sqlConnect(clDatabase);
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
dyStringFree(&dy);

/* Free up runner resources. */
freez(&run->errFileName);
cdwJobFree(&job);
run->pid = 0;
--curThreads;
}

struct runner *checkOnChildRunner(boolean doWait)
/* See if a child has finished and optionally wait for it.  Return
 * a pointer to slot child has freed if it has finished. */
{
if (curThreads > 0)
    {
    int status = 0;
    int waitFlags = (doWait ? 0 : WNOHANG);
    int child = waitpid(-1, &status, waitFlags);
    if (child < 0)
	errnoAbort("Couldn't wait");
    if (child != 0)
	{
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
	}
    }
return NULL;
}

struct runner *waitOnChildRunner()
/* Wait for child to finish. */
{
return checkOnChildRunner(TRUE);
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

void runJob(struct runner *runner, struct cdwJob *job)
/* Fork off and run job. */
{
/* Create stderr file for child  as a temp file */
char tempFileName[PATH_LEN];
safef(tempFileName, PATH_LEN, "%scdwRunDaemonXXXXXX", cdwTempDir());
int errFd = mkstemp(tempFileName);
if (errFd < 0)
    errnoAbort("Couldn't open temp file %s", tempFileName);

++curThreads;
job->startTime = cdwNow();
runner->job = job;

int childId;
if ((childId = mustFork()) == 0)
    {
    /* We be child side - execute command using system call */
    if (dup2(errFd, STDERR_FILENO) < 0)
        errnoAbort("Can't dup2 stderr to %s", tempFileName);
    int status = system(job->commandLine);
    if (status != 0)
	{
	errAbort("Error: status %d from system of %s", status, job->commandLine);
	}
    else
	exit(0);
    }
else
    {
    /* Save start time and process ID to database. */
    struct sqlConnection *conn = sqlConnect(clDatabase);
    char query[256];
    sqlSafef(query, sizeof(query), "update %s set startTime=%lld, pid=%d where id=%lld", 
	clTable, job->startTime, childId, (long long)job->id);
    sqlUpdate(conn, query);
    sqlDisconnect(&conn);

    /* We be parent - just fill in job info */
    close(errFd);
    runner->pid = childId;
    runner->errFileName = cloneString(tempFileName);
    }
}



void cdwRunDaemon(char *database, char *table, char *countString)
/* cdwRunDaemon - Run jobs on multiple processers in background. . */
{
warn("Starting cdwRunDaemon v3 on %s.%s with %s jobs", 
    database, table, countString);
clDatabase = database;
clTable = table;

/* Set up array with a slot for each simultaneous job. */
maxThreadCount = sqlUnsigned(countString);
if (maxThreadCount > 100)
    errAbort("%s jobs at once? Really, that seems excessive", countString);
AllocArray(runners, maxThreadCount);


long long lastId = 0;  // Keep track of lastId in run table that has already been handled.
int connMiss = 0;
long long lastMiss = 0;

for (;;)
    {
    /* Finish up processing on any children that have finished */
    while (checkOnChildRunner(FALSE) != NULL)	
        ;

    /* Get next bits of work from database. */
    struct sqlConnection *conn = sqlMayConnect(database);
    struct cdwJob *job = NULL;
    if (conn != NULL)
        {
	/* Connected no problem, let's get next job. */
	char query[256];
	sqlSafef(query, sizeof(query), 
	    "select * from %s where startTime = 0 and id > %lld order by id limit 1", 
	    table, lastId);
	job = cdwJobLoadByQuery(conn, query);
	sqlDisconnect(&conn);
	}
    else
        {
	/* Did not connect to database.  Oh well. Don't just abort, this happens. */
	/* Track error of no connect.  Give it 10 tries every now and again before giving up. */
	long long now = cdwNow();

	/* Reset max-retry counter every 10 minutes */
	warn("Missed connection to %s at %lld", database, now);
	int betweenBursts = 60 * 10;   // Seconds to allow between bursts of noise
	if (now - lastMiss > betweenBursts)
	    connMiss = 0;

	/* Allow 12 retries (2 minutes downtime) */
	int maxTries = 12;
	if (++connMiss > maxTries)
	    errAbort("%d misses connecting to %s, aborting", connMiss, database);
	lastMiss = now;
	}

    if (job != NULL)
        {
	lastId = job->id;
	struct runner *runner = findFreeRunner();
	runJob(runner, job);
	}
    sleep(clDelay); // Avoid network storm
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clDelay = optionInt("delay", clDelay);
if (argc != 4)
    usage();
logDaemonize(argv[0]);
cdwRunDaemon(argv[1], argv[2], argv[3]);
return 0;
}
