/* sqlProg - functions for building command line programs to deal with
 * sql databases.*/
#include "common.h"
#include "sqlProg.h"
#include "hgConfig.h"
#include "obscure.h"
#include <sys/types.h>
#include <sys/wait.h>

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
	errAbort("Writing %s to file %s failed with errno %d", path, defaultFileName, errno);
    }
}


int sqlMakeDefaultsFile(char* defaultFileName, char* profile, char* group)
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
char fileData[256];  /* constructed variable=value data for the mysql config file */
char field[256];  /* constructed profile.field name to pass to cfgVal */
char path[1024];

if ((fileNo=mkstemp(defaultFileName)) == -1)
    errAbort("Could not create unique temporary file %s", defaultFileName);

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

safef(field, sizeof(field), "%s.host", profile);
safef(fileData, sizeof(fileData), "host=%s\n", cfgVal(field));
if (write (fileNo, fileData, strlen(fileData)) == -1)
    errAbort("Writing host to temporary file %s failed with errno %d", defaultFileName, errno);

safef(field, sizeof(field), "%s.user", profile);
safef(fileData, sizeof(fileData), "user=%s\n", cfgVal(field));
if (write (fileNo, fileData, strlen(fileData)) == -1)
    errAbort("Writing user to temporary file %s failed with errno %d", defaultFileName, errno);

safef(field, sizeof(field), "%s.password", profile);
safef(fileData, sizeof(fileData), "password=%s\n", cfgVal(field));
if (write (fileNo, fileData, strlen(fileData)) == -1)
    errAbort("Writing password to temporary file %s failed with errno %d", defaultFileName, errno);

return fileNo;
}


void sqlExecProg(char *prog, char **progArgs, int userArgc, char *userArgv[])
/* Exec one of the sql programs using user and password from ~/.hg.conf.
 * progArgs is NULL-terminate array of program-specific arguments to add,
 * which maybe NULL. userArgv are arguments passed in from the command line.
 * The program is execvp-ed, this function does not return. */
{
sqlExecProgProfile("db", prog, progArgs, userArgc, userArgv);
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

void sqlExecProgProfile(char *profile, char *prog, char **progArgs, int userArgc, char *userArgv[])
/* 
 * Exec one of the sql programs using user and password defined in localDb.XXX variables from ~/.hg.conf 
 * progArgs is NULL-terminate array of program-specific arguments to add,
 * which maybe NULL. userArgv are arguments passed in from the command line.
 * The program is execvp-ed, this function does not return. 
 */
{
int i, j = 0, nargc=cntArgv(progArgs)+userArgc+6, defaultFileNo, returnStatus;
pid_t child_id;
char **nargv, defaultFileName[256], defaultFileArg[256], *homeDir;

/* Assemble defaults file */
if ((homeDir = getenv("HOME")) == NULL)
    errAbort("sqlExecProgProfile: HOME is not defined in environment; cannot create temporary password file");
safef(defaultFileName, sizeof(defaultFileName), "%s/.hgsql.cnf-XXXXXX", homeDir);
defaultFileNo=sqlMakeDefaultsFile(defaultFileName, profile, "client");
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
    nargv[j++] = userArgv[i];
nargv[j++] = NULL;

child_id = fork();
if (child_id == 0)
    {
    execvp(nargv[0], nargv);
    _exit(42);  /* Why 42?  Why not?  Need something user defined that mysql isn't going to return */
    }
else
    {
    /* Wait until the child process completes, then delete the temp file */
    wait(&returnStatus);
    unlink (defaultFileName);
    if (WIFEXITED(returnStatus))
        {
        if (WEXITSTATUS(returnStatus) == 42)
            errAbort("sqlExecProgProfile: exec failed");
        }
    else
        errAbort("sqlExecProgProfile: child process exited with abnormal status %d", returnStatus);
    }
}
