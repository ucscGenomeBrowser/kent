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
warn("ugly: Master - finish run status %d on %s", status, job->commandLine);

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

int waitOnResults(char *resultsFile, char *jobIdString, char *err)
/* Read results file until jobId appears in it, and then if necessary
 * copy over stderr to err, and finally exit with the same result
 * as command did. */
{
off_t resultsPos = 0;
int pollTime = 5;   // Poll every 5 seconds for file to open.
verbose(2, "waitOnResults(%s, %s, %s)\n", resultsFile, jobIdString, err);
warn("ugly: Child %d waitOnResults(%s, %s, %s)", childPid, resultsFile, jobIdString, err);
char *row[JOBRESULT_NUM_COLS];
for (;;)
    {
    struct lineFile *lf = lineFileOpen(resultsFile, TRUE);
    lineFileSeek(lf, resultsPos, SEEK_SET);
    warn("ugly: Child %d. resultsPos=%lld", childPid, (long long)resultsPos);
    while (lineFileRow(lf, row))
        {
	resultsPos = lineFileTell(lf);
	struct jobResult jr;
	jobResultStaticLoad(row, &jr);
	warn("ugly: Child %d. jobIdString %s vs. %s", childPid, jr.jobId, jobIdString);
	if (sameString(jr.jobId, jobIdString))
	    {
	    warn("ugly: Child %d. got match on %s err=%p, WIFEXITED(%d)=%d", childPid, jobIdString, err, jr.status, WIFEXITED(jr.status));
	    if (err != NULL)
	        warn("ugly: Child theoretically fetches from %s file %s to %s", jr.host, jr.errFile, err);
#ifdef SOON
		pmFetchFile(jr.host, jr.errFile, err);
#endif /* SOON */
	    warn("ugly: Child %d. Should be returning", childPid);
	    if (WIFEXITED(jr.status))
		return WEXITSTATUS(jr.status);
	    else
		return -1;	// Generic badness
	    }
	}
    lineFileClose(&lf);
    verbose(2, "waiting for %d\n", pollTime);
    sleep(pollTime);
    }
}

char *resultsQueue = "/hive/groups/encode/encode3/encodeAnalysisPipeline/queues/3/results";

char *startParaJob(struct edwJob *job, char *errFileName)
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
warn("ugly: Child %d escaped command line %s", childPid, escaped);
safef(command, sizeof(command),
    "ssh %s 'parasol -results=%s -printId"
    " -err=%s cpu=3 ram=5500m"
    " add job %s'"
    , paraHost, resultsQueue, errFileName, escaped);
warn("ugly: Child %d - attempting %s", childPid, command);
freez(&escaped);

char jobIdLine[64];
edwOneLineSystemResult(command, jobIdLine, sizeof(jobIdLine));
char *jobIdString = cloneString(trimSpaces(jobIdLine));
return jobIdString;
}

int paraWaitJob(char *jobIdString, char *errFileName)
/* Wait for job to finish and return error code. */
{
int status = waitOnResults(resultsQueue, jobIdString, errFileName);
warn("ugly: Child %d waitOnResults status %d for %s", childPid, status, jobIdString);
return status;
}


#ifdef OLD
int launchJob(struct edwJob *job, char *errFileName)
/* Pass job's command line to parasol. */
{
char *jobIdString = startParaJob(job, errFileName);
int status = paraWaitJob(jobIdString, errFileName);
return status;
}
#endif /* OLD */

char *eapParaTempDir = "/hive/groups/encode/encode3/encodeAnalysisPipeline/paraTmp/";

void runJob(struct runner *runner, struct edwJob *job)
/* Fork off and run job. */
{
/* Create stderr file for child  as a temp file */
char stderrTempFileName[PATH_LEN];
safef(stderrTempFileName, PATH_LEN, "%sedwAnalysisDaemonXXXXXX", eapParaTempDir);
int errFd = mkstemp(stderrTempFileName);
if (errFd < 0)
    errnoAbort("Couldn't open temp file %s", stderrTempFileName);
warn("ugly: Master - made temp file %s for stderr", stderrTempFileName);

++curThreads;
job->startTime = edwNow();
runner->job = job;

char *jobIdString = startParaJob(job, stderrTempFileName);
int childId;
if ((childId = mustFork()) == 0)
    {
    childPid = getpid();
    warn("ugly: Child %d - forked ok", childPid);
    int status = paraWaitJob(jobIdString, stderrTempFileName);
    warn("ugly: Child %d - launchJob %s returned with status %d", childPid, job->commandLine, status);
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
    warn("ugly: Master - updating sql database with start time and pid");

    /* We be parent - just fill in job info */
    close(errFd);
    runner->pid = childId;
    runner->errFileName = cloneString(stderrTempFileName);

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
if (maxThreadCount > 500)
    errAbort("%s jobs at once? Really, that seems excessive", countString);
AllocArray(runners, maxThreadCount);
warn("Ugly and intense times for %d theads", maxThreadCount);

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
	warn("ugly: Master Got job %s", job->commandLine);
	lastId = job->id;
	struct runner *runner = findFreeRunner();
	runJob(runner, job);
	}
    else
        {
	/* Wait for signal to come on named pipe or 10 seconds to pass */
	warn("ugly: Master going to wait");
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
