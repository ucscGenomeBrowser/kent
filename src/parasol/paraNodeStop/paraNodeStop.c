/* paraNodeStop - Shut down parasol node daemons on a list of machines. */
#include "paraCommon.h"
#include "linefile.h"
#include "hash.h"
#include "net.h"
#include "paraLib.h"
#include "rudp.h"
#include "paraMessage.h"

char *version = PARA_VERSION;   /* Version number. */

void usage()
/* Explain usage and exit. */
{
errAbort(
    "paraNodeStop - version %s\n"
    "Shut down parasol node daemons on a list of machines.\n"
    "usage:\n"
    "    paraNodeStop machineList\n"
    ,version
    );
}

void paraNodeStop(char *machineList)
/* Stop node server on all machines in list. */
{
struct lineFile *lf = lineFileOpen(machineList, FALSE);
char *row[1];

while (lineFileRow(lf, row))
    {
    struct rudp *ru = rudpMustOpen();
    char *name = row[0];
    struct paraMessage pm;
    ru->maxRetries = 6;
    printf("Telling %s to quit \n", name);
    pmInitFromName(&pm, name, paraNodePort);
    pmPrintf(&pm, "%s", "quit");
    pmSend(&pm, ru);
    rudpClose(&ru);
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


