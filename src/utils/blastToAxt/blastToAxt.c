/* blastToAxt - Convert from blast to axt format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blastToAxt - Convert from blast to axt format\n"
  "usage:\n"
  "   blastToAxt XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void blastToAxt(char *XXX)
/* blastToAxt - Convert from blast to axt format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
blastToAxt(argv[1]);
return 0;
}
