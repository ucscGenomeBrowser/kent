/* gensub2 - Generate condor submission file from template and two file lists. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gensub2 - Generate condor submission file from template and two file lists\n"
  "usage:\n"
  "   gensub2 XXX\n");
}

void gensub2(char *XXX)
/* gensub2 - Generate condor submission file from template and two file lists. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
gensub2(argv[1]);
return 0;
}
