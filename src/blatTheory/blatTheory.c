/* blatTheory - Calculate some theoretical blat statistics. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blatTheory - Calculate some theoretical blat statistics\n"
  "usage:\n"
  "   blatTheory XXX\n");
}

void blatTheory(char *XXX)
/* blatTheory - Calculate some theoretical blat statistics. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
blatTheory(argv[1]);
return 0;
}
