/* apacheMonitor -- check for error 500 */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hdb.h"
#include "hgConfig.h"
#include "hgRelate.h"
#include "options.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"store", OPTION_BOOLEAN}, 
    {NULL, 0} 
};


char *host = NULL;
char *user = NULL;
char *password = NULL;
char *database = NULL;
int minutes = 0;

int total = 0;

int hgw1count = 0;
int hgw2count = 0;
int hgw3count = 0;
int hgw4count = 0;
int hgw5count = 0;
int hgw6count = 0;
int hgw7count = 0;
int hgw8count = 0;
int mgccount = 0;

int robotcount = 0;

int status200 = 0; 
int status206 = 0; 
int status301 = 0;
int status302 = 0;
int status304 = 0;
int status400 = 0;
int status403 = 0;
int status404 = 0;
int status405 = 0;
int status408 = 0;
int status414 = 0;
int status500 = 0;


void usage()
/* Explain usage and exit. */
{
errAbort(
    "apacheMonitor - check for error 500s in the last minutes\n"
    "usage:\n"
    "    apacheMonitor host database minutes\n"
    "options:\n"
    "    -store  Write to status500 table");
}

void logStatus(int status)
{
switch (status)
    {
    case 200: 
        status200++;
        break;
    case 206: 
        status206++;
        break;
    case 301: 
        status301++;
        break;
    case 302: 
        status302++;
        break;
    case 304: 
        status304++;
        break;
    case 400: 
        status400++;
        break;
    case 403: 
        status403++;
        break;
    case 404: 
        status404++;
        break;
    case 405: 
        status405++;
        break;
    case 408: 
        status408++;
        break;
    case 414: 
        status414++;
        break;
    case 500: 
        status500++;
        break;
    }
}

void logMachine(char *machine)
{
if (sameString(machine, "hgw1"))
    {
    hgw1count++;
    return;
    }
if (sameString(machine, "hgw2"))
    {
    hgw2count++;
    return;
    }
if (sameString(machine, "hgw3"))
    {
    hgw3count++;
    return;
    }
if (sameString(machine, "hgw4"))
    {
    hgw4count++;
    return;
    }
if (sameString(machine, "hgw5"))
    {
    hgw5count++;
    return;
    }
if (sameString(machine, "hgw6"))
    {
    hgw6count++;
    return;
    }
if (sameString(machine, "hgw7"))
    {
    hgw7count++;
    return;
    }
if (sameString(machine, "hgw8"))
    {
    hgw8count++;
    return;
    }
}


void printDatabaseTime(struct sqlConnection *conn)
{
char query[512];
struct sqlResult *sr;
char **row;

sqlSafef(query, sizeof(query), "select now()");
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("couldn't get database time\n");
verbose(2, "\nCurrent date-time = %s\n\n", row[0]);
sqlFreeResult(&sr);
}

int getUnixTimeNow(struct sqlConnection *conn)
/* ask the database for it's unix time (seconds since Jan. 1, 1970) */
{
char query[512];
struct sqlResult *sr;
char **row;
int ret = 0;

sqlSafef(query, sizeof(query), "select unix_timestamp(now())");
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("couldn't get current time\n");
ret = sqlUnsigned(row[0]);
sqlFreeResult(&sr);
return ret;
}

void readLogs(struct sqlConnection *conn, int secondsNow, boolean write500)
/* read access_log where time_stamp > startTime */
/* write error 500 to fileName */
{
char fileName[255];
char query[512];
struct sqlResult *sr;
char **row;
int startTime = secondsNow - (minutes * 60);
FILE *outputFileHandle = NULL;
int status = 0;

if (write500)
    {
    safef(fileName, ArraySize(fileName), "/scratch/apacheMonitor/%d.tab", secondsNow);
    outputFileHandle = mustOpen(fileName, "w");
    }

sqlSafef(query, sizeof(query), "select remote_host, machine_id, status, time_stamp, "
                            "request_method, request_uri, request_line, referer, agent "
                            "from access_log where time_stamp > %d", startTime);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {  
    status = sqlUnsigned(row[2]);
    logStatus(status);
    logMachine(row[1]);
    if (isEmpty(row[7]) || sameString(row[7], "-")) robotcount++;
    total++;
    if (status == 500 && write500)
	{
	if (row[6] == NULL) row[6] = "";
	if (row[7] == NULL) row[7] = "";
        fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
	        row[0], row[1], row[3], row[4], row[5], row[6], row[7], row[8]);
	}
    } 
sqlFreeResult(&sr);
if (write500) carefulClose(&outputFileHandle);
}

void printBots(struct sqlConnection *conn, int secondsNow)
/* who is faster than a hit every 5 seconds on average? */
{
char query[512];
struct sqlResult *sr;
char **row;
int startTime = secondsNow - (minutes * 60);
int hits = 0;

verbose(2, "\nheavy hitters overall (all status values, less than 5 seconds between average hits):\n");
sqlSafef(query, sizeof(query), "select count(*), remote_host from access_log where time_stamp > %d group by remote_host", startTime);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hits = sqlUnsigned(row[0]);
    if (hits > minutes * 12)
        verbose(2, "%s\t%s\n", row[0], row[1]);
    }
