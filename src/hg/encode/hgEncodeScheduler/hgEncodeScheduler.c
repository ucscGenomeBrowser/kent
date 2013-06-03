/* hgEncodeScheduler - a chron job that runs ENCODE pipeline load jobs 
 *   that are ready for scheduling.  It reads the encpipeline database
 *   looking for projects with status == "shedule loading".
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
"NOSQLINJ create table running ("
"  pid int(10) unsigned,"
"  project int(10) unsigned,"
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

void getProjectTypeData(struct sqlConnection *conn, int project,
 char **validator, char **typeParams, int *timeOut)
/* return data from project_type record */
{
char query[256];

sqlSafef(query,sizeof(query),"select t.validator, t.type_params, t.time_out"
    " from projects p, project_types t"
    " where p.project_type_id = t.id" 
    " and p.id = '%d'", 
    project);

struct sqlResult *rs;
char **row = NULL;

rs = sqlGetResult(conn, query);
if ((row=sqlNextRow(rs)))
    {
    if (row[0] == NULL) errAbort("project_types.validator cannot be NULL");
    *validator = cloneString(row[0]);
    if (row[1] == NULL) errAbort("project_types.type_params cannot be NULL");
    *typeParams = cloneString(row[1]);
    if (row[2] == NULL) errAbort("project_types.time_out cannot be NULL");
    *timeOut = sqlUnsigned(row[2]);
    }
else
    {
    errAbort("project %d not found!\n", project);
    }
sqlFreeResult(&rs);
}

void getProjectLoadData(struct sqlConnection *conn, int project,
 char **loader, char **loadParams, int *loadTimeOut)
/* return data from project_type record */
{
char query[256];

sqlSafef(query,sizeof(query),"select t.loader, t.load_params, t.load_time_out"
    " from projects p, project_types t"
    " where p.project_type_id = t.id" 
    " and p.id = '%d'", 
    project);

struct sqlResult *rs;
char **row = NULL;

rs = sqlGetResult(conn, query);
if ((row=sqlNextRow(rs)))
    {
    if (row[0] == NULL) errAbort("project_types.loader cannot be NULL");
    *loader = cloneString(row[0]);
    if (row[1] == NULL) errAbort("project_types.load_params cannot be NULL");
    *loadParams = cloneString(row[1]);
    if (row[2] == NULL) errAbort("project_types.load_time_out cannot be NULL");
    *loadTimeOut = sqlUnsigned(row[2]);
    }
else
    {
    errAbort("project %d not found!\n", project);
    }
sqlFreeResult(&rs);
}

void getProjectUnloadData(struct sqlConnection *conn, int project,
 char **unloader, char **unloadParams, int *unloadTimeOut)
/* return data from project_type record */
{
char query[256];

sqlSafef(query,sizeof(query),"select t.unloader, t.unload_params, t.unload_time_out"
    " from projects p, project_types t"
    " where p.project_type_id = t.id" 
    " and p.id = '%d'", 
    project);

struct sqlResult *rs;
char **row = NULL;

rs = sqlGetResult(conn, query);
if ((row=sqlNextRow(rs)))
    {
    if (row[0] == NULL) errAbort("project_types.unloader cannot be NULL");
    *unloader = cloneString(row[0]);
    if (row[1] == NULL) errAbort("project_types.unload_params cannot be NULL");
    *unloadParams = cloneString(row[1]);
    if (row[2] == NULL) errAbort("project_types.unload_time_out cannot be NULL");
    *unloadTimeOut = sqlUnsigned(row[2]);
    }
else
    {
    errAbort("project %d not found!\n", project);
    }
sqlFreeResult(&rs);
}

void getRunningData(struct sqlConnection *conn, int project,
 int *pid, char **jobType, int *startTime, int *timeOut, char **commandLine)
/* return data from project record */
{
char query[256];
sqlSafef(query,sizeof(query),"select * from running"
    " where project = '%d'", 
    project);
struct sqlResult *rs;
char **row = NULL;
rs = sqlGetResult(conn, query);
if ((row=sqlNextRow(rs)))
    {
    *pid = sqlUnsigned(row[0]);
    //project = sqlUnsigned(row[1]);
    *jobType = cloneString(row[2]);
    *startTime = sqlUnsigned(row[3]);
    *timeOut = sqlUnsigned(row[4]);
    *commandLine = cloneString(row[5]);
    }
else
    {
    errAbort("project %d not found!\n", project);
    }
sqlFreeResult(&rs);
}

char *getProjectPath(int project)
/* return path to project dir */
{
char projectPath[256];
safef(projectPath, sizeof(projectPath), "%s/%d", dir, project);
return cloneString(projectPath);
}

char *getPathToErrorFile(int project, char *jobType)
/* return path to load_error or validate_error file */
{
char errorPath[256];
safef(errorPath, sizeof(errorPath), "%s/%s_error", getProjectPath(project), jobType);
return cloneString(errorPath);
}

void updateErrorFile(int project, char *jobType, char *message)
/* remove a process from running, update project load_error or validate_error file */
{
char *errorPath = getPathToErrorFile(project, jobType);

//uglyf("\n  submssionPath=%s\n", errorPath);

FILE *f = mustOpen(errorPath,"w");
fprintf(f, "%s", message);
carefulClose(&f);

}

