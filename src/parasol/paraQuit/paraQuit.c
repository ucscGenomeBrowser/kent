/* paraQuit - tell list of node servers to stand down. */
#include "common.h"
#include "net.h"
#include "linefile.h"
#include "paraLib.h"

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
int sd;
struct lineFile *lf = lineFileOpen(machineList, FALSE);
char *row[1];

while (lineFileRow(lf, row))
    {
    char *name = row[0];
    printf("Telling %s to quit \n", name);
    if ((sd = netConnect(name, paraPort)) >= 0)
	{
	write(sd, paraSig, strlen(paraSig));
	netSendLongString(sd, "quit");
	close(sd);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
paraQuit(argv[1]);
return 0;
}


