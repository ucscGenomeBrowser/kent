/* eapMonitor - Monitor jobs running through the pipeline.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "obscure.h"
#include "../../../../parasol/inc/jobResult.h"
#include "../../../../parasol/inc/paraMessage.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "eapLib.h"

/* Command line variables. */
char *clParaHost = NULL;
char *clDatabase = NULL;
char *clTable = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapMonitor - Monitor jobs running through the pipeline.\n"
  "usage:\n"
  "   eapMonitor command\n"
  "commands:\n"
  "   status - print overall pipeline activity\n"
  "   chill - remove jobs that haven't started from job table, but keep running ones going\n"
  "   cull N - remove jobs that have been running more than N days\n"
  "   err [id] print info about either most recent error or error of given id#\n"
  "options:\n"
  "   -database=<mysql database> - default %s\n"
  "   -table=<msqyl table> - default %s\n"
  "   -paraHost=<host name of paraHub> - default %s\n"
  , edwDatabase, eapJobTable, eapParaHost
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"database", OPTION_STRING},
   {"table", OPTION_STRING},
   {"paraHost", OPTION_STRING},
   {NULL, 0},
};

struct results
/* Information about a parasol results file */
    {
    struct results *next;
    char *pathName; /* may include a few slashes with the file name */
    struct slName *lineList;	/* List of contents line by line. */
    };

void searchForResults(char *dir, struct results **retResults)
/* Look in current dir and all subdirs for results files. */
{
char resultsPath[PATH_LEN];
safef(resultsPath, sizeof(resultsPath), "%s/results", dir);
if (fileExists(resultsPath))
     {
     struct results *results;
     AllocVar(results);
     results->pathName = cloneString(resultsPath);
     results->lineList = readAllLines(resultsPath);
     slAddHead(retResults, results);
     }
struct fileInfo *fi, *fiList = listDirX(dir, "*", TRUE);
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    if (fi->isDir)
	searchForResults(fi->name, retResults);
    }
}

int countParaRunning(struct sqlConnection *conn)
/* Count up number of jobs in are table that Parasol thinks are currently running */
{
int count = 0;
char query[512];
sqlSafef(query, sizeof(query), "select * from %s where startTime > 0 and endTime = 0", clTable);
struct hash *hash = eapParasolRunningHash(clParaHost, NULL);
struct edwAnalysisJob *job, *jobList = edwAnalysisJobLoadByQuery(conn, query);
for (job = jobList; job != NULL; job = job->next)
    {
    if (hashLookup(hash, job->parasolId))
        {
	++count;
	}
    }
hashFree(&hash);
return count;
}

int countOldJobs(struct sqlConnection *conn, double days)
/* Return number of jobs older than given number of days old */
{
int seconds = days * 3600 * 24;
long long limit = edwNow() - seconds;
char query[512];
safef(query, sizeof(query), "select count(*) from %s where startTime < %lld and endTime=0", 
    clTable, limit);
return sqlQuickNum(conn, query);
}

void doStatus()
/* Print out overall status */
{
struct results *q, *qList = NULL;
searchForResults(eapParaQueues, &qList);
int qCount = 0;
long finishedJobs = 0;
for (q = qList; q != NULL; q = q->next)
    {
    ++qCount;
    finishedJobs += slCount(q->lineList);
    }
printf("Queue dir: %s\n", eapParaQueues);
printf("Jobs finished: %ld\n", finishedJobs);
printf("Total batches: %d\n", qCount);

printf("Jobs table: %s\n", clTable);

struct sqlConnection *conn = edwConnect();
char query[512];
sqlSafef(query, sizeof(query), "select count(*) from %s", clTable);
int tableCount = sqlQuickNum(conn, query);
printf("DB Job count:      %d\n", tableCount);

sqlSafef(query, sizeof(query), "select count(*) from %s where startTime>0", clTable);
int dbJobsStarted = sqlQuickNum(conn, query);
printf("DB Jobs started:   %d\n", dbJobsStarted);

sqlSafef(query, sizeof(query), "select count(*) from %s where endTime > 0", clTable);
int dbJobsFinished = sqlQuickNum(conn, query);
printf("DB Jobs Finished:  %d\n", dbJobsFinished);

sqlSafef(query, sizeof(query), "select count(*) from %s where returnCode != 0", clTable);
int dbJobsCrashed = sqlQuickNum(conn, query);
printf("DB Jobs crashed:   %d\n", dbJobsCrashed);

sqlSafef(query, sizeof(query), "select count(*) from %s where startTime>0 and endTime=0", clTable);
int dbJobsPending = sqlQuickNum(conn, query);
printf("DB Jobs scheduled: %d\n", dbJobsPending);

sqlSafef(query, sizeof(query), "select count(*) from %s where startTime=0", clTable);
int dbJobsQueued = sqlQuickNum(conn, query);
printf("DB Jobs queued:    %d\n", dbJobsQueued);

printf("Jobs 1 days old:   %d\n", countOldJobs(conn, 1));
printf("Jobs 2 days old:   %d\n", countOldJobs(conn, 2));
printf("Jobs 3 days old:   %d\n", countOldJobs(conn, 3));
printf("Jobs 5 days old:   %d\n", countOldJobs(conn, 5));
printf("Jobs 10 days old:  %d\n", countOldJobs(conn, 10));

printf("parasol running:   %d\n", countParaRunning(conn));
}

void doChill()
/* Remove jobs we haven't gotten to yet */
{
struct sqlConnection *conn = sqlConnect(clDatabase);
char query[512];
safef(query, sizeof(query), "select count(*) from %s where startTime=0", clTable);
printf("Chilling %d jobs\n", sqlQuickNum(conn, query));
safef(query, sizeof(query), "delete from %s where startTime=0", clTable);
sqlUpdate(conn, query);
}

void doCull(double days)
/* Remove jobs that are older than a certain number of days. */
{
int seconds = days * 3600 * 24;
long long limit = edwNow() - seconds;
struct sqlConnection *conn = sqlConnect(clDatabase);
char query[512];
safef(query, sizeof(query), "select parasolId from %s where startTime < %lld and endTime=0", 
    clTable, limit);
struct slName *id, *idList = sqlQuickList(conn, query);
printf("Culling %d jobs\n", slCount(idList));
safef(query, sizeof(query), "delete from %s where startTime < %lld and endTime=0", 
    clTable, limit);
sqlUpdate(conn, query);
for (id = idList; idList != NULL; id = id->next)
    {
    char buf[256];
    safef(buf, sizeof(buf), "removeJob %s", id->name);
    pmHubSingleLineQuery(buf, clParaHost);
    }
}

void eapMonitor(char *command, int argc, char **argv)
/* eapMonitor - Monitor jobs running through the pipeline.. */
{
if (sameString(command, "status"))
    {
    doStatus();
    }
else if (sameString(command, "chill"))
    {
    doChill();
    }
else if (sameString(command, "cull"))
    {
    if (argc != 1)
         usage();
    double days = sqlDouble(argv[0]);
    doCull(days);
    }
else if (sameString(command, "err"))
    {
    uglyAbort("Can't handle errors.");
    }
else
    {
    errAbort("Unrecognized command %s", command);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
clParaHost = optionVal("paraHost", eapParaHost);
clDatabase = optionVal("database", edwDatabase);
clTable = optionVal("table", eapJobTable);
eapMonitor(argv[1], argc-2, argv+2);
return 0;
}
