/* edwRunDaemon - Run jobs on multiple processers in background.  This is done with
 * a combination of infrequent polling of the database, and a unix fifo which can be
 * sent a signal (anything ending with a newline actually) that tells it to go look
 * at database now. */

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

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwRunDaemon - Run jobs on multiple processers in background.  This is done with\n"
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
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"log", OPTION_STRING},
   {"logFacility", OPTION_STRING},
   {"debug", OPTION_BOOLEAN},
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

/* Save start time to database. */
struct sqlConnection *conn = sqlConnect(clDatabase);
char query[256];
sqlSafef(query, sizeof(query), "update %s set startTime=%lld where id=%lld", 
    clTable, job->startTime, (long long)job->id);
sqlUpdate(conn, query);
sqlDisconnect(&conn);

runner->job = job;
int childId;
if ((childId = mustFork()) == 0)
    {
    /* We be child side - execute command using system call */
    if (dup2(errFd, STDERR_FILENO) < 0)
        errnoAbort("Can't dup2 stderr to %s", tempFileName);
    int status = system(job->commandLine);
    if (status != 0)
        exit(-1);
    else
	exit(0);
    }
else
    {
    /* We be parent - just fill in job info */
    close(errFd);
    runner->pid = childId;
    runner->errFileName = cloneString(tempFileName);
    }
}

void readToNewLineAndIgnore(int fd)
/* Read byteCount from fd, and just throw it away.  We are just using named pipe
 * as a synchronization device is why. */
{
char c;
for (;;)
    {
    int oneRead = read(fd, &c, 1);
    if (oneRead == -1)
        errnoAbort("Problem reading from named pipe.");
    if (c == '\n')
        break;
    }
}

void syncWithTimeout(int fd, long long microsecs)
{
int bytesReady = netWaitForData(fd, microsecs);
if (bytesReady > 0)
    readToNewLineAndIgnore(fd);
}


void edwRunDaemon(char *database, char *table, char *countString, char *namedPipe)
/* edwRunDaemon - Run jobs on multiple processers in background. . */
{
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

struct edwJob *jobList = NULL;
long long lastId = 0;  // Keep track of lastId in run table that has already been handled.

for (;;)
    {
    /* Wait for signal to come on named pipe or one minute to pass */
    syncWithTimeout(fd, 60*1000000);

    /* Finish up processing on any children that have finished */
    while (checkOnChildRunner(FALSE) != NULL)	
        ;

    /* Get next bits of work from database. */
    struct sqlConnection *conn = sqlConnect(database);
    char query[256];
    sqlSafef(query, sizeof(query), 
	"select * from %s where startTime = 0 and id > %lld order by id", table, lastId);
    struct edwJob *newJobs = edwJobLoadByQuery(conn, query);
    int newJobCount = slCount(newJobs);
    if (newJobCount > 0)
	{
	struct edwJob *lastJob = slLastEl(newJobs);
	lastId = lastJob->id;
	}
    verbose(2, "Got %d new jobs\n", newJobCount);
    sqlDisconnect(&conn);
    jobList = slCat(jobList, newJobs);

    while (jobList != NULL)
        {
	struct edwJob *job = jobList;
	jobList = jobList->next;
	struct runner *runner = findFreeRunner();
	syncWithTimeout(fd, 0);
	runJob(runner, job);
	}
    }
close(dummyFd);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
logDaemonize(argv[0]);
edwRunDaemon(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
