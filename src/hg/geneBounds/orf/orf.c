/* orf - Find orf for cDNAs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "orf - Find orf for cDNAs\n"
  "usage:\n"
  "   orf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void orf(char *XXX)
/* orf - Find orf for cDNAs. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
orf(argv[1]);
return 0;
}
