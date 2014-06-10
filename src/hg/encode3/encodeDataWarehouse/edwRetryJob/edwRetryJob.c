/* edwRetryJob - Add jobs that failed back to a edwJob format queue.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

/* Variables that are set from command line. */
boolean dry = FALSE;
int minId = 0;
double maxTime = 1.0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwRetryJob - Add jobs that failed back to a edwJob format queue.\n"
  "usage:\n"
  "   edwRetryJob database jobTable\n"
  "options:\n"
  "   -dry - dry run, just print jobs that would rerun\n"
  "   -minId=minimum ID of job to retry, default %d\n"
  "   -maxTime=N.N - Maximum time (in days) for a job to succeed. Default %2.1g\n"
  , minId, maxTime
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dry", OPTION_BOOLEAN},
   {"minId", OPTION_DOUBLE},
   {"maxTime", OPTION_DOUBLE},
   {NULL, 0},
};

void edwRetryJob(char *database, char *jobTable)
/* edwRetryJob - Add jobs that failed back to a edwJob format queue.. */
{
/* Figure out thresholds for age. */
long long now = edwNow();
long long maxTimeThreshold = now - 24*3600 * maxTime;

/* Get list of all jobs ordered by ID with most recent job first. */
struct sqlConnection *conn = sqlConnect(database);
char query[512];
sqlSafef(query, sizeof(query), "select * from %s order by id desc", jobTable);
struct edwJob *job, *jobList = edwJobLoadByQuery(conn, query);

/* Loop through job list looking for commandLines that failed or timed out  in their
 * most recent attempt. */
struct hash *jobHash = hashNew(0);
struct slRef *failRef, *failRefList = NULL;
for (job = jobList; job != NULL; job = job->next)
    {
    if (hashLookup(jobHash, job->commandLine) == NULL)
        {
	hashAdd(jobHash, job->commandLine, job);
	if (job->id >= minId && job->startTime != 0)
	    {
	    boolean timedOut = (job->startTime < maxTimeThreshold && job->endTime == 0);
	    if (job->returnCode != 0 || timedOut)
	        {
		refAdd(&failRefList, job);
		}
	    }
	}
    }

/* Loop through failed list printing or retrying */
for (failRef = failRefList; failRef != NULL; failRef = failRef->next)
    {
    job = failRef->val;
    if (dry)
	printf("%s\n", job->commandLine);
    else
	{
	sqlSafef(query, sizeof(query), 
	    "insert into %s (commandLine) values ('%s')",  jobTable, job->commandLine);
	sqlUpdate(conn, query);
	}
    } 
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
dry = optionExists("dry");
minId = optionInt("minId", minId);
maxTime = optionDouble("maxTime", maxTime);
edwRetryJob(argv[1], argv[2]);
return 0;
}
