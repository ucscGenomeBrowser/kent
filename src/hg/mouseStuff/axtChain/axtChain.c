/* axtChain - Chain together axt alignments.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtChain - Chain together axt alignments.\n"
  "usage:\n"
  "   axtChain XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtChain(char *XXX)
/* axtChain - Chain together axt alignments.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtChain(argv[1]);
return 0;
}
