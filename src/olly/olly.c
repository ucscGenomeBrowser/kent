/* olly - Look for matches and near matches to short sequences genome-wide. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "olly - Look for matches and near matches to short sequences genome-wide\n"
  "usage:\n"
  "   olly XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void olly(char *XXX)
/* olly - Look for matches and near matches to short sequences genome-wide. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
olly(argv[1]);
return 0;
}
