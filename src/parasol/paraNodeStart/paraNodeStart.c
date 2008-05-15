/* paraNodeStart - Start up parasol node daemons on a list of machines. */
#include "paraCommon.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "options.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"logFacility", OPTION_STRING},
    {"log", OPTION_STRING},
    {"hub", OPTION_STRING},
    {"umask", OPTION_INT},
    {"userPath", OPTION_STRING},
    {"sysPath", OPTION_STRING},
    {"randomDelay", OPTION_INT},
    {"exe", OPTION_STRING},
    {"rsh", OPTION_STRING},
    {NULL, 0}
};

char *version = PARA_VERSION;   /* Version number. */

void usage()
/* Explain usage and exit. */
{
errAbort(
 "paraNodeStart - version %s\n"
 "Start up parasol node daemons on a list of machines.\n"
 "usage:\n"
 "    paraNodeStart machineList\n"
 "where machineList is a file containing a list of hosts.\n"
 "Machine list contains the following columns:\n"
 "     <name> <number of cpus>\n"
 "It may have other columns as well.\n"
 "options:\n"
 "    -exe=/path/to/paraNode\n"
 "    -logFacility=facility  Log to the specified syslog facility - default local0.\n"
 "    -log=file  Log to file instead of syslog.\n"
 "    -umask=000  Set umask to run under - default 002.\n"
 "    -randomDelay=N  Set random start delay in milliseconds - default 5000.\n"
 "    -userPath=bin:bin/i386  User dirs to add to path.\n"
 "    -sysPath=/sbin:/local/bin  System dirs to add to path.\n"
 "    -hub=machineHostingParaHub  Nodes will ignore messages from elsewhere.\n"
 "    -rsh=/path/to/rsh/like/command.\n"
 , version
 );
}

void carryOption(char *option, struct dyString *dy)
/* Carry option from our command line to paraNode's. */
{
char *val = optionVal(option, NULL);
if (val != NULL)
   dyStringPrintf(dy, " %s=%s", option, val);
}

void paraNodeStart(char *machineList)
/* Start node servers on all machines in list. */
{
char *exe = optionVal("exe", "paraNode");
char *rsh = optionVal("rsh", "rsh");
struct lineFile *lf = lineFileOpen(machineList, TRUE);
char *row[2];
struct dyString *dy = newDyString(256);

while (lineFileRow(lf, row))
    {
    char *name = row[0];
    int cpu = atoi(row[1]);
    if (cpu <= 0)
        errAbort("Expecting cpu count in second column, line %d of %s\n",
		lf->lineIx, lf->fileName);
    dyStringClear(dy);
    dyStringPrintf(dy, "%s %s %s start -cpu=%d", rsh, name, exe, cpu);
    carryOption("logFacility", dy);
    carryOption("log", dy);
    carryOption("hub", dy);
    carryOption("umask", dy);
    carryOption("sysPath", dy);
    carryOption("userPath", dy);
    carryOption("randomDelay", dy);
    printf("%s\n", dy->string);
    system(dy->string);
    }
lineFileClose(&lf);
freeDyString(&dy);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 2)
    usage();
paraNodeStart(argv[1]);
return 0;
}


