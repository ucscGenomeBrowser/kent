/* blat2p - blat two proteins. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blat2p - blat two proteins\n"
  "usage:\n"
  "   blat2p XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void blat2p(char *XXX)
/* blat2p - blat two proteins. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
blat2p(argv[1]);
return 0;
}