void removeProcess(struct sqlConnection *conn, int project, char *jobType, int status, char *extra)
/* remove a process from running, update project status */
{
char query[256];

char *jobStatus = NULL;
if (status==0)
    {
    if (sameString(jobType,"load"))
	jobStatus = "loaded";
    else if (sameString(jobType,"unload"))
	jobStatus = "schedule deleting";
    else if (sameString(jobType,"validate"))
	jobStatus = "validated";
    else if (sameString(jobType,"upload"))
	{
	char temp[256];
	safef(temp, sizeof(temp), "schedule expanding %s", extra);
	jobStatus = cloneString(temp);
	}
    else if (sameString(jobType,"reexpandall"))
	jobStatus = "schedule expand all";
    else
	errAbort("unexpected jobType=[%s]", jobType);
    }
else
    {
    if (sameString(jobType,"load"))
	jobStatus = "load failed";
    else if (sameString(jobType,"unload"))
	jobStatus = "unload failed";
    else if (sameString(jobType,"validate"))
	jobStatus = "validate failed";
    else if (sameString(jobType,"upload"))
	jobStatus = "upload failed";
    else if (sameString(jobType,"reexpandall"))
	jobStatus = "re-expand-all failed";
    else
	errAbort("unexpected jobType=[%s]", jobType);
    }
sqlSafef(query, sizeof(query), "delete from running where project=%d", 
    project);
sqlUpdate(conn, query);
sqlSafef(query, sizeof(query), "update projects set status = '%s' where id=%d", 
    jobStatus, project);
sqlUpdate(conn, query);
/* log new status value */
sqlSafef(query, sizeof(query), "insert into project_status_logs"
    " set project_id=%d, status = '%s', created_at=now()", 
    project, jobStatus);
sqlUpdate(conn, query);

}


