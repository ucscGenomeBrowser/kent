/* netClass - Add classification info to net. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netClass - Add classification info to net\n"
  "usage:\n"
  "   netClass XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void netClass(char *XXX)
/* netClass - Add classification info to net. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
netClass(argv[1]);
return 0;
}
