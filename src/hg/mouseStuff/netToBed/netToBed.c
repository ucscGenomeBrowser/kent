/* netToBed - Convert target coverage of net to a bed file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netToBed - Convert target coverage of net to a bed file.\n"
  "usage:\n"
  "   netToBed XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void netToBed(char *XXX)
/* netToBed - Convert target coverage of net to a bed file.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
netToBed(argv[1]);
return 0;
}
