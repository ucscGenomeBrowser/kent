/* dnaMotifToTable - Convert dnaMotif to a simple table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dnaMotifToTable - Convert dnaMotif to a simple table\n"
  "usage:\n"
  "   dnaMotifToTable XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void dnaMotifToTable(char *XXX)
/* dnaMotifToTable - Convert dnaMotif to a simple table. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
dnaMotifToTable(argv[1]);
return 0;
}
