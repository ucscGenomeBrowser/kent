/* checkSeed - Check seeds are good. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkSeed - Check seeds are good\n"
  "usage:\n"
  "   checkSeed XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void checkSeed(char *XXX)
/* checkSeed - Check seeds are good. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
checkSeed(argv[1]);
return 0;
}
