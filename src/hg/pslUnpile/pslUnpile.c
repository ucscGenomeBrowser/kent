/* pslUnpile - Removes huge piles of alignments from sorted psl files (due to unmasked repeats presumably).. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslUnpile - Removes huge piles of alignments from sorted psl files (due to unmasked repeats presumably).\n"
  "usage:\n"
  "   pslUnpile XXX\n");
}

void pslUnpile(char *XXX)
/* pslUnpile - Removes huge piles of alignments from sorted psl files (due to unmasked repeats presumably).. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
pslUnpile(argv[1]);
return 0;
}
