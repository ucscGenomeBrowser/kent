/* netFilter - Filter out parts of net.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netFilter - Filter out parts of net.\n"
  "usage:\n"
  "   netFilter XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void netFilter(char *XXX)
/* netFilter - Filter out parts of net.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
netFilter(argv[1]);
return 0;
}
