/* eapDaemon - Run jobs remotely via parasol based on jobs in table.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include <sys/wait.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "errAbort.h"
#include "portable.h"
#include "rbTree.h"
#include "obscure.h"
#include "net.h"
#include "log.h"
#include "sqlNum.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "verbose.h"
#include "../../../../parasol/inc/jobResult.h"
#include "../../../../parasol/inc/paraMessage.h"
#include "eapDb.h"
#include "eapLib.h"

/* Variables set from command line */
char *clDatabase = NULL;
char *clTable = NULL;
char *clParaHost = NULL;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapDaemon - Run jobs remotely via parasol based on jobs in table.\n"
  "usage:\n"
  "   eapDaemon count\n"
  "where:\n"
  "   count - number of simultanious jobs to run\n"
  "options:\n"
  "   -database=%s - mySQL database where eapJob table lives\n"
  "   -table=%s - table in eapJob format to use\n"
  "   -debug - don't fork, close stdout, and daemonize self\n"
  "   -log=logFile - send error messages and warnings of daemon itself to logFile\n"
  "        There are not many of these.  Error mesages from jobs daemon runs end up\n"
  "        in errorMessage fields of database.\n"
  "   -logFacility - sends error messages and such to system log facility instead.\n"
  "   -paraHost - machine running parasol (paraHub in particular)\n"
  , edwDatabase, eapJobTable
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"database", OPTION_STRING},
   {"table", OPTION_STRING},
   {"log", OPTION_STRING},
   {"logFacility", OPTION_STRING},
   {"debug", OPTION_BOOLEAN},
   {"paraHost", OPTION_STRING},
   {NULL, 0},
};


int cmpByParasolId(void *a, void *b)
/* Set up rbTree so as to work on strings. */
{
struct eapJob *aJob = a, *bJob = b;
return strcmp(aJob->parasolId, bJob->parasolId);
}

struct resultsQueue
/* A file with job results in it */
    {
    struct resultsQueue *next;
    char *fileName;	/* Results file shared with parasol. */
    long long pos;	/* Current position in file. */
    boolean believePos; /* Set to true if you actually trust position */
    };

struct resultsQueue *resultsQueueNew(char *name, boolean seekToEnd)
/* Return a new results queue */
{
struct resultsQueue *queue;
AllocVar(queue);
queue->fileName = cloneString(name);
if (fileExists(queue->fileName))
    {
    if (seekToEnd)
	queue->pos = fileSize(queue->fileName);
    }
else
    {
    char dir[PATH_LEN],root[FILENAME_LEN],ext[FILEEXT_LEN];
    splitPath(name, dir, root, ext);
    makeDirsOnPath(dir);
    FILE *f = mustOpen(name, "a");
    carefulClose(&f);
    }
return queue;
}

struct resultsQueue *allQueues = NULL;

struct resultsQueue *findOrCreateQueue(char *name, boolean seekToEndOnNew)
/* Find name in existing list of results queue or if it's not there make it. */
{
struct resultsQueue *queue;
for (queue = allQueues; queue != NULL; queue = queue->next)
    {
    if (sameString(queue->fileName, name))
         return queue;
    }
queue = resultsQueueNew(name, seekToEndOnNew);
slAddHead(&allQueues, queue);
return queue;
}


