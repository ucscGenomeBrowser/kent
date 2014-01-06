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
#include "../../../../parasol/inc/jobResult.h"
#include "../../../../parasol/inc/paraMessage.h"

pid_t childPid;
char *clDatabase = "encodeDataWarehouse", *clTable;
char *paraHost = "ku";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwAnalysisDaemon - Run jobs remotely via parasol based on jobs in table.\n"
  "usage:\n"
  "   edwAnalysisDaemon table count\n"
  "where:\n"
  "   table - table with same six fields as edwRun table\n"
  "   count - number of simultanious jobs to run\n"
  "options:\n"
  "   -database=%s - mySQL database where edwRun table lives\n"
  "   -debug - don't fork, close stdout, and daemonize self\n"
  "   -log=logFile - send error messages and warnings of daemon itself to logFile\n"
  "        There are not many of these.  Error mesages from jobs daemon runs end up\n"
  "        in errorMessage fields of database.\n"
  "   -logFacility - sends error messages and such to system log facility instead.\n"
  "   -paraHost - machine running parasol (paraHub in particular)\n"
  , clDatabase
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"database", OPTION_STRING},
   {"log", OPTION_STRING},
   {"logFacility", OPTION_STRING},
   {"debug", OPTION_BOOLEAN},
   {"paraHost", OPTION_STRING},
   {NULL, 0},
};

struct runner
/* Keeps track of running process. */
    {
    int pid;	/* Process ID or 0 if none */
    struct edwJob *job;	 /* The job we are running */
    };

int curThreads = 0, maxThreadCount;
struct runner *runners;

char *eapParaTempDir = "/hive/groups/encode/encode3/encodeAnalysisPipeline/paraTmp/";

