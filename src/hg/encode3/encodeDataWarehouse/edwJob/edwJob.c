/* edwJob - Look at a edwJob format table and report what's up, optionally do something about it.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

char *clDatabase = NULL;
char *clTable = "edwJob";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwJob - Look at a edwJob format table and report what's up, optionally do something about it.\n"
  "usage:\n"
  "   edwJob command\n"
  "Where commands are:\n"
  "   status - report overall status\n"
  "   count - just count table size\n"
  "   failed - list failed jobs\n"
  "   running - list running jobs\n"
  "   fihished - list finished (but not failed) jobs\n"
  "options:\n"
  "   -database=<mysql database> default %s\n"
  "   -table=<mysql table> default %s\n"
  , edwDatabase, clTable
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"database", OPTION_STRING},
   {"table", OPTION_STRING},
   {NULL, 0},
};

void statusCommand(struct sqlConnection *conn)
/* Handle status command. */
{
char query[512];
sqlSafef(query, sizeof(query), "select * from %s ", clTable);
struct edwJob *list = edwJobLoadByQuery(conn, query);
printf("database %s\n", clDatabase);
printf("table %s\n", clTable);

/* Declare list of stats we want to gather from list. */
long long count = 0;	// Number of rows in table.
long long successes = 0;  // Number of successful runs
long long failures = 0;  // Number of failed runs
long long waiting = 0;  // Number of jobs waiting to be started
long long running = 0;  // Number of jobs currently running

/* Loop through whole list gathering stats. */
struct edwJob *job;
for (job = list; job != NULL; job = job->next)
    {
    count += 1;
    if (job->startTime == 0)
        ++waiting;
    else if (job->returnCode != 0)
        ++failures;
    else if (job->endTime == 0)
        ++running;
    else 
        ++successes;
    }

/* Print out stats. */
printf("count %lld\n", count);
printf("successes %lld\n", successes);
printf("failures %lld\n", failures);
printf("waiting %lld\n", waiting);
printf("running %lld\n", running);
}

void countCommand(struct sqlConnection *conn)
/* Handle status command. */
{
char query[512];
sqlSafef(query, sizeof(query), "select count(*) from %s ", clTable);
printf("%d\n", sqlQuickNum(conn, query));
}

void listJobCommand(struct sqlConnection *conn, char *command)
/* Handle some list oriented command command. */
{
char query[512];
if (sameString(command, "failed"))
    sqlSafef(query, sizeof(query), "select * from %s where returnCode != 0 order by id", clTable);
else if (sameString(command, "running"))
    sqlSafef(query, sizeof(query), 
	    "select * from %s where startTime > 0 and endTime = 0"
	    "and returnCode = 0 order by id", clTable);
else if (sameString(command, "finished"))
    sqlSafef(query, sizeof(query), 
	    "select * from %s where returnCode = 0 and endTime > 0 order by id", clTable);
else if (sameString(command, "waiting"))
    sqlSafef(query, sizeof(query), "select * from %s where startTime = 0 order by id", clTable);
else 
    errAbort("Unrecognized command %s", command);

struct edwJob *list = edwJobLoadByQuery(conn, query);
struct edwJob *job;
long long now = edwNow();
printf("#time\tcode\tstart\tend\tcommandLine\n");
for (job = list; job != NULL; job = job->next)
    {
    int duration = 0;
    if (job->endTime > job->startTime)
        duration = job->endTime - job->startTime;
    else if (job->startTime > 0)
        duration = now - job->startTime;
    printf("%d\t%d\t%lld\t%lld\t%s\n", duration, job->returnCode, 
	job->startTime, job->endTime, job->commandLine);
    }
}


void edwJob(char *command)
/* edwJob - Look at a edwJob format table and report what's up, optionally do something about it.. */
{
struct sqlConnection *conn = sqlConnect(clDatabase);
if (sameString(command, "status"))
    {
    statusCommand(conn);
    }
else if (sameString(command, "count"))
    {
    countCommand(conn);
    }
else if (sameString(command, "failed")
      || sameString(command, "running")
      || sameString(command, "finished")
      || sameString(command, "waiting") )
    {
    listJobCommand(conn, command);
    }
else
    {
    errAbort("Unknown edwJob command %s", command);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
clDatabase = optionVal("database", edwDatabase);
clTable = optionVal("table", clTable);
edwJob(argv[1]);
return 0;
}
