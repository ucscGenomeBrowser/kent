/* paraStat - query list of nodes for status. */
#include "common.h"
#include "linefile.h"
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
struct lineFile *lf = lineFileOpen(machineList, FALSE);
int sd;
char statCmd[256];
char status[256];
int size;
char *row[1];

while (lineFileRow(lf, row))
    {
    char *name = row[0];
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
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
paraStat(argv[1]);
return 0;
}


