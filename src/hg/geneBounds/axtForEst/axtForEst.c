/* axtForEst - Generate file of mouse/human alignments corresponding to MGC EST's. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtForEst - Generate file of mouse/human alignments corresponding to MGC EST's\n"
  "usage:\n"
  "   axtForEst XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtForEst(char *XXX)
/* axtForEst - Generate file of mouse/human alignments corresponding to MGC EST's. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtForEst(argv[1]);
return 0;
}
