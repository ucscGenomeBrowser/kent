/* mafFilter - Filter out maf files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafFilter - Filter out maf files\n"
  "usage:\n"
  "   mafFilter XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void mafFilter(char *XXX)
/* mafFilter - Filter out maf files. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
mafFilter(argv[1]);
return 0;
}
