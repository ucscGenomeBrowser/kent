/* raPriority - Set priority fields in order that they occur in trackDb.ra file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "raPriority - Set priority fields in order that they occur in trackDb.ra file\n"
  "usage:\n"
  "   raPriority XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void raPriority(char *XXX)
/* raPriority - Set priority fields in order that they occur in trackDb.ra file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
raPriority(argv[1]);
return 0;
}
