/* subsetAlx - Rescore alignments and output those over threshold. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "subsetAlx - Rescore alignments and output those over threshold\n"
  "usage:\n"
  "   subsetAlx XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void subsetAlx(char *XXX)
/* subsetAlx - Rescore alignments and output those over threshold. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
subsetAlx(argv[1]);
return 0;
}
