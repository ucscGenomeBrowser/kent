/* paraStat - query list of nodes for status. */
#include "common.h"
#include "obscure.h"
#include "net.h"
#include "paraLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort("paraStat - query status of parasol node servers.\n"
         "usage:\n"
	 "    paraStat machineList\n");
}


void paraStat(char *machineList)
/* Stop node server on all machines in list. */
{
int mCount;
char *mBuf, **mNames, *name;
int i;
int sd;
char statCmd[256];
char status[256];
int size;

readAllWords(machineList, &mNames, &mCount, &mBuf);
for (i=0; i<mCount; ++i)
    {
    name = mNames[i];
    if ((sd = netConnect(name, paraPort)) >= 0)
	{
	write(sd, paraSig, strlen(paraSig));
	netSendLongString(sd, "status");
	size = read(sd, status, sizeof(status)-1);
	if (size >= 0)
	    {
	    status[size] = 0;
	    printf("%s %s\n", name, status);
	    }
	else
	    {
	    printf("%s no status return\n", name);
	    }
	close(sd);
	}
    else
	{
	printf("%s - no node server\n", name);
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
paraStat(argv[1]);
return 0;
}


