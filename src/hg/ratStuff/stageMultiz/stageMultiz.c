/* stageMultiz - Stage input directory for Webb's multiple aligner. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "stageMultiz - Stage input directory for Webb's multiple aligner\n"
  "usage:\n"
  "   stageMultiz XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void stageMultiz(char *XXX)
/* stageMultiz - Stage input directory for Webb's multiple aligner. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
stageMultiz(argv[1]);
return 0;
}
