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
  "    paraStat machineList\n"
  "options:\n"
  "    -long - List details of current and recent jobs\n");
}

void listJobsErr(char *name, int n)
/*Report list jobs error. */
{
warn("%s: listJobs bad reponse %d", name, n);
}

void showLong(char *name, int sd, int *pRunning, int *pRecent)
/* Fetch and display response to listJobs message.
 * Increment running and recent counts. */
{
char *line;
int running, recent, i;
if ((line = netGetLongString(sd)) == NULL)
    {
    warn("%s: no listJobs response", name);
    return;
    }
running = atoi(line);
freez(&line);
for (i=0; i<running; ++i)
    {
    line = netGetLongString(sd);
    if (line == NULL)
        {
	listJobsErr(name, 1);
	return;
	}
    printf("%s %s %s\n", name, "running", line);
    freez(&line);
    }
if ((line = netGetLongString(sd)) == NULL)
    {
    listJobsErr(name, 2);
    return;
    }
recent = atoi(line);
freez(&line);
for (i=0; i<recent; ++i)
    {
    line = netGetLongString(sd);
    if (line == NULL)
        {
	listJobsErr(name, 3);
	return;
	}
    printf("%s %s %s\n", name, "recent", line);
    freez(&line);
    line = netGetLongString(sd);
    if (line == NULL)
        {
	listJobsErr(name, 4);
	return;
	}
    printf("%s %s %s\n", name, "result", line);
    freez(&line);
    }
printf("%s summary %d running %d recent\n", name, running, recent);
printf("\n");
*pRunning += running;
*pRecent += recent;
}

void paraNodeStatus(char *machineList)
/* paraNodeStatus - Check status of paraNode on a list of machines. */
{
struct lineFile *lf = lineFileOpen(machineList, FALSE);
boolean longFormat = optionExists("long");
int sd;
char statCmd[256];
char status[256];
int size;
char *row[1];
int totalCpu = 0, totalBusy = 0, totalRecent = 0;

while (lineFileRow(lf, row))
    {
    char *name = row[0];
    if ((sd = netConnect(name, paraPort)) >= 0)
	{
	if (longFormat)
	    {
	    mustSendWithSig(sd, "listJobs");
	    showLong(name, sd, &totalBusy, &totalRecent);
	    }
	else
	    {
	    mustSendWithSig(sd, "status");
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
	    }
	close(sd);
	}
    else
	{
	printf("%s - no node server\n", name);
	}
    }
if (longFormat)
    printf("%d running, %d recent\n", totalBusy, totalRecent);
else
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
