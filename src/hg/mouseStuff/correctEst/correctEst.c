/* correctEst - Correct ESTs by passing them through genome. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "correctEst - Correct ESTs by passing them through genome\n"
  "usage:\n"
  "   correctEst XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void correctEst(char *XXX)
/* correctEst - Correct ESTs by passing them through genome. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
correctEst(argv[1]);
return 0;
}
