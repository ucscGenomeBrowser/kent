/* ave - Compute average and basic stats. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ave - Compute average and basic stats\n"
  "usage:\n"
  "   ave XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void ave(char *XXX)
/* ave - Compute average and basic stats. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
ave(argv[1]);
return 0;
}
