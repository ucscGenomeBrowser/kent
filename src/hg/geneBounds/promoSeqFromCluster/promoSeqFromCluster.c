/* promoSeqFromCluster - Get promoter regions from cluster. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "promoSeqFromCluster - Get promoter regions from cluster\n"
  "usage:\n"
  "   promoSeqFromCluster XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void promoSeqFromCluster(char *XXX)
/* promoSeqFromCluster - Get promoter regions from cluster. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
promoSeqFromCluster(argv[1]);
return 0;
}
