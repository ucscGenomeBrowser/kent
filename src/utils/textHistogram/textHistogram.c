/* textHistogram - Make a histogram in ascii. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "textHistogram - Make a histogram in ascii\n"
  "usage:\n"
  "   textHistogram XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void textHistogram(char *XXX)
/* textHistogram - Make a histogram in ascii. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
textHistogram(argv[1]);
return 0;
}