void sendToParasol(struct eapJob *job, struct resultsQueue *queue)
/* Add job to current parasol queue. Sets job->parasolId and saves it
 * to database. */
{
/* Prepare parasol add job command and send it to hub. */
int cpu = (job->cpusRequested ?  job->cpusRequested : 1);
long long ram = 8LL * 1024 * 1024 * 1024;
struct dyString *cmd = dyStringNew(1024);
dyStringPrintf(cmd, "addJob2 %s %s /dev/null /dev/null %s %f %lld %s",
    getUser(), getCurrentDir(), queue->fileName, (double)cpu, ram, job->commandLine);
char *jobIdString = pmHubSingleLineQuery(cmd->string, clParaHost);
verbose(1, "%s: %s\n", jobIdString, cmd->string);

/* Check for sick batch result */
if (sameString(jobIdString, "0"))
    {
    warn("command: %s", cmd->string);
    errAbort("Looks like paraHub has decided we're sick.  It's usually right.");
    }

/* Save to data structure in memory and to database. */
job->parasolId = cloneString(jobIdString);
struct sqlConnection *conn = sqlConnect(clDatabase);
char query[512];
sqlSafef(query, sizeof(query), 
    "update %s set startTime=%lld, parasolId='%s' where id=%u", 
    clTable, edwNow(), jobIdString, job->id);
sqlUpdate(conn, query);
sqlDisconnect(&conn);

dyStringFree(&cmd);
}

void finishJob(struct eapJob *job, struct jobResult *jr) 
/* Move parasol job result into eapJob table */
{
int status = -1;
if (WIFEXITED(jr->status))
    status = WEXITSTATUS(jr->status);

/* Save end time and process status and errFile to database. */
struct sqlConnection *conn = sqlConnect(clDatabase);
char query[512];
sqlSafef(query, sizeof(query), 
    "update %s set endTime=%lld, returnCode=%d, stderr='%s:%s' where id=%u",
    clTable, edwNow(), status, jr->host, jr->errFile, job->id);
sqlUpdate(conn, query);
sqlDisconnect(&conn);
}

int finishJobsWithResults(struct rbTree *running, struct resultsQueue *queueList)
/* Go through all results queues and look for jobs that are in our running container
 * that have finished.  For these jobs transport results to database and remove them
 * from container. Returns the number of finished jobs. */
{
int finishedCount = 0;
struct resultsQueue *queue;
for (queue = queueList; queue != NULL; queue = queue->next)
    {
    struct lineFile *lf = lineFileOpen(queue->fileName, TRUE);
    lineFileSeek(lf, queue->pos, SEEK_SET);
    for (;;)
	{
	char *line;
	if (!lineFileNextReal(lf, &line))
	    break;
	char *row[JOBRESULT_NUM_COLS];
	int wordsRead = chopByWhite(line, row, ArraySize(row));
	if (wordsRead < ArraySize(row))
	    {
	    if (queue->believePos)
		errAbort("%s seems screwed up, expecting %d words in line got %d", 
		    lf->fileName, (int)ArraySize(row), wordsRead);
	    else
		{
		continue;   // Forgive bad first line in case initial position, based on file size
			    // is not at an even line boundary.
		}
	    }
	queue->believePos = TRUE;
	struct jobResult *jr;
	jr = jobResultLoad(row);
	struct eapJob jobKey = {.parasolId = jr->jobId,};
	struct eapJob *job = rbTreeFind(running, &jobKey);
	if (job != NULL)
	    {
	    finishJob(job, jr);
	    rbTreeRemove(running, job);
	    eapJobFree(&job);
	    finishedCount += 1;
	    }
	jobResultFree(&jr);
	}
    queue->pos = lineFileTell(lf);
    lineFileClose(&lf);
    }
return finishedCount;
}

int checkFreeThreads(struct rbTree *running, struct resultsQueue *queueList, boolean mustWait)
/* Wait on the queue for one of our jobs to come in. */
{
int freeCount = 0;
int pollTime = 3;   // Poll every 3 seconds for file to open.
while (freeCount == 0)
    {
    freeCount = finishJobsWithResults(running, queueList);
    verbose(2, "waiting for %d\n", pollTime);
    if (!mustWait)
	break;
    if (freeCount == 0)
	sleep(pollTime);
    }
return freeCount;
}

