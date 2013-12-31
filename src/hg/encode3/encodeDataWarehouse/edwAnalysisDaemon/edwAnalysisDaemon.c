/* edwAnalysisDaemon - Run jobs remotely via parasol based on jobs in table.. */
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
  "edwAnalysisDaemon - Run jobs remotely via parasol based on jobs in table.\n"
  "usage:\n"
  "   edwAnalysisDaemon database table count fifo\n"
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

int launchJob(struct edwJob *job, char *errFileName)
/* Pass job's command line to parasol. */
{
char *paraHost = "encode-02";
char command[2048];
char *escaped = makeEscapedString(job->commandLine, '\'');
safef(command, sizeof(command),
    "ssh %s 'parasol -results=/data/encode3/encodeAnalysisPipeline/para.results -err=%s -wait "
    "add job %s'"
    , paraHost, errFileName, escaped);
warn("attempting %s", command);
int status = system(command);
freez(&escaped);
return status;
}

void runJob(struct runner *runner, struct edwJob *job)
/* Fork off and run job. */
{
/* Create stderr file for child  as a temp file */
char *paraTempDir = "/hive/groups/encode/encode3/encodeDataWarehouse/tmp/";
char tempFileName[PATH_LEN];
safef(tempFileName, PATH_LEN, "%sedwAnalysisDaemonXXXXXX", paraTempDir);
int errFd = mkstemp(tempFileName);
if (errFd < 0)
    errnoAbort("Couldn't open temp file %s", tempFileName);

++curThreads;
job->startTime = edwNow();
runner->job = job;

int childId;
if ((childId = mustFork()) == 0)
    {
    int status = launchJob(job, tempFileName);
    if (status != 0)
        exit(-1);
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

void readAndIgnore(int fd, int byteCount)
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
    readAndIgnore(fd, bytesReady);
    }
}

void edwAnalysisDaemon(char *database, char *table, char *countString, char *namedPipe)
/* edwAnalysisDaemon - Run jobs remotely via parasol based on jobs in table.. */
{
clDatabase = database;
clTable = table;

/* Set up array with a slot for each simultaneous job. */
maxThreadCount = sqlUnsigned(countString);
if (maxThreadCount > 500)
    errAbort("%s jobs at once? Really, that seems excessive", countString);
AllocArray(runners, maxThreadCount);


/* Open our file, which really should be a named pipe. */
int fd = mustOpenFd(namedPipe, O_RDONLY);      // Program waits here until something written to pipe
int dummyFd = mustOpenFd(namedPipe, O_WRONLY); // Keeps pipe from generating EOF when empty

long long lastId = 0;  // Keep track of lastId in run table that has already been handled.

for (;;)
    {
    /* Finish up processing on any children that have finished */
    while (checkOnChildRunner(FALSE) != NULL)	
        ;

    /* Get next bits of work from database. */
    struct sqlConnection *conn = sqlConnect(database);
    char query[256];
    sqlSafef(query, sizeof(query), 
	"select * from %s where startTime = 0 and id > %lld order by id limit 1", table, lastId);
    struct edwJob *job = edwJobLoadByQuery(conn, query);
    sqlDisconnect(&conn);
    if (job != NULL)
        {
	lastId = job->id;
	struct runner *runner = findFreeRunner();
	runJob(runner, job);
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
if (argc != 5)
    usage();
logDaemonize(argv[0]);
edwAnalysisDaemon(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
