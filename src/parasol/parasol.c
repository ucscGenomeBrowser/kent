/* parasol - paraTestJob A good test job to run on Parasol.  Can be configured to take a long time or crash. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "parasol - paraTestJob A good test job to run on Parasol.  Can be configured to take a long time or crash\n"
  "usage:\n"
  "   parasol XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void parasol(char *XXX)
/* parasol - paraTestJob A good test job to run on Parasol.  Can be configured to take a long time or crash. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
parasol(argv[1]);
return 0;
}
