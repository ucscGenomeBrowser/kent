/* netSplit - Split a genome net file into chromosome net files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netSplit - Split a genome net file into chromosome net files\n"
  "usage:\n"
  "   netSplit XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void netSplit(char *XXX)
/* netSplit - Split a genome net file into chromosome net files. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
netSplit(argv[1]);
return 0;
}
