/* motifSig - Combine info from multiple control runs and main improbizer run. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "motifSig - Combine info from multiple control runs and main improbizer run\n"
  "usage:\n"
  "   motifSig XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void motifSig(char *XXX)
/* motifSig - Combine info from multiple control runs and main improbizer run. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
motifSig(argv[1]);
return 0;
}
