/* burstClient - A udp client that sends a burst of data to the server. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "burstClient - A udp client that sends a burst of data to the server\n"
  "usage:\n"
  "   burstClient XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void burstClient(char *XXX)
/* burstClient - A udp client that sends a burst of data to the server. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
burstClient(argv[1]);
return 0;
}
