/* hgEncodeScheduler - a chron job that runs ENCODE pipeline load jobs 
 *   that are ready for scheduling.  It reads the encpipeline database
 *   looking for submissions with status == "shedule loading".
 *   It starts the load job, records the process id, changes the status
 *   to "loading" and looks for old processes.  If those have timed out,
 *   it kills them and sets the status to "load failed" (and possibly
 *   do any other cleanup needed?)
 */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hgConfig.h"
#include "jksql.h"
#include "portable.h"
#include <signal.h>
#include <sys/wait.h>

static char const rcsid[] = "$Id: hgEncodeScheduler.c,v 1.1 2007/10/26 22:10:43 galt Exp $";

char *db = NULL;
char *dir = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgEncodeScheduler - a chron job that runs ENCODE pipeline load jobs "
  "that are ready for scheduling\n"
  "usage:\n"
  "   hgEncodeScheduler db working\n"
  "where db is the encpipeline database\n"
  "and working is the working file area\n"
  "options:\n"
  );
}

void initRunning(struct sqlConnection *conn)
/* create running table if not exist */
{
char sql[512] = 
"create table running ("
"  pid int(10) unsigned,"
"  submission int(10) unsigned,"
"  startTime int(10),"
"  commandLine varchar(255)"
")";

if (sqlTableExists(conn,"running"))
    return;

sqlUpdate(conn, sql);

}

char *getProcCmdLine(int pid)
/* get the commandline of the process by pid - linux specific */
    // alternate method:  ps --format cmd --no-heading -p 32481
{
char procPath[256];
safef(procPath, sizeof(procPath), "/proc/%d/cmdline", pid);
FILE *f = fopen(procPath, "r");
char *procLine = readLine(f);
carefulClose(&f);
char *c = procLine;
while(TRUE)
    {
    if (*c++ == 0)
	{
	if (*c == 0)
	    {
	    c[-1] = 0;
	    break;
	    }
	c[-1] = ' ';
	}
    }
return procLine;
}

void parse(char *buf, char **args, int maxArgs)
/* split the command in buf into individual arguments. */
{
char **limit = args + maxArgs;
while (*buf != 0) 
    {
    if (args >= limit)
	{ 
    	errAbort("Exceeded max. number of args %d!", maxArgs);
    	}
    /* Strip whitespace.  Use nulls, so
     * that the previous argument is terminated
     * automatically.  */
    while ((*buf == ' ') || (*buf == '\t'))
	*buf++ = 0;
    /* Save the argument */
    *args++ = buf;
    /* Skip over the argument */
    while ((*buf != 0) && (*buf != ' ') && (*buf != '\t'))
	buf++;
    }
*args = NULL;
}

void getRunningData(struct sqlConnection *conn, int submission,
 int *pid, int *startTime, char **commandLine)
/* return data from submission record */
{
char query[256];
safef(query,sizeof(query),"select * from running"
    " where submission = '%d'", 
    submission);
struct sqlResult *rs;
char **row = NULL;
rs = sqlGetResult(conn, query);
if ((row=sqlNextRow(rs)))
    {
    *pid = sqlUnsigned(row[0]);
    //submission = sqlUnsigned(row[1]);
    *startTime  = sqlUnsigned(row[2]);
    *commandLine = cloneString(row[3]);
    }
else
    {
    errAbort("submission %d not found!\n", submission);
    }
sqlFreeResult(&rs);
}

char *getPathToLoadErrorFile(struct sqlConnection *conn, int submission)
/* return path to load_error file */
{
char query[256];
int pid = 0;
int startTime = 0;
char *commandLine = NULL;
char submissionPath[256];
getRunningData(conn, submission, &pid, &startTime, &commandLine);
safef(query, sizeof(query), "select user_id from submissions where id=%d", 
    submission);
int user = sqlQuickNum(conn, query);
safef(submissionPath, sizeof(submissionPath), "%s/%d/%d/load_error", dir, user, submission);
return cloneString(submissionPath);
}

void updateLoadErrorFile(struct sqlConnection *conn, int submission, char *message)
/* remove a process from running, update submission load_error file */
{
char *submissionPath = getPathToLoadErrorFile(conn, submission);

//uglyf("\n  submssionPath=%s\n", submissionPath);

FILE *f = fopen(submissionPath,"w");
fprintf(f, "%s", message);
carefulClose(&f);

}

void removeProcess(struct sqlConnection *conn, int submission, int status)
/* remove a process from running, update submission status */
{
char query[256];

char *loadStatus = NULL;
if (status==0)
    loadStatus = "loaded";
else
    loadStatus = "load failed";
safef(query, sizeof(query), "delete from running where submission=%d", 
    submission);
sqlUpdate(conn, query);
safef(query, sizeof(query), "update submissions set status = '%s' where id=%d", 
    loadStatus, submission);
sqlUpdate(conn, query);

}


