/* trfBig - Mask tandem repeats on a big sequence file.. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trfBig - Mask tandem repeats on a big sequence file.\n"
  "usage:\n"
  "   trfBig XXX\n");
}

void trfBig(char *XXX)
/* trfBig - Mask tandem repeats on a big sequence file.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
trfBig(argv[1]);
return 0;
}
