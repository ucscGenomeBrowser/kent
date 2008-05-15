/* paraNodeStatus - Check status of paraNode on a list of machines. */
#include "paraCommon.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "net.h"
#include "internet.h"
#include "paraLib.h"
#include "rudp.h"
#include "paraMessage.h"

char *version = PARA_VERSION;   /* Version number. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "paraNodeStatus - version %s\n"
  "Check status of paraNode on a list of machines.\n"
  "usage:\n"
  "    paraNodeStatus machineList\n"
  "options:\n"
  "    -retries=N  Number of retries to get in touch with machine.\n"
  "        The first retry is after 1/100th of a second. \n"
  "        Each retry after that takes twice as long up to a maximum\n"
  "        of 1 second per retry.  Default is 7 retries and takes\n"
  "        about a second.\n"
  "    -long  List details of current and recent jobs.\n"
  , version
  );
}

void listJobsErr(char *name, int n)
/*Report list jobs error. */
{
warn("%s: listJobs bad reponse %d", name, n);
}

void showLong(char *name, struct rudp *ru, int *pRunning, int *pRecent)
/* Fetch and display response to listJobs message.
 * Increment running and recent counts. */
{
int running, recent, i;
struct paraMessage pm;

if (!pmReceive(&pm, ru))
    {
    warn("%s: no listJobs response", name);
    return;
    }
running = atoi(pm.data);
for (i=0; i<running; ++i)
    {
    if (!pmReceive(&pm, ru))
        {
	listJobsErr(name, 1);
	return;
	}
    printf("%s %s %s\n", name, "running", pm.data);
    }
if (!pmReceive(&pm, ru))
    {
    listJobsErr(name, 2);
    return;
    }
recent = atoi(pm.data);
for (i=0; i<recent; ++i)
    {
    if (!pmReceive(&pm, ru))
        {
	listJobsErr(name, 3);
	return;
	}
    printf("%s %s %s\n", name, "recent", pm.data);
    if (!pmReceive(&pm, ru))
        {
	listJobsErr(name, 4);
	return;
	}
    printf("%s %s %s\n", name, "result", pm.data);
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
char *row[1];
int totalCpu = 0, totalBusy = 0, totalRecent = 0;

while (lineFileRow(lf, row))
    {
    char *name = row[0];
    struct paraMessage pm;
    struct rudp *ru = rudpMustOpen();

    if (optionExists("retries"))
        ru->maxRetries = optionInt("retries", 7);
    pmInitFromName(&pm, name, paraNodePort);
    if (longFormat)
	{
	pmPrintf(&pm, "%s", "listJobs");
	if (pmSend(&pm, ru))
	    showLong(name, ru, &totalBusy, &totalRecent);
	}
    else
	{
	pmPrintf(&pm, "%s", "status");
	if (pmSend(&pm, ru))
	    {
	    if (pmReceive(&pm, ru))
		{
		char *row[3];
		printf("%s %s\n", name, pm.data);
		chopLine(pm.data, row);
		totalBusy += atoi(row[0]);
		if (!sameString(row[1], "of"))
		    errAbort("paraNode status message format changed");
		totalCpu += atoi(row[2]);
		}
	    else
		{
		printf("%s no status return: %s\n", name, strerror(errno));
		}
	    }
	else
	    {
	    printf("%s unreachable\n", name);
	    }
	}
    rudpClose(&ru);
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
