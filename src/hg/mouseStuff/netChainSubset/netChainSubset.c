/* netChainSubset - Create chain file with subset of chains that appear in the net. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netChainSubset - Create chain file with subset of chains that appear in the net\n"
  "usage:\n"
  "   netChainSubset XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void netChainSubset(char *XXX)
/* netChainSubset - Create chain file with subset of chains that appear in the net. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
netChainSubset(argv[1]);
return 0;
}
