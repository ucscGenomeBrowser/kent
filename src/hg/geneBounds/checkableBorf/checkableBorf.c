/* checkableBorf - Convert borfBig orf-finder output to checkable form. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkableBorf - Convert borfBig orf-finder output to checkable form\n"
  "usage:\n"
  "   checkableBorf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void checkableBorf(char *XXX)
/* checkableBorf - Convert borfBig orf-finder output to checkable form. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
checkableBorf(argv[1]);
return 0;
}
