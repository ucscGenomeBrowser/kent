/* paraHubStop - Shut down paraHub daemon. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "paraHubStop - Shut down paraHub daemon\n"
  "usage:\n"
  "   paraHubStop XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void paraHubStop(char *XXX)
/* paraHubStop - Shut down paraHub daemon. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
paraHubStop(argv[1]);
return 0;
}
