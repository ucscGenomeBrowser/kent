/* paraNodeStop - Shut down parasol node daemons on a list of machines. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "net.h"
#include "paraLib.h"
#include "rudp.h"
#include "paraMessage.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
    "paraNodeStop - Shut down parasol node daemons on a list of machines\n"
    "usage:\n"
    "    paraNodeStop machineList\n");
}

void paraNodeStop(char *machineList)
/* Stop node server on all machines in list. */
{
struct lineFile *lf = lineFileOpen(machineList, FALSE);
char *row[1];
struct rudp *ru = rudpMustOpen();

while (lineFileRow(lf, row))
    {
    char *name = row[0];
    struct paraMessage pm;
    printf("Telling %s to quit \n", name);
    pmInitFromName(&pm, name, paraNodePort);
    pmPrintf(&pm, "%s", "quit");
    pmSend(&pm, ru);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
paraNodeStop(argv[1]);
return 0;
}


