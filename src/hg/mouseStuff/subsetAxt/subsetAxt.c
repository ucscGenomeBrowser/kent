/* subsetAxt - Rescore alignments and output those over threshold. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "subsetAxt - Rescore alignments and output those over threshold\n"
  "usage:\n"
  "   subsetAxt XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void subsetAxt(char *XXX)
/* subsetAxt - Rescore alignments and output those over threshold. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
subsetAxt(argv[1]);
return 0;
}
