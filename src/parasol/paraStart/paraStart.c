/* paraHub - parasol hub server. */
#include "common.h"
#include "obscure.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort("paraStart - start parasol hub server on host list.\n"
         "usage:\n"
	 "    paraStart machineList\n"
	 "where machineList is a file containing a list of hosts\n"
	 "options:\n"
	 "    -exe=/path/to/paranode");
}

void paraStart(char *machineList)
/* Start node servers on all machines in list. */
{
int mCount;
char *mBuf, **mNames, *name;
int i;
char buf[512];
char *exe = optionVal("exe", "paraNode");

readAllWords(machineList, &mNames, &mCount, &mBuf);
for (i=0; i<mCount; ++i)
    {
    name = mNames[i];
    uglyf("Starting up %s\n", name);
    sprintf(buf, "rsh %s %s start", name, exe);
    system(buf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
paraStart(argv[1]);
return 0;
}


