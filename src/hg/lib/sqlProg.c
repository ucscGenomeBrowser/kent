/* sqlProg - functions for building command line programs to deal with
 * sql databases.*/
#include "common.h"
#include "sqlProg.h"
#include "hgConfig.h"

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

void sqlExecProg(char *prog, char **progArgs, int userArgc, char *userArgv[])
/* Exec one of the sql programs using user and password from ~/.hg.conf.
 * progArgs is NULL-terminate array of program-specific arguments to add,
 * which maybe NULL. userArgv are arguments passed in from the command line.
 * The program is execvp-ed, this function does not return. */
{
int i, j = 0, nargc=cntArgv(progArgs)+userArgc+6;
char **nargv, passArg[128], hostArg[128];
safef(passArg, sizeof(passArg), "-p%s", cfgVal("db.password"));
safef(hostArg, sizeof(hostArg), "-h%s", cfgVal("db.host"));
AllocArray(nargv, nargc);

nargv[j++] = prog;
nargv[j++] = "-u";
nargv[j++] = cfgVal("db.user");
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

