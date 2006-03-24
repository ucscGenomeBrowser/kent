/* apacheMonitor -- check for error 500 */
#include "common.h"
#include "hdb.h"
#include "hgConfig.h"

char *host = NULL;
char *user = NULL;
char *password = NULL;
char *database = NULL;
int minutes = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "apacheMonitor - check for error 500s in the last minutes\n"
    "usage:\n"
    "    apacheMonitor host database minutes\n");
}

static char* getCfgValue(char* envName, char* cfgName)
/* get a configuration value, from either the environment or the cfg file,
 * with the env take precedence.
*/
{
char *val = getenv(envName);
if (val == NULL)
    val = cfgOption(cfgName);
    return val;
}    

int getTimeNow()
/* ask the database for it's unix time (seconds since Jan. 1, 1970) */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int ret = 0;

safef(query, sizeof(query), "select unix_timestamp(now())");
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("couldn't get current time\n");
ret = sqlUnsigned(row[0]);
sqlFreeResult(&sr);
return ret;
}

void checkFor500(int secondsNow)
/* report total row count and error 500 count, with details */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int startTime = secondsNow - (minutes * 60);
int hits = 0;
int errors = 0;

safef(query, sizeof(query), "select count(*) from access_log where time_stamp > %d", startTime);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("couldn't get row count\n");
hits = sqlUnsigned(row[0]);
verbose(1, "%d hits in the last %d minutes\n", hits, minutes);
sqlFreeResult(&sr);

safef(query, sizeof(query), "select count(*) from access_log where time_stamp > %d and status = 500", startTime);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("couldn't get error 500 row count\n");
errors = sqlUnsigned(row[0]);
verbose(1, "%d error 500\n", errors);
sqlFreeResult(&sr);

/* get details if necessary*/

if (errors == 0) return;
verbose(1, "remote_host  request_uri  machine  referer\n");
verbose(1, "-------------------------------------------\n");
safef(query, sizeof(query), "select remote_host, request_uri, machine_id, referer from access_log "
                            "where time_stamp > %d and status = 500", startTime);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {  
    verbose(1, "%s %s %s %s\n", row[0], row[1], row[2], row[3]);
    } 
sqlFreeResult(&sr);

}


int main(int argc, char *argv[])
/* Check args and call checkFor500. */
{
int timeNow = 0;
if (argc != 4)
    usage();
host = argv[1];
database = argv[2];
minutes = atoi(argv[3]);

user = getCfgValue("HGDB_USER", "db.user");
password = getCfgValue("HGDB_PASSWORD", "db.password");

hSetDbConnect(host, database, user, password);

timeNow = getTimeNow();
printf("time = %d\n", timeNow);
checkFor500(timeNow);
return 0;
}
