/* catDir - concatenate files in directory - for those times when too many files for cat to handle.. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "catDir - concatenate files in directory - for those times when too many files for cat to handle.\n"
  "usage:\n"
  "   catDir XXX\n");
}

void catDir(char *XXX)
/* catDir - concatenate files in directory - for those times when too many files for cat to handle.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
catDir(argv[1]);
return 0;
}
