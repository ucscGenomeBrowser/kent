/* netStats - Gather statistics on net. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netStats - Gather statistics on net\n"
  "usage:\n"
  "   netStats XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void netStats(char *XXX)
/* netStats - Gather statistics on net. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
netStats(argv[1]);
return 0;
}
