/* axtBest - Remove second best alignments. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtBest - Remove second best alignments\n"
  "usage:\n"
  "   axtBest XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtBest(char *XXX)
/* axtBest - Remove second best alignments. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtBest(argv[1]);
return 0;
}
