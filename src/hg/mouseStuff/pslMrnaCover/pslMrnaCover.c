/* pslMrnaCover - Make histogram of coverage percentage of mRNA in psl. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslMrnaCover - Make histogram of coverage percentage of mRNA in psl\n"
  "usage:\n"
  "   pslMrnaCover XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void pslMrnaCover(char *XXX)
/* pslMrnaCover - Make histogram of coverage percentage of mRNA in psl. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
pslMrnaCover(argv[1]);
return 0;
}
