/* subsetTraces - Build subset of mouse traces that actually align. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "subsetTraces - Build subset of mouse traces that actually align\n"
  "usage:\n"
  "   subsetTraces XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void subsetTraces(char *XXX)
/* subsetTraces - Build subset of mouse traces that actually align. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
subsetTraces(argv[1]);
return 0;
}
