/* paraNodeStatus - Check status of paraNode on a list of machines. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "net.h"
#include "paraLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "paraNodeStatus - Check status of paraNode on a list of machines\n"
  "usage:\n"
  "    paraStat machineList\n");
}

void paraNodeStatus(char *machineList)
/* paraNodeStatus - Check status of paraNode on a list of machines. */
{
struct lineFile *lf = lineFileOpen(machineList, FALSE);
int sd;
char statCmd[256];
char status[256];
int size;
char *row[1];
int totalCpu = 0, totalBusy = 0;

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
	    char *row[3];
	    status[size] = 0;
	    printf("%s %s\n", name, status);
	    chopLine(status, row);
	    totalBusy += atoi(row[0]);
	    if (!sameString(row[1], "of"))
	        errAbort("paraNode status message format changed");
	    totalCpu += atoi(row[2]);
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
printf("%d of %d CPUs busy total\n", totalBusy, totalCpu);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
paraNodeStatus(argv[1]);
return 0;
}
