/* axtToPsl - Convert axt to psl format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtToPsl - Convert axt to psl format\n"
  "usage:\n"
  "   axtToPsl XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtToPsl(char *XXX)
/* axtToPsl - Convert axt to psl format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtToPsl(argv[1]);
return 0;
}
