/* chainSwap - Swap target and query in chain. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainSwap - Swap target and query in chain\n"
  "usage:\n"
  "   chainSwap XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void chainSwap(char *XXX)
/* chainSwap - Swap target and query in chain. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
chainSwap(argv[1]);
return 0;
}
