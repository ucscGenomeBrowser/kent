/* paraHub - parasol hub server. */
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "common.h"
#include "dlist.h"
#include "dystring.h"
#include "linefile.h"
#include "paraLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort("paraStart - start parasol hub server on host list.\n"
         "usage:\n"
	 "    paraStart machineList\n"
	 "where machineList is a file containing a list of hosts\n");
}

void paraStart(char *machineList)
/* Start node servers on all machines in list. */
{
int mCount;
char *mBuf, **mNames, *name;
int i;

readAllWords(machineList, &mNames, &mCount, &mBuf);
for (i=0; i<mCount; ++i)
    {
    name = mNames[i];
    uglyf("Starting up %s\n", name);
    if (fork() == 0)
	{
	execlp("rsh", name,
		"/projects/compbiousr/kent/html/parasol/paraNode/paraNode", 
		"start", NULL);
	}
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
paraStart(argv[1]);
}


