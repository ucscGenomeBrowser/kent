/* burstServer - A udp server that accepts a burst of data, queues it, and prints it out slowly. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "burstServer - A udp server that accepts a burst of data, queues it, and prints it out slowly\n"
  "usage:\n"
  "   burstServer XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void burstServer(char *XXX)
/* burstServer - A udp server that accepts a burst of data, queues it, and prints it out slowly. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
burstServer(argv[1]);
return 0;
}
