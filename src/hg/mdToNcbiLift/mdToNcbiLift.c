/* mdToNcbiLift - Convert seq_contig.md file to ncbi.lft. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mdToNcbiLift - Convert seq_contig.md file to ncbi.lft\n"
  "usage:\n"
  "   mdToNcbiLift XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void mdToNcbiLift(char *XXX)
/* mdToNcbiLift - Convert seq_contig.md file to ncbi.lft. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
mdToNcbiLift(argv[1]);
return 0;
}
