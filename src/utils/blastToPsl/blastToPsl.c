/* blastToPsl - Convert from blast to psl format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blastToPsl - Convert from blast to psl format\n"
  "usage:\n"
  "   blastToPsl XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void blastToPsl(char *XXX)
/* blastToPsl - Convert from blast to psl format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
blastToPsl(argv[1]);
return 0;
}
