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
#include "cheapcgi.h"

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
    execlp("rsh", name,
	    "/projects/cc/hg/jk/bin/i386/paraNode", 
	    "start", NULL);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
paraStart(argv[1]);
}


