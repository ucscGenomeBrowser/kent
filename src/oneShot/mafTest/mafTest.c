/* mafTest - Testing out maf stuff. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafTest - Testing out maf stuff\n"
  "usage:\n"
  "   mafTest XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void mafTest(char *XXX)
/* mafTest - Testing out maf stuff. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
mafTest(argv[1]);
return 0;
}