sqlFreeResult(&sr);
}



void desc500(struct sqlConnection *conn, int secondsNow)
{
char query[512];
struct sqlResult *sr;
char **row;
int startTime = secondsNow - (minutes * 60);

if (status500)
    {
    sqlSafef(query, sizeof(query),
	  "select from_unixtime(time_stamp), remote_host, machine_id, request_uri, referer "
	  "from access_log "
	  "where time_stamp > %d and status = 500 order by time_stamp desc limit 20\n", startTime);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        verbose(1, "%s\t%s\t%s\t%s\t%s\n", row[0], row[1], row[2], row[3], row[4]);
    sqlFreeResult(&sr);
    return;
    }

sqlSafef(query, sizeof(query), "select count(*) from access_log where time_stamp > %d and status = 500 and referer = '-'", startTime);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    verbose(1, "500 from robots: %s\n", row[0]);
else
    verbose(2, "none from robots\n");
sqlFreeResult(&sr);

verbose(2, "\nmachine_ids (non-robots):\n");
sqlSafef(query, sizeof(query), "select count(*), machine_id from access_log where time_stamp > %d and status = 500 and "
                            "referer != '-' group by machine_id", startTime);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    verbose(2, "%s\t%s\n", row[0], row[1]);
sqlFreeResult(&sr);

verbose(2, "\nremote_hosts:\n");
sqlSafef(query, sizeof(query), "select count(*), remote_host from access_log where time_stamp > %d and status = 500 group by remote_host", startTime);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    verbose(2, "%s\t%s\n", row[0], row[1]);
sqlFreeResult(&sr);

verbose(2, "\ndistinct request_uris:\n");
sqlSafef(query, sizeof(query), "select distinct(request_uri) from access_log where time_stamp > %d and status = 500", startTime);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    verbose(2, "%s\n", row[0]);
sqlFreeResult(&sr);
}

void printMachines()
{
verbose(2, "Count by machine: \n");
verbose(2, "hgw1 = %d\n", hgw1count);
verbose(2, "hgw2 = %d\n", hgw2count);
verbose(2, "hgw3 = %d\n", hgw3count);
verbose(2, "hgw4 = %d\n", hgw4count);
verbose(2, "hgw5 = %d\n", hgw5count);
verbose(2, "hgw6 = %d\n", hgw6count);
verbose(2, "hgw7 = %d\n", hgw7count);
verbose(2, "hgw8 = %d\n\n", hgw8count);
}

void printStatus()
{
verbose(2, "Count by status: \n");
verbose(2, "200: %d\n", status200);
verbose(1, "206: %d\n", status206); 
verbose(1, "301: %d\n", status301);
verbose(1, "302: %d\n", status302);
verbose(1, "304: %d\n", status304);
verbose(1, "400: %d\n", status400);
verbose(1, "403: %d\n", status403);
verbose(1, "404: %d\n", status404);
verbose(1, "405: %d\n", status405);
verbose(1, "408: %d\n", status408);
verbose(1, "414: %d\n", status414);
verbose(1, "500: %d\n\n", status500);
}

void store500(struct sqlConnection *conn, int timeNow)
{
char localfileName[255];
char fullfileName[255];
char tableName[255];
FILE *f;

/* open the file because hgLoadNamedTabFile closes it */
safef(fullfileName, ArraySize(fullfileName), "/scratch/apacheMonitor/%d.tab", timeNow);
f = mustOpen(fullfileName, "r");

safef(tableName, ArraySize(tableName), "status500");
safef(localfileName, ArraySize(localfileName), "%d", timeNow);
hgLoadNamedTabFile(conn, "/scratch/apacheMonitor", tableName, localfileName, &f);
}

void cleanup(int timeNow)
{
char command[255];
safef(command, ArraySize(command), "rm /scratch/apacheMonitor/%d.tab", timeNow);
mustSystem(command);
}

int main(int argc, char *argv[])
/* Check args and call readLogs. */
{
int timeNow = 0;

optionInit(&argc, argv, optionSpecs); 

if (argc != 4)
    usage();
host = argv[1];
database = argv[2];
minutes = atoi(argv[3]);

user = cfgOptionEnv("HGDB_USER", "db.user");
password = cfgOptionEnv("HGDB_PASSWORD", "db.password");
if (user == NULL || password == NULL)
    errAbort("Both db and password must be defined as environment variables "
	     "(HGDB_USER, HGDB_PASSWORD) or as hg.conf (db.user, db.password).");
struct sqlConnection *conn = sqlConnectRemote(host, user, password, database);

printDatabaseTime(conn);
timeNow = getUnixTimeNow(conn);
readLogs(conn, timeNow, optionExists("store"));
verbose(2, "Total hits in the last %d minutes = %d\n", minutes, total);
verbose(2, "Hits from robots = %d\n\n", robotcount);
printMachines();
printStatus();
if (status500 >= 10)
    desc500(conn, timeNow);
if (optionExists("store") && status500 > 0)
    {
    store500(conn, timeNow);
    cleanup(timeNow);
    }
printBots(conn, timeNow);
sqlDisconnect(&conn);
return 0;
}
