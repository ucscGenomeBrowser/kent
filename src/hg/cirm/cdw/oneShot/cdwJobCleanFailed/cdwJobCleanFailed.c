/* cdwJobCleanFailed - scan cdwJob table to remove failed jobs that later succeeded. 

 * This is needed until a bug in cdwValidation retry command gets fixed.
 * Basically, retry does manage to retry ok the first time. But if you run 
 * it again, it will keep trying to re-run everything that has failed,
 * regardless of whether it later succeeded.

 * CURRENT LIMITATION : This utility should be run AFTER all the jobs in the previous retry are done.
 * So there are no queued or running jobs for the given submitId.
 */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"


//char *prefix = "";
//boolean brokenOnly = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwJobCleanFailed - scan symlinks list to determine the target type.\n"
  "usage:\n"
  " cdwJobCleanFailed submitId\n"
//  "options:\n"
  "\n"
  "CURRENT LIMITATION : \n"
  "This utility should be run AFTER all the jobs in the previous retry are done.\n"
  "So there are no queued or running jobs for the given submitId.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   //{"prefix", OPTION_STRING},
   //{"brokenOnly", OPTION_BOOLEAN},
   {NULL, 0},
};


void cdwJobCleanFailed(int submitId)
/* Check out the symlink to determine its type. */
{

struct sqlConnection *conn = sqlConnect("cdw");

struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, 
 "select id, commandLine, startTime, endTime, returnCode, pid from cdwJob where submitId=%d "
"order by commandLine,CAST(returnCode AS unsigned)", 
 submitId);
 // NOTE we need this CAST on returnCode since it can be -1. we want success 0 first.

// TODO DO we need to add any other conditions such as distinguishing
// between running, queued, and done?

/* Scan through result set finding redundant rows beyond success row. */
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
char *lastCommand = "";
boolean success = FALSE;
struct slInt *list = NULL;
struct slInt *e;
while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned int id = sqlUnsigned(row[0]);
    char *commandLine = row[1];
    unsigned long startTime = sqlUnsignedLong(row[2]);
    unsigned long endTime = sqlUnsignedLong(row[3]);
    int returnCode = sqlSigned(row[4]);
    unsigned int pid = sqlUnsigned(row[5]);
    verbose(2, "%u\t%s\t%lu\t%lu\t%d\t%u\t%u\n", id, commandLine, startTime, endTime, returnCode, pid, submitId);
    if (sameString(lastCommand, commandLine))
	{
	if (success)  // we already succeeded, the old failure is unwanted baggage.
	    {
	    e = slIntNew(id);  // or add it to a list of rows whose ids should get removed
	    slAddHead(&list, e);
	    }
	}
    else
	{
	if (returnCode == 0)
	    success = TRUE;
	else
	    success = FALSE;
	}
    // note fields pid and submitId are defined as signed integers in cdwJob table, probably should be unsigned.
    lastCommand = cloneString(commandLine);
    }
                                                                              
sqlFreeResult(&sr);

slReverse(&list);
for(e=list;e;e=e->next)
    {
    dyStringClear(query);
    sqlDyStringPrintf(query, "delete from cdwJob where id=%u", (unsigned int) e->val);
    //printf("%s\n", query->string);
    sqlUpdate(conn, query->string);
    }

/* Clean up and go home */
dyStringFree(&query);

sqlDisconnect(&conn);


}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
//prefix = optionVal("prefix", prefix);
//brokenOnly = optionExists("brokenOnly");
cdwJobCleanFailed(sqlUnsigned(argv[1]));
return 0;
}
