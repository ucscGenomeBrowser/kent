/* lineCount - Count lines in a file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "lineCount - Count lines in a file\n"
  "usage:\n"
  "   lineCount XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void lineCount(char *XXX)
/* lineCount - Count lines in a file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
lineCount(argv[1]);
return 0;
}
