/* emblMatrixToMotif - Convert transfac matrix in EMBL format to dnaMotif. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "emblMatrixToMotif - Convert transfac matrix in EMBL format to dnaMotif\n"
  "usage:\n"
  "   emblMatrixToMotif XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void emblMatrixToMotif(char *XXX)
/* emblMatrixToMotif - Convert transfac matrix in EMBL format to dnaMotif. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
emblMatrixToMotif(argv[1]);
return 0;
}
