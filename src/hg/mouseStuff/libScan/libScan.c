/* libScan - Scan for G capped libraries. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "libScan - Scan for G capped libraries\n"
  "usage:\n"
  "   libScan XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void libScan(char *XXX)
/* libScan - Scan for G capped libraries. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
libScan(argv[1]);
return 0;
}
