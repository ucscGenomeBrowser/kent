/* sqlProg - functions for building command line programs to deal with
 * sql databases.*/

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "sqlProg.h"
#include "hgConfig.h"
#include "obscure.h"
#include "portable.h"
#include "jksql.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

char *tempFileNameToRemove = NULL;
int killChildPid = 0;

/* trap signals to remove the temporary file and terminate the child process */

static void sqlProgCatchSignal(int sigNum)
/* handler for various terminal signals for removing the temp file */
{

if (tempFileNameToRemove)
    unlink(tempFileNameToRemove);

if (killChildPid != 0) // the child should be runing
    {
    if (sigNum == SIGINT)  // control-C usually
	{
    	kill(killChildPid, SIGINT);  // sometimes redundant, but not if someone sends SIGINT directly to the parent.
	sleep(5);
	}
    kill(killChildPid, SIGTERM);
    } 

if (sigNum == SIGTERM || sigNum == SIGHUP) 
    exit(1);   // so that atexit cleanup get called

raise(SIGKILL);
}

void sqlProgInitSigHandlers()
/* set handler for various terminal signals for logging purposes.
 * if dumpStack is TRUE, attempt to dump the stack. */
{
// SIGKILL is not trappable or ignorable
signal(SIGTERM, sqlProgCatchSignal);
signal(SIGHUP,  sqlProgCatchSignal);
signal(SIGINT,  sqlProgCatchSignal);
signal(SIGABRT, sqlProgCatchSignal);
signal(SIGSEGV, sqlProgCatchSignal);
signal(SIGFPE,  sqlProgCatchSignal);
signal(SIGBUS,  sqlProgCatchSignal);
}


static int cntArgv(char **argv)
/* count number of elements in a null terminated vector of strings. 
 * NULL is considered empty */
{
if (argv == NULL)
    return 0;
else
    {
    int i;
    for (i = 0; argv[i] != NULL; i++)
        continue;
    return i;
    }
}


void copyCnfToDefaultsFile(char *path, char *defaultFileName, int fileNo)
/* write the contents of the path to the fileNo */
{
if (fileExists(path))
    {
    char *cnf = NULL;
    size_t cnfSize = 0;
    readInGulp(path, &cnf, &cnfSize);
    if (write (fileNo, cnf, cnfSize) == -1)
        {
        freeMem(cnf);
	errAbort("Writing %s to file %s failed with errno %d", path, defaultFileName, errno);
        }
    freeMem(cnf);
    }
}

int sqlMakeDefaultsFile(char* defaultFileName, char* profile, char* group, char *prog)
/* Create a temporary file in the supplied directory to be passed to
 * mysql with --defaults-file.  Writes a mysql options set for
 * the mysql group [group] with the profile.host, profile.user, and
 * profile.password values returned from cfgVal().  If group is not
 * client or mysql, a --defaults-group-suffix=group must be passed to
 * mysql to actually invoke the settings.
 * passFileName cannot be static, and the last 6 characters must
 * be XXXXXX.  Those characters will be modified to form a unique suffix.
 * Returns a file descriptor for the file or dies with an error */
{
int fileNo;
char paddedGroup [256]; /* string with brackets around the group name */
char path[1024];

if ((fileNo=mkstemp(defaultFileName)) == -1)
    errAbort("Could not create unique temporary file %s", defaultFileName);
tempFileNameToRemove = defaultFileName;

/* pick up the global and local defaults
 *  -- the order matters: later settings over-ride earlier ones. */

safef(path, sizeof path, "/etc/my.cnf");
copyCnfToDefaultsFile(path, defaultFileName, fileNo);

safef(path, sizeof path, "/etc/mysql/my.cnf");
copyCnfToDefaultsFile(path, defaultFileName, fileNo);

safef(path, sizeof path, "/var/lib/mysql/my.cnf");
copyCnfToDefaultsFile(path, defaultFileName, fileNo);

//  SYSCONFDIR/my.cnf should be next, but I do not think it matters. maybe it is just /var/lib/mysql anyways.

safef(path, sizeof path, "%s/my.cnf", getenv("MYSQL_HOME"));
copyCnfToDefaultsFile(path, defaultFileName, fileNo);

safef(path, sizeof path, "%s/.my.cnf", getenv("HOME"));
copyCnfToDefaultsFile(path, defaultFileName, fileNo);

/* write out the group name, user, host, and password */
safef(paddedGroup, sizeof(paddedGroup), "[%s]\n", group);
if (write (fileNo, paddedGroup, strlen(paddedGroup)) == -1)
    errAbort("Writing group to temporary file %s failed with errno %d", defaultFileName, errno);

char *settings = sqlProfileToMyCnf(profile);
if (!settings)
    errAbort("profile %s not found in sqlProfileToMyCnf() -- failed for file %s failed with errno %d", profile, defaultFileName, errno);
if (sameString(prog, "mysqldump"))
    {  // need to suppress the database setting, it messes up mysqldump and is not needed. comment it out
    settings = replaceChars(settings, "\ndatabase=", "\n#database=");
    }
if (write (fileNo, settings, strlen(settings)) == -1)
    errAbort("Writing profile %s settings=[%s] as my.cnf format failed for file %s failed with errno %d", profile, settings, defaultFileName, errno);


return fileNo;
}


