/* iriToDnaMotif - Convert improbRunInfo to dnaMotif. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "iriToDnaMotif - Convert improbRunInfo to dnaMotif\n"
  "usage:\n"
  "   iriToDnaMotif XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void iriToDnaMotif(char *XXX)
/* iriToDnaMotif - Convert improbRunInfo to dnaMotif. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
iriToDnaMotif(argv[1]);
return 0;
}
