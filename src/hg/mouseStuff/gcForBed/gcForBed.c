/* gcForBed - Calculate g/c percentage and other stats for regions covered by bed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gcForBed - Calculate g/c percentage and other stats for regions covered by bed\n"
  "usage:\n"
  "   gcForBed XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void gcForBed(char *XXX)
/* gcForBed - Calculate g/c percentage and other stats for regions covered by bed. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
gcForBed(argv[1]);
return 0;
}
