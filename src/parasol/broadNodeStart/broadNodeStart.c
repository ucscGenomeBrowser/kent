/* broadNodeStart - Start up broadcast node daemons on a list of machines. */
#include "paraCommon.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "broadNodeStart - Start up broadcast node daemons on a list of machines\n"
  "usage:\n"
  "   broadNodeStart machineList\n"
  "where machineList is a file containing a list of hosts\n"
  "with one line per host.  The first word in the line is the\n"
  "host name or ip address (ip address is much quicker)\n"
  "Options:\n"
  "    exe=/path/to/broadNode\n"
  "    rsh=/path/to/rsh/like/command\n"
  );
}

void broadNodeStart(char *machineList)
/* broadNodeStart - Start up broadcast node daemons on a list of machines. */
{
int i;
char *exe = optionVal("exe", "broadNode");
char *rsh = optionVal("rsh", "rsh");
struct lineFile *lf = lineFileOpen(machineList, TRUE);
char *row[1];
struct dyString *dy = newDyString(256);

while (lineFileRow(lf, row))
    {
    char *name = row[0];
    dyStringClear(dy);
    dyStringPrintf(dy, "%s %s %s start -ip=%s", rsh, name, exe, name);
    printf("%s\n", dy->string);
    system(dy->string);
    }
lineFileClose(&lf);
freeDyString(&dy);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
broadNodeStart(argv[1]);
return 0;
}
