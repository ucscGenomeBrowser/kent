/* oneShot - rbTest Test rb trees. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "oneShot - rbTest Test rb trees\n"
  "usage:\n"
  "   oneShot XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void oneShot(char *XXX)
/* oneShot - rbTest Test rb trees. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
oneShot(argv[1]);
return 0;
}
