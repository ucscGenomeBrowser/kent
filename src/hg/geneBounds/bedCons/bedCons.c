/* bedCons - Look at conservation of a BED track vs. a refence (nonredundant) alignment track. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedCons - Look at conservation of a BED track vs. a refence (nonredundant) alignment track\n"
  "usage:\n"
  "   bedCons XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void bedCons(char *XXX)
/* bedCons - Look at conservation of a BED track vs. a refence (nonredundant) alignment track. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
bedCons(argv[1]);
return 0;
}
