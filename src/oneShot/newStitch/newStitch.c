/* newStitch - Experiments with new ways of stitching together alignments. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "newStitch - Experiments with new ways of stitching together alignments\n"
  "usage:\n"
  "   newStitch XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void newStitch(char *XXX)
/* newStitch - Experiments with new ways of stitching together alignments. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
newStitch(argv[1]);
return 0;
}
