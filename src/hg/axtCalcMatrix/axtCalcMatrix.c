/* axtCalcMatrix - Calculate substitution matrix and make indel histogram. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtCalcMatrix - Calculate substitution matrix and make indel histogram\n"
  "usage:\n"
  "   axtCalcMatrix XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtCalcMatrix(char *XXX)
/* axtCalcMatrix - Calculate substitution matrix and make indel histogram. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtCalcMatrix(argv[1]);
return 0;
}
