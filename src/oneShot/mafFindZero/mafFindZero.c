/* mafFindZero - Find small mafs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafFindZero - Find small mafs\n"
  "usage:\n"
  "   mafFindZero XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void mafFindZero(char *XXX)
/* mafFindZero - Find small mafs. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
mafFindZero(argv[1]);
return 0;
}
