/* axtInvert - Swap source and query in an axt file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtInvert - Swap source and query in an axt file\n"
  "usage:\n"
  "   axtInvert XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtInvert(char *XXX)
/* axtInvert - Swap source and query in an axt file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtInvert(argv[1]);
return 0;
}
