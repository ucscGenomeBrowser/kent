/* featureBits - Correlate tables via bitmap projections and booleans. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "featureBits - Correlate tables via bitmap projections and booleans\n"
  "usage:\n"
  "   featureBits XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void featureBits(char *XXX)
/* featureBits - Correlate tables via bitmap projections and booleans. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
featureBits(argv[1]);
return 0;
}
