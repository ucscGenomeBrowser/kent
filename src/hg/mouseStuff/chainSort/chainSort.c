/* chainSort - Sort chains. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainSort - Sort chains\n"
  "usage:\n"
  "   chainSort XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void chainSort(char *XXX)
/* chainSort - Sort chains. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
chainSort(argv[1]);
return 0;
}