void eapDaemon(char *countString)
/* eapDaemon - Run jobs remotely via parasol based on jobs in table.. */
{
verbose(1, "Starting eapDaemon v16 on %s %s with %s threads.\n", 
    clDatabase, clTable, countString);
int maxThreads = sqlUnsigned(countString);

/* On start up we first try to connect to any jobs that were started but not finished
 * at time daemon died last time around. */

/* Look for any jobs mentioned in table as started but not finished */
char query[256];
struct sqlConnection *conn = sqlConnect(clDatabase);
sqlSafef(query, sizeof(query), 
    "select * from %s where startTime > 0 and endTime = 0 order by id", clTable);
struct eapJob *oldJob, *oldJobList = eapJobLoadByQuery(conn, query);
verbose(1, "Got %d old jobs in %s to reconnect to\n", slCount(oldJobList), clTable);

/* Set up rbTree as a convenient quick access container for jobs */
struct rbTree *running = rbTreeNew(cmpByParasolId);

/* Add old jobs to container and get associated queues */
for (oldJob = oldJobList; oldJob != NULL; oldJob = oldJob->next)
    {
    char *commandName = eapStepFromCommandLine(oldJob->commandLine);
    char queueName[PATH_LEN];
    safef(queueName, sizeof(queueName), "%s/%s/results", eapParaDirs(conn), commandName);
    findOrCreateQueue(queueName, FALSE);
    rbTreeAdd(running, oldJob);
    freez(&commandName);
    }
oldJobList = NULL;	// We've transfered ownership of jobs in list to container
int oldFinishedCount = finishJobsWithResults(running, allQueues);  // This updates database

/* Figure out jobs that are *still* running in one of our batches.  These
 * we'll add to our count of running threads. */
oldJobList = eapJobLoadByQuery(conn, query);  // Reload hopefully diminished old job list
struct hash *currentlyRunningHash = eapParasolRunningHash(clParaHost, NULL);
int oldRunningCount = 0;
for (oldJob = oldJobList; oldJob != NULL; oldJob = oldJob->next)
    {
    if (hashLookup(currentlyRunningHash, oldJob->parasolId) != NULL)
	oldRunningCount += 1;
    }
hashFree(&currentlyRunningHash);
eapJobFree(&oldJobList);
sqlDisconnect(&conn);

verbose(1, "Reconnected to %d finished and %d running old jobs\n", 
    oldFinishedCount, oldRunningCount);

/* Ok, we are done with all the old business.  Now onto the main loop where
 * where we poll parasol for finished jobs and database for new jobs. */
int threadCount = oldRunningCount;
for (;;)
    {
    /* Process finished jobs, if need be waiting for some to finish. */
    boolean tooManyThreads = (threadCount >= maxThreads);
    int freeThreads = checkFreeThreads(running, allQueues, tooManyThreads);
    threadCount -= freeThreads;

    /* Get next job from database. */
    conn = sqlConnect(clDatabase);  // It may have been a while so open fresh connection
    sqlSafef(query, sizeof(query), 
	"select * from %s where startTime = 0 order by id limit 1", clTable);
    struct eapJob *job = eapJobLoadByQuery(conn, query);

    if (job != NULL)
        {
	/* Cool, got a job, send it to parasol and track it in running container */
	char *commandName = eapStepFromCommandLine(job->commandLine);
	char queueName[PATH_LEN];
	safef(queueName, sizeof(queueName), "%s/%s/results", eapParaDirs(conn), commandName);
	struct resultsQueue *queue = findOrCreateQueue(queueName, TRUE);
	sendToParasol(job, queue);
	rbTreeAdd(running, job);
	threadCount += 1;
	freez(&commandName);
	}
    else
        {
	/* No new jobs, maybe we'll nap for 10 seconds */
	sleep(10);
	}
    sqlDisconnect(&conn);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
clParaHost = optionVal("paraHost", eapParaHost);
clDatabase = optionVal("database", edwDatabase);
clTable = optionVal("table", eapJobTable);
logDaemonize(argv[0]);
if (optionExists("log"))
    verboseSetLogFile(optionVal("log", NULL));
eapDaemon(argv[1]);
return 0;
}
