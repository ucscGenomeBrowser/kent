/* dnaMotifFind - Locate preexisting motifs in DNA sequence. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dnaMotifFind - Locate preexisting motifs in DNA sequence\n"
  "usage:\n"
  "   dnaMotifFind XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void dnaMotifFind(char *XXX)
/* dnaMotifFind - Locate preexisting motifs in DNA sequence. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
dnaMotifFind(argv[1]);
return 0;
}
