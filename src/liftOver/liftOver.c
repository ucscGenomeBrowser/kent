/* liftOver - Move annotations from one assembly to another. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftOver - Move annotations from one assembly to another\n"
  "usage:\n"
  "   liftOver XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void liftOver(char *XXX)
/* liftOver - Move annotations from one assembly to another. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
liftOver(argv[1]);
return 0;
}
