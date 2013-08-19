/* sqlProg - functions for building command line programs to deal with
 * sql databases.*/
#include "common.h"
#include "sqlProg.h"
#include "hgConfig.h"
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


int sqlMakePassFile(char* passFileName)
/* Create a password file in the supplied directory to be passed to
 * mysql with --defaults-extra-file.  Writes a mysql options set using
 * the profile [client] and the password from cfgVal("db.password").
 * passFileName cannot be static, and the last 6 characters should
 * be XXXXXX.  Those characters will be modified to form a unique suffix.
 * Returns the file descriptor of the password file */
/* This should later be modified to accept a profile argument, to select
 * which profile the password (and other options?) comes from */
{
int passFileNo;
char passFileData[256];

if ((passFileNo=mkstemp(passFileName)) == -1)
    errAbort("Could not create unique temporary file %s", passFileName);
safef(passFileData, sizeof(passFileData), "[client]\npassword=%s", cfgVal("db.password"));
if (write (passFileNo, passFileData, strlen(passFileData)) == -1)
    errAbort("Writing to temporary file %s failed with errno %d", passFileName, errno);
return passFileNo;
}


void sqlExecProg(char *prog, char **progArgs, int userArgc, char *userArgv[])
/* Exec one of the sql programs using user and password from ~/.hg.conf.
 * progArgs is NULL-terminate array of program-specific arguments to add,
 * which maybe NULL. userArgv are arguments passed in from the command line.
 * The program is execvp-ed, this function does not return. */
{
int i, j = 0, nargc=cntArgv(progArgs)+userArgc+6, passFileNo;
pid_t child_id;
char **nargv, passArg[256], hostArg[128], *homeDir, passFileName[256];

/* Assemble defaults file */
if ((homeDir = getenv("HOME")) == NULL)
    errAbort("HOME is not defined in environment; cannot create temporary password file");
safef(passFileName, sizeof(passFileName), "%s/.hgsql.cnfXXXXXX", homeDir);
passFileNo=sqlMakePassFile(passFileName);
safef(passArg, sizeof(passArg), "--defaults-extra-file=%s", passFileName);

safef(hostArg, sizeof(hostArg), "-h%s", cfgVal("db.host"));
AllocArray(nargv, nargc);

nargv[j++] = prog;
/* --defaults-extra-file must come before other options */
nargv[j++] = passArg;
nargv[j++] = "-u";
nargv[j++] = cfgVal("db.user");
nargv[j++] = hostArg;
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
    errnoAbort("exec of %s failed with errno %d", nargv[0], errno);
}
else
{
    /* Wait until the called process completes, then delete the temp file */
    wait(NULL);
    unlink (passFileName);
}
}


void sqlExecProgLocal(char *prog, char **progArgs, int userArgc, char *userArgv[])
/* 
 * Exec one of the sql programs using user and password defined in localDb.XXX variables from ~/.hg.conf 
 * progArgs is NULL-terminate array of program-specific arguments to add,
 * which maybe NULL. userArgv are arguments passed in from the command line.
 * The program is execvp-ed, this function does not return. 
 */
{
int i, j = 0, nargc=cntArgv(progArgs)+userArgc+6;
char **nargv, passArg[128], hostArg[128];
safef(passArg, sizeof(passArg), "-p%s", cfgOption("localDb.password"));
safef(hostArg, sizeof(hostArg), "-h%s", cfgOption("localDb.host"));
AllocArray(nargv, nargc);

nargv[j++] = prog;
nargv[j++] = "-u";
nargv[j++] = cfgOption("localDb.user");
nargv[j++] = passArg;
nargv[j++] = hostArg;
if (progArgs != NULL)
    {
    for (i = 0; progArgs[i] != NULL; i++)
        nargv[j++] = progArgs[i];
    }
for (i = 0; i < userArgc; i++)
    nargv[j++] = userArgv[i];
nargv[j++] = NULL;
execvp(nargv[0], nargv);
errnoAbort("exec of %s failed", nargv[0]);
}

void sqlExecProgProfile(char *profile, char *prog, char **progArgs, int userArgc, char *userArgv[])
/* 
 * Exec one of the sql programs using user and password defined in localDb.XXX variables from ~/.hg.conf 
 * progArgs is NULL-terminate array of program-specific arguments to add,
 * which maybe NULL. userArgv are arguments passed in from the command line.
 * The program is execvp-ed, this function does not return. 
 */
{
int i, j = 0, nargc=cntArgv(progArgs)+userArgc+6;
char **nargv, passArg[128], hostArg[128], optionStr[128];
safef(optionStr, sizeof(optionStr), "%s.password", profile);
safef(passArg, sizeof(passArg), "-p%s", cfgOption(optionStr));
safef(optionStr, sizeof(optionStr), "%s.host", profile);
safef(hostArg, sizeof(hostArg), "-h%s", cfgOption(optionStr));
AllocArray(nargv, nargc);

nargv[j++] = prog;
nargv[j++] = "-u";
safef(optionStr, sizeof(optionStr), "%s.user", profile);
nargv[j++] = cfgOption(optionStr);
nargv[j++] = passArg;
nargv[j++] = hostArg;
if (progArgs != NULL)
    {
    for (i = 0; progArgs[i] != NULL; i++)
        nargv[j++] = progArgs[i];
    }
for (i = 0; i < userArgc; i++)
    nargv[j++] = userArgv[i];
nargv[j++] = NULL;
execvp(nargv[0], nargv);
errnoAbort("exec of %s failed", nargv[0]);
}
