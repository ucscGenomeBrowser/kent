/* paraHubStop - Shut down paraHub daemon. */
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
  "paraHubStop - Shut down paraHub daemon\n"
  "usage:\n"
  "   paraHubStop now\n");
}

void paraHubStop(char *now)
/* paraHubStop - Shut down paraHub daemon. */
{
int hubFd = netMustConnect(getHost(), paraPort);
sendWithSig(hubFd, "quit");
close(hubFd);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
paraHubStop(argv[1]);
return 0;
}
