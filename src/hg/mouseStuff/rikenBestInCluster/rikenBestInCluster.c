/* rikenBestInCluster - Find best looking in Riken cluster. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rikenBestInCluster - Find best looking in Riken cluster\n"
  "usage:\n"
  "   rikenBestInCluster XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void rikenBestInCluster(char *XXX)
/* rikenBestInCluster - Find best looking in Riken cluster. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
rikenBestInCluster(argv[1]);
return 0;
}
