/* axtSplitByTarget - Split a single axt file into one file per target. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtSplitByTarget - Split a single axt file into one file per target\n"
  "usage:\n"
  "   axtSplitByTarget XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtSplitByTarget(char *XXX)
/* axtSplitByTarget - Split a single axt file into one file per target. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtSplitByTarget(argv[1]);
return 0;
}
