/* rbTest - Test rb trees. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rbTest - Test rb trees\n"
  "usage:\n"
  "   rbTest XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void rbTest(char *XXX)
/* rbTest - Test rb trees. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
rbTest(argv[1]);
return 0;
}
