/* iriToControlTable - Convert improbizer run to simple list of control scores. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "iriToControlTable - Convert improbizer run to simple list of control scores\n"
  "usage:\n"
  "   iriToControlTable XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void iriToControlTable(char *XXX)
/* iriToControlTable - Convert improbizer run to simple list of control scores. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
iriToControlTable(argv[1]);
return 0;
}
