/* geneBand - Find bands for all genes. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "geneBand - Find bands for all genes\n"
  "usage:\n"
  "   geneBand XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void geneBand(char *XXX)
/* geneBand - Find bands for all genes. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
geneBand(argv[1]);
return 0;
}
