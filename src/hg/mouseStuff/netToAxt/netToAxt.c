/* netToAxt - Convert net (and chain) to axt.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netToAxt - Convert net (and chain) to axt.\n"
  "usage:\n"
  "   netToAxt XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void netToAxt(char *XXX)
/* netToAxt - Convert net (and chain) to axt.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
netToAxt(argv[1]);
return 0;
}
