/* chainMergeSort - Combine sorted files into larger sorted file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainMergeSort - Combine sorted files into larger sorted file\n"
  "usage:\n"
  "   chainMergeSort XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void chainMergeSort(char *XXX)
/* chainMergeSort - Combine sorted files into larger sorted file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
chainMergeSort(argv[1]);
return 0;
}
