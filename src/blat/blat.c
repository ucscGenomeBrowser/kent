/* blat - Standalone BLAT fast sequence search command line tool. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blat - Standalone BLAT fast sequence search command line tool\n"
  "usage:\n"
  "   blat XXX\n");
}

void blat(char *XXX)
/* blat - Standalone BLAT fast sequence search command line tool. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
blat(argv[1]);
return 0;
}
