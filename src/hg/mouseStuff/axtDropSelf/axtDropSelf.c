/* axtDropSelf - Drop alignments that just align same thing to itself. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtDropSelf - Drop alignments that just align same thing to itself\n"
  "usage:\n"
  "   axtDropSelf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtDropSelf(char *XXX)
/* axtDropSelf - Drop alignments that just align same thing to itself. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtDropSelf(argv[1]);
return 0;
}
