/* detab - remove tabs from program. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "detab - remove tabs from program\n"
  "usage:\n"
  "   detab XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void detab(char *XXX)
/* detab - remove tabs from program. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
detab(argv[1]);
return 0;
}
