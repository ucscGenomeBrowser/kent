/* edwRunDaemon - Run jobs on multiple processers in background.  This is done with
 * a combination of infrequent polling of the database, and a unix fifo which can be
 * sent a signal (anything ending with a newline actually) that tells it to go look
 * at database now. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* Version history -
 *  v2 - making it so daemon only keeps current job in memory consulting db table always
 *       for next job.  This simplifies code and allows the daemon to respond to changes 
 *       the job table at the expense of spinning the DB daemon a little more often.  Also
 *       maintaining a new column, pid, that contains unix process id for the job. 
 *  v3 - put delay to default to 1, since seems to be needed when restarting if there are
 *       jobs in the queue. */

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
#include "encodeDataWarehouse.h"
#include "edwLib.h"

char *clDatabase, *clTable;
int clDelay = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwRunDaemon v3 - Run jobs on multiple processers in background.  This is done with\n"
  "a combination of infrequent polling of the database, and a unix fifo which can be\n"
  "sent a signal (anything ending with a newline actually) that tells it to go look\n"
  "at database now.\n"
  "usage:\n"
  "   edwRunDaemon database table count fifo\n"
  "where:\n"
  "   database - mySQL database where edwRun table lives\n"
  "   table - table with same six fields as edwRun table\n"
  "   count - number of simultanious jobs to run\n"
  "   fifo - named pipe used to notify daemon of new data\n"
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
    struct edwJob *job;	 /* The job we are running */
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
struct edwJob *job = run->job;
job->endTime = edwNow();
job->returnCode = status;

/* Read in stderr */
size_t errorMessageSize;
readInGulp(run->errFileName, &job->stderr, &errorMessageSize);
remove(run->errFileName);

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
edwJobFree(&job);
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

void runJob(struct runner *runner, struct edwJob *job)
/* Fork off and run job. */
{
/* Create stderr file for child  as a temp file */
char tempFileName[PATH_LEN];
safef(tempFileName, PATH_LEN, "%sedwRunDaemonXXXXXX", edwTempDir());
int errFd = mkstemp(tempFileName);
if (errFd < 0)
    errnoAbort("Couldn't open temp file %s", tempFileName);

++curThreads;
job->startTime = edwNow();
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


void edwReadAndIgnore(int fd, int byteCount)
/* Read byteCount from fd, and just throw it away.  We are just using named pipe
 * as a synchronization device is why. */
{
/* This is implemented perhaps too fancily - so as to efficiently discard datae
 * if need be by reading into a reasonable fixed sized buffer as opposed to a single
 * character. */
int bytesLeft = byteCount;
char buf[128];
int bufSize = sizeof(buf);
while (bytesLeft > 0)
    {
    int bytesAttempted = (bytesLeft > bufSize ? bufSize : bytesLeft);
    int bytesRead = read(fd, buf, bytesAttempted);
    if (bytesRead == -1)
        errnoAbort("Problem reading from named pipe.");
    bytesLeft -= bytesRead;
    }
}

void syncWithTimeout(int fd, long long microsecs)
/* Wait for either input from file fd, or timeout.  Will
 * ignore input - just using it as a sign to short-circuit
 * wait part of a polling loop. */
{
int bytesReady = netWaitForData(fd, microsecs);
if (bytesReady > 0)
    {
    edwReadAndIgnore(fd, bytesReady);
    }
}


void edwRunDaemon(char *database, char *table, char *countString, char *namedPipe)
/* edwRunDaemon - Run jobs on multiple processers in background. . */
{
warn("Starting edwRunDaemon v2 on %s.%s with %s jobs synced on %s", 
    database, table, countString,namedPipe);
clDatabase = database;
clTable = table;

/* Set up array with a slot for each simultaneous job. */
maxThreadCount = sqlUnsigned(countString);
if (maxThreadCount > 100)
    errAbort("%s jobs at once? Really, that seems excessive", countString);
AllocArray(runners, maxThreadCount);


/* Open our file, which really should be a named pipe. */
int fd = mustOpenFd(namedPipe, O_RDONLY);      // Program waits here until something written to pipe
int dummyFd = mustOpenFd(namedPipe, O_WRONLY); // Keeps pipe from generating EOF when empty

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
    struct edwJob *job = NULL;
    if (conn != NULL)
        {
	/* Connected no problem, let's get next job. */
	char query[256];
	sqlSafef(query, sizeof(query), 
	    "select * from %s where startTime = 0 and id > %lld order by id limit 1", 
	    table, lastId);
	job = edwJobLoadByQuery(conn, query);
	sqlDisconnect(&conn);
	}
    else
        {
	/* Did not connect to database.  Oh well. Don't just abort, this happens. */
	/* Track error of no connect.  Give it 10 tries every now and again before giving up. */
	long long now = edwNow();

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
	sleep(clDelay); // Avoid network storm
	}
    else
        {
	/* Wait for signal to come on named pipe or 10 seconds to pass */
	syncWithTimeout(fd, 10*1000000);
	}
    }
close(dummyFd);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clDelay = optionInt("delay", clDelay);
if (argc != 5)
    usage();
logDaemonize(argv[0]);
edwRunDaemon(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
