/* subsetRef - Make BED track that is a subset of reference sequence. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "subsetRef - Make BED track that is a subset of reference sequence\n"
  "usage:\n"
  "   subsetRef XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void subsetRef(char *XXX)
/* subsetRef - Make BED track that is a subset of reference sequence. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
subsetRef(argv[1]);
return 0;
}
