/* faTrans - Translate DNA .fa file to peptide. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faTrans - Translate DNA .fa file to peptide\n"
  "usage:\n"
  "   faTrans XXX\n");
}

void faTrans(char *XXX)
/* faTrans - Translate DNA .fa file to peptide. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
faTrans(argv[1]);
return 0;
}
