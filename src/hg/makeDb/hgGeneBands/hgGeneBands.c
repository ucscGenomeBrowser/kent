/* hgGeneBands - Make geneBands tab-delimited file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGeneBands - Make geneBands tab-delimited file\n"
  "usage:\n"
  "   hgGeneBands XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgGeneBands(char *XXX)
/* hgGeneBands - Make geneBands tab-delimited file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
hgGeneBands(argv[1]);
return 0;
}
