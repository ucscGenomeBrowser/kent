/* fakeFinContigs - Fake up contigs for a finished chromosome. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fakeFinContigs - Fake up contigs for a finished chromosome\n"
  "usage:\n"
  "   fakeFinContigs XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  ");\n"
}

void fakeFinContigs(char *XXX)
/* fakeFinContigs - Fake up contigs for a finished chromosome. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
fakeFinContigs(argv[1]);
return 0;
}
