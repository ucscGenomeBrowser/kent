/* orfStats - Collect stats on orfs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "orfStats - Collect stats on orfs\n"
  "usage:\n"
  "   orfStats XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void orfStats(char *XXX)
/* orfStats - Collect stats on orfs. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
orfStats(argv[1]);
return 0;
}
