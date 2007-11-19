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

static char const rcsid[] = "$Id: hgEncodeScheduler.c,v 1.5 2007/11/19 23:21:43 galt Exp $";

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
"  jobType varchar(255),"
"  startTime int(10),"
"  timeOut int(10),"
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

void getSubmissionTypeData(struct sqlConnection *conn, int submission,
 char **validator, char **typeParams, int *timeOut)
/* return data from submission_type record */
{
char query[256];
safef(query,sizeof(query),"select t.validator, t.type_params, t.time_out"
    " from submissions s, submission_types t"
    " where s.submission_type_id = t.id" 
    " and s.id = '%d'", 
    submission);
struct sqlResult *rs;
char **row = NULL;
rs = sqlGetResult(conn, query);
if ((row=sqlNextRow(rs)))
    {
    *validator = cloneString(row[0]);
    *typeParams = cloneString(row[1]);
    *timeOut = sqlUnsigned(row[2]);
    }
else
    {
    errAbort("submission %d not found!\n", submission);
    }
sqlFreeResult(&rs);
}

void getRunningData(struct sqlConnection *conn, int submission,
 int *pid, char **jobType, int *startTime, int *timeOut, char **commandLine)
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
    *jobType = cloneString(row[2]);
    *startTime = sqlUnsigned(row[3]);
    *timeOut = sqlUnsigned(row[4]);
    *commandLine = cloneString(row[5]);
    }
else
    {
    errAbort("submission %d not found!\n", submission);
    }
sqlFreeResult(&rs);
}

char *getPathToErrorFile(struct sqlConnection *conn, int submission, char *jobType)
/* return path to load_error or validate_error file */
{
char query[256];
safef(query, sizeof(query), "select user_id from submissions where id=%d", 
    submission);
int user = sqlQuickNum(conn, query);
char submissionPath[256];
safef(submissionPath, sizeof(submissionPath), "%s/%d/%d/%s_error", dir, user, submission, jobType);
return cloneString(submissionPath);
}

void updateErrorFile(struct sqlConnection *conn, int submission, char *jobType, char *message)
/* remove a process from running, update submission load_error or validate_error file */
{
char *submissionPath = getPathToErrorFile(conn, submission, jobType);

//uglyf("\n  submssionPath=%s\n", submissionPath);

FILE *f = fopen(submissionPath,"w");
fprintf(f, "%s", message);
carefulClose(&f);

}

void removeProcess(struct sqlConnection *conn, int submission, char *jobType, int status, char *extra)
/* remove a process from running, update submission status */
{
char query[256];

char *jobStatus = NULL;
if (status==0)
    {
    if (sameString(jobType,"load"))
	jobStatus = "loaded";
    else if (sameString(jobType,"validate"))
	jobStatus = "validated";
    else if (sameString(jobType,"upload"))
	{
	char temp[256];
	safef(temp, sizeof(temp), "schedule expanding %s", extra);
	jobStatus = cloneString(temp);
	}
    else
	errAbort("unexpected jobType=[%s]", jobType);
    }
else
    {
    if (sameString(jobType,"load"))
	jobStatus = "load failed";
    else if (sameString(jobType,"validate"))
	jobStatus = "invalid";
    else if (sameString(jobType,"upload"))
	jobStatus = "upload failed";
    else
	errAbort("unexpected jobType=[%s]", jobType);
    }
safef(query, sizeof(query), "delete from running where submission=%d", 
    submission);
sqlUpdate(conn, query);
safef(query, sizeof(query), "update submissions set status = '%s' where id=%d", 
    jobStatus, submission);
sqlUpdate(conn, query);

}


