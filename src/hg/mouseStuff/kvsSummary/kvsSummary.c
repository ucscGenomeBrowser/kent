/* kvsSummary - Summarize output of a bunch of knownVsBlats. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kvsSummary - Summarize output of a bunch of knownVsBlats\n"
  "usage:\n"
  "   kvsSummary XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void kvsSummary(char *XXX)
/* kvsSummary - Summarize output of a bunch of knownVsBlats. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
kvsSummary(argv[1]);
return 0;
}
