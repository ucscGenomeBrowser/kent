/* chainSplit - Split chains up by target or query sequence. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainSplit - Split chains up by target or query sequence\n"
  "usage:\n"
  "   chainSplit XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void chainSplit(char *XXX)
/* chainSplit - Split chains up by target or query sequence. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
chainSplit(argv[1]);
return 0;
}
