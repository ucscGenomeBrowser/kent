/* chainNet - Make alignment nets out of chains. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainNet - Make alignment nets out of chains\n"
  "usage:\n"
  "   chainNet XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void chainNet(char *XXX)
/* chainNet - Make alignment nets out of chains. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
chainNet(argv[1]);
return 0;
}
