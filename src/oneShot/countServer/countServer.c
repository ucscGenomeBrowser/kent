/* countServer - A server that just returns a steadily increasing stream of numbers. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "countServer - A server that just returns a steadily increasing stream of numbers\n"
  "usage:\n"
  "   countServer XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void countServer(char *XXX)
/* countServer - A server that just returns a steadily increasing stream of numbers. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
countServer(argv[1]);
return 0;
}
