/* trimFosmids - trim fosmid ends. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trimFosmids - trim fosmid ends\n"
  "usage:\n"
  "   trimFosmids XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void trimFosmids(char *XXX)
/* trimFosmids - trim fosmid ends. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
trimFosmids(argv[1]);
return 0;
}
