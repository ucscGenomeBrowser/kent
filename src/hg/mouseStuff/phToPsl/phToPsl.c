/* phToPsl - Convert from Pattern Hunter to PSL format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "phToPsl - Convert from Pattern Hunter to PSL format\n"
  "usage:\n"
  "   phToPsl XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void phToPsl(char *XXX)
/* phToPsl - Convert from Pattern Hunter to PSL format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
phToPsl(argv[1]);
return 0;
}
