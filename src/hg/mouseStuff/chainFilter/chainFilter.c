/* chainFilter - Filter chains. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainFilter - Filter chains\n"
  "usage:\n"
  "   chainFilter XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void chainFilter(char *XXX)
/* chainFilter - Filter chains. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
chainFilter(argv[1]);
return 0;
}
