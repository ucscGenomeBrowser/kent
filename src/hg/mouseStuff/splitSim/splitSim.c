/* splitSim - Simulate gapless distribution size. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "splitSim - Simulate gapless distribution size\n"
  "usage:\n"
  "   splitSim XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void splitSim(char *XXX)
/* splitSim - Simulate gapless distribution size. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
splitSim(argv[1]);
return 0;
}