void startBackgroundProcess(int project)
/* start background job process for project */
{

fflush(stdout);
fflush(stderr);

int xpid = fork();
if ( xpid < 0 )
    { 
    errAbort("Cannot fork!");
    }
if (xpid > 0)
    return;  /* return to start any remaining ready jobs */

signal(SIGCLD, SIG_DFL);  /* will be waiting for child */

/* get job info */
struct sqlConnection *conn = sqlConnect(db);
char *jobType=NULL;
int timeOut = 60;   // debug restore: 3600;  // seconds
char commandLine[256];
char query[256];
sqlSafef(query, sizeof(query), "select status from projects where id = %d", project);
char *status = sqlQuickString(conn, query);
if (sameOk(status, "schedule loading"))
    jobType = "load";
else if (sameOk(status, "schedule unloading"))
    jobType = "unload";
else if (sameOk(status, "schedule validating"))
    jobType = "validate";
else if (startsWith("schedule uploading ", status))
    jobType = "upload";
else if (sameOk(status, "schedule re-expand all"))
    jobType = "reexpandall";
else
    errAbort("unexpected jobType=[%s]", jobType);

char *errorPath = getPathToErrorFile(project, jobType);

char *projectDir = getProjectPath(project);
uglyf("projectDir=[%s]\n",projectDir);

char *plainName = NULL;
if (sameString(jobType,"validate"))
    {
    char *validator;
    char *typeParams;
    getProjectTypeData(conn, project, &validator, &typeParams, &timeOut);
    safef(commandLine, sizeof(commandLine), "%s %s %s", validator, typeParams, projectDir);
     //uglyf("commandLine=[%s]\n",commandLine);
    }
else if (sameString(jobType,"load"))
    {
    //safef(commandLine, sizeof(commandLine), "/bin/sleep 20");
    char *loader;
    char *loadParams;
    getProjectLoadData(conn, project, &loader, &loadParams, &timeOut);
    safef(commandLine, sizeof(commandLine), "%s %s %s", loader, loadParams, projectDir);
     //uglyf("commandLine=[%s]\n",commandLine);
    }
else if (sameString(jobType,"unload"))
    {
    //safef(commandLine, sizeof(commandLine), "/bin/sleep 20");
    char *unloader;
    char *unloadParams;
    getProjectUnloadData(conn, project, &unloader, &unloadParams, &timeOut);
    safef(commandLine, sizeof(commandLine), "%s %s %s", unloader, unloadParams, projectDir);
     //uglyf("commandLine=[%s]\n",commandLine);
    }
else if (sameString(jobType,"upload"))
    {
    timeOut = 300;
    char *url = cloneString(status+strlen("schedule uploading "));
     uglyf("url=[%s]\n",url);

    char query[256];
    sqlSafef(query, sizeof(query), "select archive_count from projects where id=%d", project);
    int nextArchiveNo = sqlQuickNum(conn, query) + 1;

    plainName = url; // oldway:  strrchr(url,'/')+1;
    char filename[256];
    safef(filename, sizeof(filename), "%03d_%s", nextArchiveNo, plainName);
     uglyf("filename=[%s]\n",filename);

    //safef(commandLine, sizeof(commandLine), "wget -nv -O %s/%s '%s'", projectDir, filename, url);
    safef(commandLine, sizeof(commandLine), "%s/todo", projectDir);
     uglyf("commandLine=[%s]\n",commandLine);
    }
else if (sameString(jobType,"reexpandall"))
    {
    timeOut = 300;
    safef(commandLine, sizeof(commandLine), "%s/todo", projectDir);
     uglyf("commandLine=[%s]\n",commandLine);
    }
updateErrorFile(project, jobType, ""); // clear out job_error file
sqlDisconnect(&conn);


fflush(stdout);
fflush(stderr);


int f = open(errorPath, O_WRONLY | O_NOCTTY);
dup2(f, STDERR_FILENO);
/* required to free cron job to finish immediately without waiting */
dup2(f, STDOUT_FILENO);  
close(f);

int pid = fork();
if ( pid < 0 )
    { 
    errAbort("Cannot fork!");
    }
if ( pid == 0 )
    {
    /* Child process */ 

    //uglyf("commandLine=[%s]\n",commandLine);

    char *args[64];
    // parse(commandLine, args, sizeof(args));
    args[0] = getenv("SHELL");
    args[1] = "-c";
    args[2] = commandLine;
    args[3] = 0;
    
    //execvp(*args, args);  
    execv(*args, args);  
    /* only get here if exec itself fails */
    //perror(*args);  
    errnoAbort("%s %s %s", args[0], args[1], args[2]);
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
    else if (sameString(jobType,"unload"))
	jobStatus = "unloading";
    else if (sameString(jobType,"validate"))
	jobStatus = "validating";
    else if (sameString(jobType,"upload"))
	jobStatus = "uploading/expanding";
    else if (sameString(jobType,"reexpandall"))
	jobStatus = "re-expanding all archives";
    else
	errAbort("unexpected jobType=[%s]", jobType);
    sqlSafef(query, sizeof(query), "insert into running values (%d, %d, '%s', %d, %d, '%s')", 
	pid, project, jobType, (int)now, timeOut, commandLine);
    sqlUpdate(conn, query);
    sqlSafef(query, sizeof(query), "update projects set status = '%s' where id=%d", 
	jobStatus, project);
    sqlUpdate(conn, query);
    /* log new status value */
    sqlSafef(query, sizeof(query), "insert into project_status_logs"
	" set project_id=%d, status = '%s', created_at=now()", 
    	project, jobStatus);
    sqlUpdate(conn, query);
    sqlDisconnect(&conn);
    /* wait for child to finish */
    int status;
    while (wait(&status) != pid)
	/* do nothing */ ;
    conn = sqlConnect(db);
    removeProcess(conn, project, jobType, status, plainName);
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
sqlSafef(query,sizeof(query),"select count(*) from running");
if (sqlQuickNum(conn,query) < 10) // only allow max 10 jobs at once 
    {
    /* start loading any projects that are ready */
    sqlSafef(query,sizeof(query),"select id from projects"
	" where status = 'schedule validating'"
	" or status = 'schedule loading'"
	" or status = 'schedule unloading'"
	" or status like 'schedule uploading %%'"
        " or status = 'schedule re-expand all'"
	);
    struct sqlResult *rs;
    char **row = NULL;
    rs = sqlGetResult(conn, query);
    while((row=sqlNextRow(rs)))
	{
    	int project = sqlUnsigned(row[0]);
	// note: consider paranoid check for job already in "running" with the projectid?
	e = slIntNew(project);
	slAddHead(&list, e);
	}
    sqlFreeResult(&rs);
    }

sqlDisconnect(&conn);  /* don't want the child to inherit an open connection */
for(e=list; e; e = e->next)
    {
    readyId = e->val;
    uglyf("ready project id: %d\n", readyId);
    
    /* run the child */
    startBackgroundProcess(readyId);

    if (e->next)
    	sleep(1);
    }
conn = sqlConnect(db);


/* scan for old jobs that have timed out */
sqlSafef(query,sizeof(query),"select project from running"
    " where unix_timestamp() - startTime > timeOut");
int project = 0;
while((project = sqlQuickNum(conn,query)))
    {
    uglyf("found timed-out project id : %d\n", project);

    int pid = 0;
    char *jobType = NULL;
    int startTime = 0;
    int timeOut = 0;
    char *commandLine = NULL;
    getRunningData(conn, project, &pid, &jobType, &startTime, &timeOut, &commandLine);

    time_t now = time(NULL);
    int age = ((int) now - startTime)/60;
    char message[256];
    sqlSafef(message, sizeof(message), "load timed out age = %d minutes\n", age);
    /* save status in result file in build area */
    updateErrorFile(project, jobType, message);

    /* kill old job timedout, set status in db */

    char *procLine = getProcCmdLine(pid);
    //uglyf("procLine=[%s]\n", procLine);
    if (sameString(procLine, commandLine))
	kill( pid, SIGKILL ); 

    // killing the process is enough by itself normally
      // but what if the parent has died for some reason,
      // we would not want to leave orphan records around 
      // indefinitely.  Doing a redundant removeProcess() is harmless.
    removeProcess(conn, project, jobType, 1, NULL);

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
