/* twinOrfStats2 - Collect stats on refSeq cDNAs aligned to another species.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "twinOrfStats2 - Collect stats on refSeq cDNAs aligned to another species.\n"
  "usage:\n"
  "   twinOrfStats2 XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void twinOrfStats2(char *XXX)
/* twinOrfStats2 - Collect stats on refSeq cDNAs aligned to another species.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
twinOrfStats2(argv[1]);
return 0;
}
