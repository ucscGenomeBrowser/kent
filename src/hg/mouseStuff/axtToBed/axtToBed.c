/* axtToBed - Convert axt alignments to simple bed format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtToBed - Convert axt alignments to simple bed format\n"
  "usage:\n"
  "   axtToBed XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtToBed(char *XXX)
/* axtToBed - Convert axt alignments to simple bed format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtToBed(argv[1]);
return 0;
}