void finishRun(struct runner *run, int status)
/* Finish up job. Copy results into database */
{
/* Get job and fill in two easy fields. */
struct edwJob *job = run->job;
job->endTime = edwNow();
job->returnCode = status;

/* Create stderr file for child  as a temp file */
char stderrTempFileName[PATH_LEN];
safef(stderrTempFileName, PATH_LEN, "%sedwAnalysisDaemonXXXXXX", eapParaTempDir);
int errFd = mkstemp(stderrTempFileName);
if (errFd < 0)
    errnoAbort("Couldn't open temp file %s", stderrTempFileName);
close(errFd);

/* Load in existing stderr contents which should be host:filename. 
 * Copy them from parasol node to stderrTempFileName. */
struct sqlConnection *conn = sqlConnect(clDatabase);
char query[512];
sqlSafef(query, sizeof(query), "select stderr from %s where id = %u", clTable, job->id);
char tmpHostFile[PATH_LEN*2];
if (sqlQuickQuery(conn, query, tmpHostFile, sizeof(tmpHostFile)))
    {
    char *colon =strchr(tmpHostFile, ':');
    int maxHostName=50;
    if (colon  != NULL && colon - tmpHostFile <= maxHostName)
        {
	char command[3*PATH_LEN];
	safef(command, sizeof(command), "ssh %s 'rcp %s %s'", 
	    paraHost, tmpHostFile, stderrTempFileName);
	mustSystem(command);
	}
    }
sqlDisconnect(&conn);


/* Read in stderr */
size_t errorMessageSize;
readInGulp(stderrTempFileName, &job->stderr, &errorMessageSize);
remove(stderrTempFileName);

/* Update database with job results */
struct dyString *dy = dyStringNew(0);
sqlDyStringPrintf(dy, "update %s set stderr='%s' where id=%u", clTable, trimSpaces(job->stderr), job->id);
conn = sqlConnect(clDatabase);
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
dyStringFree(&dy);

/* Free up runner resources. */
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

struct jobResult *waitOnResults(char *resultsFile, char *jobIdString)
/* Read results file until jobId appears in it, and then if necessary
 * copy over stderr to err, and finally exit with the same result
 * as command did. */
{
off_t resultsPos = 0;
int pollTime = 3;   // Poll every 3 seconds for file to open.
verbose(2, "waitOnResults(%s, %s)\n", resultsFile, jobIdString);
char *row[JOBRESULT_NUM_COLS];
for (;;)
    {
    struct lineFile *lf = lineFileOpen(resultsFile, TRUE);
    lineFileSeek(lf, resultsPos, SEEK_SET);
    while (lineFileRow(lf, row))
        {
	resultsPos = lineFileTell(lf);
	struct jobResult *jr;
	jr = jobResultLoad(row);
	if (sameString(jr->jobId, jobIdString))
	    {
	    return jr;
	    }
	jobResultFree(&jr);
	}
    lineFileClose(&lf);
    verbose(2, "waiting for %d\n", pollTime);
    sleep(pollTime);
    }
}

char *resultsQueue = "/hive/groups/encode/encode3/encodeAnalysisPipeline/queues/2/results";

char *startParaJob(struct edwJob *job)
/* Tell parasol about a job and return parasol job ID.  Free jobId when done.  */
{
/* Make sure results file exists. */
if (!fileExists(resultsQueue))
     {
     FILE *f = mustOpen(resultsQueue, "a");
     carefulClose(&f);
     }

char command[2048];
char *escaped = makeEscapedString(job->commandLine, '\'');
safef(command, sizeof(command),
    "ssh %s 'parasol -results=%s -printId cpu=2 ram=5500m add job %s'"
    , paraHost, resultsQueue, escaped);
freez(&escaped);

char jobIdLine[64];
edwOneLineSystemResult(command, jobIdLine, sizeof(jobIdLine));
char *jobIdString = cloneString(trimSpaces(jobIdLine));
return jobIdString;
}

void runJob(struct runner *runner, struct edwJob *job)
/* Fork off and run job. */
{
++curThreads;
job->startTime = edwNow();
runner->job = job;

char *jobIdString = startParaJob(job);
int childId;
if ((childId = mustFork()) == 0)
    {
    childPid = getpid();
    struct jobResult *jr = waitOnResults(resultsQueue, jobIdString);
    int status = -1;
    if (WIFEXITED(jr->status))
        {
	status = WEXITSTATUS(jr->status);
	}

    /* Save end time and process status and errFile to database. */
    struct sqlConnection *conn = sqlConnect(clDatabase);
    char query[3*PATH_LEN];
    sqlSafef(query, sizeof(query), 
	"update %s set endTime=%lld, returnCode=%d, stderr='%s:%s' where id=%u",
	clTable, edwNow(), status, jr->host, jr->errFile, job->id);

    sqlUpdate(conn, query);
    sqlDisconnect(&conn);
    exit(status);
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
    runner->pid = childId;

    freez(&jobIdString);
    }
}


void edwAnalysisDaemon(char *table, char *countString)
/* edwAnalysisDaemon - Run jobs remotely via parasol based on jobs in table.. */
{
clTable = table;

warn("Starting edwAnalysisDaemon on %s %s", clDatabase, table);

/* Set up array with a slot for each simultaneous job. */
maxThreadCount = sqlUnsigned(countString);
if (maxThreadCount > 512)
    errAbort("%s jobs at once? Really, that seems excessive", countString);
AllocArray(runners, maxThreadCount);
warn("Ugly and fast times for %d theads", maxThreadCount);

long long lastId = 0;  // Keep track of lastId in run table that has already been handled.

for (;;)
    {
    /* Finish up processing on any children that have finished */
    while (checkOnChildRunner(FALSE) != NULL)	
        ;

    /* Get next bits of work from database. */
    struct sqlConnection *conn = sqlConnect(clDatabase);
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
	sleep(10);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
paraHost = optionVal("paraHost", paraHost);
clDatabase = optionVal("database", clDatabase);
logDaemonize(argv[0]);
edwAnalysisDaemon(argv[1], argv[2]);
return 0;
}
