/* randomLines - Pick out random lines from file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "randomLines - Pick out random lines from file\n"
  "usage:\n"
  "   randomLines XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void randomLines(char *XXX)
/* randomLines - Pick out random lines from file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
randomLines(argv[1]);
return 0;
}
