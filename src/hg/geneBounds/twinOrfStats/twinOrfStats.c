/* twinOrfStats - Collect stats on refSeq cDNAs aligned to another species via axtForEst. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "twinOrfStats - Collect stats on refSeq cDNAs aligned to another species via axtForEst\n"
  "usage:\n"
  "   twinOrfStats XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void twinOrfStats(char *XXX)
/* twinOrfStats - Collect stats on refSeq cDNAs aligned to another species via axtForEst. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
twinOrfStats(argv[1]);
return 0;
}
