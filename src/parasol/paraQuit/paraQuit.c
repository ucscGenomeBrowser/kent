/* paraQuit - tell list of node servers to stand down. */
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "common.h"
#include "dlist.h"
#include "dystring.h"
#include "linefile.h"
#include "paraLib.h"
#include "net.h"

void usage()
/* Explain usage and exit. */
{
errAbort("paraQuit - kill parasol node servers.\n"
         "usage:\n"
	 "    paraQuit machineList\n");
}

void paraQuit(char *machineList)
/* Stop node server on all machines in list. */
{
int mCount;
char *mBuf, **mNames, *name;
int i;
int sd;
char buf[256];

readAllWords(machineList, &mNames, &mCount, &mBuf);
for (i=0; i<mCount; ++i)
    {
    name = mNames[i];
    uglyf("Telling %s to quit \n", name);
    if ((sd = netConnPort(name, paraPort)) >= 0)
	{
	write(sd, paraSig, strlen(paraSig));
	netSendLongString(sd, "quit");
	close(sd);
	}
    }
freeMem(mNames);
freeMem(mBuf);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
paraQuit(argv[1]);
}


