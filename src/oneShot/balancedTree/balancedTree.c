/* balancedTree - Balanced trees with fast insert/deletion. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "balancedTree - Balanced trees with fast insert/deletion\n"
  "usage:\n"
  "   balancedTree XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void balancedTree(char *XXX)
/* balancedTree - Balanced trees with fast insert/deletion. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
balancedTree(argv[1]);
return 0;
}
