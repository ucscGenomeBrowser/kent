/* chainPreNet - Remove chains that don't have a chance of being netted. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainPreNet - Remove chains that don't have a chance of being netted\n"
  "usage:\n"
  "   chainPreNet XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void chainPreNet(char *XXX)
/* chainPreNet - Remove chains that don't have a chance of being netted. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
chainPreNet(argv[1]);
return 0;
}
