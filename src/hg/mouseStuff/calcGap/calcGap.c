/* calcGap - calculate gap scores. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "calcGap - calculate gap scores\n"
  "usage:\n"
  "   calcGap XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void calcGap(char *XXX)
/* calcGap - calculate gap scores. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
calcGap(argv[1]);
return 0;
}
