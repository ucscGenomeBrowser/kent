/* dnsInfo - Get info from DNS about a machine. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dnsInfo - Get info from DNS about a machine\n"
  "usage:\n"
  "   dnsInfo XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void dnsInfo(char *XXX)
/* dnsInfo - Get info from DNS about a machine. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
dnsInfo(argv[1]);
return 0;
}