void startBackgroundProcess(int submission)
/* start background job process for submission */
{
int xpid = fork();
if ( xpid < 0 )
    { 
    errAbort("Cannot fork!");
    }
if (xpid > 0)
    return;  /* return to start any remaining ready jobs */


/* get job info */
struct sqlConnection *conn = sqlConnect(db);
char *jobType=NULL;
int timeOut = 60;   // debug restore: 3600;  // seconds
char commandLine[256];
char query[256];
safef(query, sizeof(query), "select status from submissions where id = %d", submission);
char *status = sqlQuickString(conn, query);
if (sameOk(status, "schedule loading"))
    jobType = "load";
else if (sameOk(status, "schedule validating"))
    jobType = "validate";
else if (startsWith("schedule uploading ", status))
    jobType = "upload";
else
    errAbort("unexpected jobType=[%s]", jobType);
char *submissionPath = getPathToErrorFile(conn, submission, jobType);
char *plainName = NULL;
if (sameString(jobType,"validate"))
    {
    char *validator;
    char *typeParams;
    char *submissionDir = cloneStringZ(submissionPath, strrchr(submissionPath,'/')-submissionPath);
     //uglyf("submissionDir=[%s]\n",submissionDir);
    getSubmissionTypeData(conn, submission, &validator, &typeParams, &timeOut);
    safef(commandLine, sizeof(commandLine), "%s %s %s", validator, typeParams, submissionDir);
     //uglyf("commandLine=[%s]\n",commandLine);
    }
else if (sameString(jobType,"load"))
    {
    safef(commandLine, sizeof(commandLine), "/bin/sleep 20");
    }
else if (sameString(jobType,"upload"))
    {
    char *url = cloneString(status+strlen("schedule uploading "));
     uglyf("url=[%s]\n",url);
    char *submissionDir = cloneStringZ(submissionPath, strrchr(submissionPath,'/')-submissionPath);
     uglyf("submissionDir=[%s]\n",submissionDir);

    char query[256];
    safef(query, sizeof(query), "select archive_count from submissions where id=%d", submission);
    int nextArchiveNo = sqlQuickNum(conn, query) + 1;

    plainName = strrchr(url,'/')+1;
    char filename[256];
    safef(filename, sizeof(filename), "%03d_%s", nextArchiveNo, plainName);
     uglyf("filename=[%s]\n",filename);

    safef(commandLine, sizeof(commandLine), "wget -nv -O %s/%s '%s'", submissionDir, filename, url);
     uglyf("commandLine=[%s]\n",commandLine);
    }
updateErrorFile(conn, submission, jobType, ""); // clear out job_error file
sqlDisconnect(&conn);

signal(SIGCLD, SIG_DFL);  /* will be waiting for child */
int pid = fork();
if ( pid < 0 )
    { 
    errAbort("Cannot fork!");
    }
if ( pid == 0 )
    {
    /* Child process */ 
    //int tmpstdin[2], tmpstdout[2],tmpstderr[2];

    //dup2(tmpstdin[0], STDIN_FILENO);
    //dup2(tmpstdout[1], STDOUT_FILENO);
    //close(tmpstdin[0]);
    //close(tmpstdout[1]);

    ///* Redirect stderr to /dev/null */
    //tmpstderr[0] = open("/dev/null", O_WRONLY | O_NOCTTY);

    int f = open(submissionPath, O_WRONLY | O_NOCTTY);
    dup2(f, STDERR_FILENO);
    close(f);

    char *args[64];
    // parse(commandLine, args, sizeof(args));
    args[0] = "/bin/sh";
    args[1] = "-c";
    args[2] = commandLine;
    args[3] = 0;
    
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
    char *jobStatus = NULL;
    if (sameString(jobType,"load"))
	jobStatus = "loading";
    else if (sameString(jobType,"validate"))
	jobStatus = "validating";
    else if (sameString(jobType,"upload"))
	jobStatus = "uploading";
    else
	errAbort("unexpected jobType=[%s]", jobType);
    safef(query, sizeof(query), "insert into running values (%d, %d, '%s', %d, %d, '%s')", 
	pid, submission, jobType, (int)now, timeOut, sqlEscapeString(commandLine));
    sqlUpdate(conn, query);
    safef(query, sizeof(query), "update submissions set status = '%s' where id=%d", 
	jobStatus, submission);
    sqlUpdate(conn, query);
    sqlDisconnect(&conn);
    /* wait for child to finish */
    int status;
    while (wait(&status) != pid)
	/* do nothing */ ;
    conn = sqlConnect(db);
    removeProcess(conn, submission, jobType, status, plainName);
    sqlDisconnect(&conn);
    exit(0);
    }
}


void hgEncodeScheduler()
/* do encode pipeline scheduling and cleanup */
{
char query[256];
int readyId = 0;

signal(SIGCLD, SIG_IGN);  /* won't be waiting for child */

struct sqlConnection *conn = sqlConnect(db);

initRunning(conn);

// note would we ever want to check "uptime" to see if the system
//  had just been rebooted so that all jobs could be restarted automatically or
//  at least removed from the running table?

struct slInt *e, *list = NULL;
safef(query,sizeof(query),"select count(*) from running");
if (sqlQuickNum(conn,query) < 10) // only allow max 10 jobs at once 
    {
    /* start loading any submissions that are ready */
    safef(query,sizeof(query),"select id from submissions"
	" where status = 'schedule validating'"
	" or status = 'schedule loading'"
	" or status like 'schedule uploading %%'"
	);
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
    startBackgroundProcess(readyId);

    if (e->next)
    	sleep(1);
    }
conn = sqlConnect(db);

/* scan for old jobs that have timed out */
safef(query,sizeof(query),"select submission from running"
    " where unix_timestamp() - startTime > timeOut");
int submission = 0;
while((submission = sqlQuickNum(conn,query)))
    {
    uglyf("found timed-out submission id : %d\n", submission);

    int pid = 0;
    char *jobType = NULL;
    int startTime = 0;
    int timeOut = 0;
    char *commandLine = NULL;
    getRunningData(conn, submission, &pid, &jobType, &startTime, &timeOut, &commandLine);

    time_t now = time(NULL);
    int age = ((int) now - startTime)/60;
    char message[256];
    safef(message, sizeof(message), "load timed out age = %d minutes\n", age);
    // save status in result file in build area
    updateErrorFile(conn, submission, jobType, message);

    /* kill old job timedout, set status in db */

    char *procLine = getProcCmdLine(pid);
    //uglyf("procLine=[%s]\n", procLine);
    if (sameString(procLine, commandLine))
	kill( pid, SIGKILL ); 

    // killing the process is enough by itself normally
      // but what if the parent has died for some reason,
      // we would not want to leave orphan records around 
      // indefinitely.  Doing a redundant removeProcess() is harmless.
    removeProcess(conn, submission, jobType, 1, NULL);

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
