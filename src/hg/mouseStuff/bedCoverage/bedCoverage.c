/* bedCoverage - Analyse coverage by bed files - chromosome by chromosome and genome-wide.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedCoverage - Analyse coverage by bed files - chromosome by chromosome and genome-wide.\n"
  "usage:\n"
  "   bedCoverage XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void bedCoverage(char *XXX)
/* bedCoverage - Analyse coverage by bed files - chromosome by chromosome and genome-wide.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
bedCoverage(argv[1]);
return 0;
}
