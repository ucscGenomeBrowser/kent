/* pslCat - concatenate psl files. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslCat - concatenate psl files\n"
  "usage:\n"
  "   pslCat XXX\n");
}

void pslCat(char *XXX)
/* pslCat - concatenate psl files. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
pslCat(argv[1]);
return 0;
}