void startBackgroundProcess(char *commandLine, int submission)
/* start background process 
 *  return pid of child or -1 for failure
 *  make it a child of "init" so we won't leave zombies
 */
{
int xpid = fork();
if ( xpid < 0 )
    { 
    errAbort("Cannot fork!");
    }
if (xpid > 0)
    return;  /* return to start any remaining ready jobs */

signal(SIGCLD, SIG_DFL);  /* will be waiting for child */
int pid = fork();
if ( pid < 0 )
    { 
    errAbort("Cannot fork!");
    }
if ( pid == 0 )
    {
    /* Child process */ 
    char *args[64];
    parse(commandLine, args, sizeof(args));
    execvp(*args, args);  
    perror(*args);  /* only get here if exec fails */
    exit(1);
    } 
else
    {
    /* Parent process */ 
    struct sqlConnection *conn = sqlConnect(db);
    char query[256];
    time_t now = time(NULL);
    safef(query, sizeof(query), "insert into running values (%d, %d, %d, '%s')", 
	pid, submission, (int)now, commandLine);
    sqlUpdate(conn, query);
    safef(query, sizeof(query), "update submissions set status = 'loading' where id=%d", 
	submission);
    sqlUpdate(conn, query);
    updateLoadErrorFile(conn, submission, ""); // clear out load_error file
    sqlDisconnect(&conn);
    /* wait for child to finish */
    int status;
    while (wait(&status) != pid)
	/* do nothing */ ;
    conn = sqlConnect(db);
    removeProcess(conn, submission, status);
    sqlDisconnect(&conn);
    exit(0);
    }
}


void hgEncodeScheduler()
/* do encode pipeline scheduling and cleanup */
{
char query[256];
char commandLine[256];
int readyId = 0;
int timeOut = 1;   // debug restore: 60;  // minutes

signal(SIGCLD, SIG_IGN);  /* won't be waiting for child */

struct sqlConnection *conn = sqlConnect(db);

initRunning(conn);

// note would we ever want to check "uptime" to see if the system
//  had just been rebooted so that all jobs could be restarted automatically or
//  at least removed from the running table?

struct slInt *e, *list = NULL;
safef(query,sizeof(query),"select count(*) from running");
if (sqlQuickNum(conn,query) < 10) // only allow max 10 loaders at once 
    {
    /* start loading any submissions that are ready */
    safef(query,sizeof(query),"select id from submissions where status = 'schedule loading'");
    struct sqlResult *rs;
    char **row = NULL;
    rs = sqlGetResult(conn, query);
    while((row=sqlNextRow(rs)))
	{
    	int submission = sqlUnsigned(row[0]);
	// note: consider paranoid check for job already in "running" with the submissionid?
	slAddHead(&list, slIntNew(submission));
	}
    sqlFreeResult(&rs);
    }

sqlDisconnect(&conn);  /* don't want the child to inherit an open connection */
for(e=list; e; e = e->next)
    {
    readyId = e->val;
    uglyf("ready submission id: %d\n", readyId);

    // run the child
    safef(commandLine, sizeof(commandLine), "/bin/sleep 130");

    startBackgroundProcess(commandLine, readyId);

    if (e->next)
    	sleep(1);
    }
conn = sqlConnect(db);

/* scan for old jobs that have timed out */
safef(query,sizeof(query),"select submission from running"
    " where unix_timestamp() - startTime > %d", 
    timeOut * 60);
int submission = 0;
while((submission = sqlQuickNum(conn,query)))
    {
    uglyf("found timed-out submission id : %d\n", submission);

    int pid = 0;
    int startTime = 0;
    char *commandLine = NULL;
    getRunningData(conn, submission, &pid, &startTime, &commandLine);

    time_t now = time(NULL);
    int age = ((int) now - startTime)/60;
    char message[256];
    safef(message, sizeof(message), "load timed out age = %d minutes\n", age);
    // save status in result file in build area
    updateLoadErrorFile(conn, submission, message);

    /* kill old job timedout, set status in db */

    char *procLine = getProcCmdLine(pid);
    //uglyf("procLine=[%s]\n", procLine);
    if (sameString(procLine, commandLine))
	kill( pid, SIGKILL ); 

    // killing the process is enough by itself normally
      // but what if the parent has died for some reason,
      // we would not want to leave orphan records around 
      // indefinitely.  Doing a redundant removeProcess() is harmless.
    removeProcess(conn, submission, 1);

    // TODO are there any times when automatic retry should be done? up to what limit?
       // note we could add a "retries" column to the running table?

    // TODO email when problems
    // TODO logging
    

    }

sqlDisconnect(&conn);

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
//xx = optionExists("xx");
db = argv[1];
dir = argv[2];
hgEncodeScheduler();
return 0;
}