void sqlExecProg(char *prog, char **progArgs, int userArgc, char *userArgv[])
/* Exec one of the sql programs using user and password from ~/.hg.conf.
 * progArgs is NULL-terminate array of program-specific arguments to add,
 * which maybe NULL. userArgv are arguments passed in from the command line.
 * The program is execvp-ed, this function does not return. */
{
sqlExecProgProfile(getDefaultProfileName(), prog, progArgs, userArgc, userArgv);
}


void sqlExecProgLocal(char *prog, char **progArgs, int userArgc, char *userArgv[])
/* 
 * Exec one of the sql programs using user and password defined in localDb.XXX variables from ~/.hg.conf 
 * progArgs is NULL-terminate array of program-specific arguments to add,
 * which maybe NULL. userArgv are arguments passed in from the command line.
 * The program is execvp-ed, this function does not return. 
 */
{
sqlExecProgProfile("localDb", prog, progArgs, userArgc, userArgv);
}


void nukeOldCnfs(char *homeDir)
/* Remove .hgsql.cnf-* files older than a month */
{
// ignoreStatFailures must be TRUE due to race condition between two hgsql
// commands might trying to purge the same file.
struct fileInfo *file, *fileList = listDirXExt(homeDir, ".hgsql.cnf-*", FALSE, TRUE);
time_t now = time(0);
for (file = fileList; file != NULL; file = file->next)
    {
    if ((file->statErrno == 0) && (difftime(now, file->lastAccess) >  30 * 24 * 60 * 60))  // 30 days in seconds.
	{
	char homePath[256];
	safef(homePath, sizeof homePath, "%s/%s", homeDir, file->name);
	remove(homePath);
	}
    }
slFreeList(&fileList);
}

void sqlExecProgProfile(char *profile, char *prog, char **progArgs, int userArgc, char *userArgv[])
/* 
 * Exec one of the sql programs using user and password defined in localDb.XXX variables from ~/.hg.conf 
 * progArgs is NULL-terminate array of program-specific arguments to add,
 * which maybe NULL. userArgv are arguments passed in from the command line.
 * The program is execvp-ed, this function does not return. 
 */
{
int i, j = 0, nargc=cntArgv(progArgs)+userArgc+6, status;
pid_t childId;
char **nargv, defaultFileName[256], defaultFileArg[256], *homeDir;

// install cleanup signal handlers
sqlProgInitSigHandlers();

/* Assemble defaults file */
if ((homeDir = getenv("HOME")) == NULL)
    errAbort("sqlExecProgProfile: HOME is not defined in environment; cannot create temporary password file");

nukeOldCnfs(homeDir);
// look for special parameter -profile=name
for (i = 0; i < userArgc; i++)
    if (startsWith("-profile=", userArgv[i]))
	profile=cloneString(userArgv[i]+strlen("-profile="));

safef(defaultFileName, sizeof(defaultFileName), "%s/.hgsql.cnf-XXXXXX", homeDir);
// discard returned fileNo
(void) sqlMakeDefaultsFile(defaultFileName, profile, "client", prog);

safef(defaultFileArg, sizeof(defaultFileArg), "--defaults-file=%s", defaultFileName);

AllocArray(nargv, nargc);

nargv[j++] = prog;
nargv[j++] = defaultFileArg;   /* --defaults-file must come before other options */
if (progArgs != NULL)
    {
    for (i = 0; progArgs[i] != NULL; i++)
        nargv[j++] = progArgs[i];
    }
for (i = 0; i < userArgc; i++)
    if (!startsWith("-profile=", userArgv[i]))
	nargv[j++] = userArgv[i];
nargv[j++] = NULL;

// flush before forking so we can't accidentally get two copies of the output
fflush(stdout);
fflush(stderr);

childId = fork();
killChildPid = childId;
if (childId == 0)
    {
    execvp(nargv[0], nargv);
    _exit(42);  /* Why 42?  Why not?  Need something user defined that mysql isn't going to return */
    }
else
    {
    /* Wait until the child process completes, then delete the temp file */

    pid_t endId = waitpid(childId, &status, 0);
    if (endId == -1)             /* error calling waitpid       */
	{
	errAbort("waitpid error");
	exit(1);
	}
    else if (endId == childId)   /* child ended                 */
	{
	if (WIFEXITED(status))
	    {
	    // Child ended normally.
	    unlink (defaultFileName);
	    int childExitStatus = WEXITSTATUS(status);
	    if (childExitStatus == 42)
		errAbort("sqlExecProgProfile: exec failed");
	    else
		{
		// Propagate child's exit status:
		_exit(childExitStatus);
		}
	    }
	else if (WIFSIGNALED(status))
	    errAbort("Child mysql process ended because of an uncaught signal.n");
	else if (WIFSTOPPED(status))
	    errAbort("Child mysql process has stopped.n");
	else
	    errAbort("sqlExecProgProfile: child mysql process exited with abnormal status %d", status);
	}
    else if (endId == 0)         /* child still running should not happen */
	{
	errAbort("Unexpected error, child still running");
	}
    }
}
