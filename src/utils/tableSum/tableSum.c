/* tableSum - Summarize a table somehow. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tableSum - Summarize a table somehow\n"
  "usage:\n"
  "   tableSum XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void tableSum(char *XXX)
/* tableSum - Summarize a table somehow. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
tableSum(argv[1]);
return 0;
}
