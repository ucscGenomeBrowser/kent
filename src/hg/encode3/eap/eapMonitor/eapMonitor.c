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
  "   info - information about configuration\n"
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


int countParaRunning(struct sqlConnection *conn, struct hash *parasolRunningHash)
/* Count up number of jobs in are table that Parasol thinks are currently running */
{
int count = 0;
char query[512];
sqlSafef(query, sizeof(query), "select * from %s where startTime > 0 and endTime = 0", clTable);
struct edwAnalysisJob *job, *jobList = edwAnalysisJobLoadByQuery(conn, query);
for (job = jobList; job != NULL; job = job->next)
    {
    if (hashLookup(parasolRunningHash, job->parasolId))
        {
	++count;
	}
    }
return count;
}

int countOldJobs(struct sqlConnection *conn, double days)
/* Return number of jobs older than given number of days old */
{
int seconds = days * 3600 * 24;
long long limit = edwNow() - seconds;
char query[512];
safef(query, sizeof(query), 
    "select count(*) from %s where startTime != 0 and startTime < %lld and endTime=0", 
    clTable, limit);
return sqlQuickNum(conn, query);
}

int printDaysOldJobs(struct sqlConnection *conn, int days)
/* Print info about job more than %d days old if any.  Return that count.  */
{
int jobCount = countOldJobs(conn, days);
if (jobCount > 0)
    printf("Jobs %d day old:    %-4d\n", days, countOldJobs(conn, days));
return jobCount;
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
printf("Jobs finished: %ld\n", finishedJobs);
printf("Total batches: %d\n", qCount);

struct sqlConnection *conn = edwConnect();
char query[512];
sqlSafef(query, sizeof(query), "select count(*) from %s", clTable);
int tableCount = sqlQuickNum(conn, query);
printf("DB Job count:      %d\n", tableCount);

sqlSafef(query, sizeof(query), "select count(*) from %s where startTime>0", clTable);
int dbJobsStarted = sqlQuickNum(conn, query);
printf("DB Jobs started:   %d\n", dbJobsStarted);

printf("Jobs 12 hours old: %-4d\n", countOldJobs(conn, 0.5));
printDaysOldJobs(conn, 1);
printDaysOldJobs(conn, 2);
printDaysOldJobs(conn, 3);
printDaysOldJobs(conn, 5);
printDaysOldJobs(conn, 10);

struct hash *runningHash = eapParasolRunningHash(clParaHost, NULL);
int runningCount = countParaRunning(conn, runningHash);
if (runningCount > 0)
    {
    sqlSafef(query, sizeof(query), "select min(startTime) from %s where startTime>0 and endTime=0",
	clTable);
    double oldestSeconds = edwNow() - sqlQuickNum(conn, query);
    printf("Oldest job:        %-5.2f hours\n", oldestSeconds/(3600));
    sqlSafef(query, sizeof(query), "select max(startTime) from %s where startTime>0 and endTime=0",
	clTable);
    double youngestSeconds = edwNow() - sqlQuickNum(conn, query);
    printf("Youngest job:      %-5.2f hours\n", youngestSeconds/(3600));
    }

sqlSafef(query, sizeof(query), "select count(*) from %s where startTime>0 and endTime=0", clTable);
int dbJobsPending = sqlQuickNum(conn, query);
printf("Jobs scheduled: %7d\n", dbJobsPending);

sqlSafef(query, sizeof(query), "select count(*) from %s where startTime=0", clTable);
int dbJobsQueued = sqlQuickNum(conn, query);
printf("Jobs queued:    %7d\n", dbJobsQueued);

sqlSafef(query, sizeof(query), "select count(*) from %s where returnCode != 0", clTable);
int dbJobsCrashed = sqlQuickNum(conn, query);
printf("Jobs crashed:   %7d\n", dbJobsCrashed);

sqlSafef(query, sizeof(query), "select max(startTime) from %s where returnCode != 0", clTable);
long long lastCrashTime = sqlQuickLongLong(conn, query);
printf("Last crash:     %5.2g hours ago\n", (double)(edwNow() - lastCrashTime)/3600);

sqlSafef(query, sizeof(query), "select count(*) from %s where endTime > 0", clTable);
int dbJobsFinished = sqlQuickNum(conn, query);
printf("Jobs finished:  %7d\n", dbJobsFinished);

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

void doInfo()
/* Print out configuration info. */
{
printf("Queue dir:     %s\n", eapParaQueues);
printf("Database:      %s\n", clDatabase);
printf("Jobs table:    %s\n", clTable);
printf("Parasol host:  %s\n", clParaHost);
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
else if (sameString(command, "info"))
    {
    doInfo();
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
