/* tickToDate - Convert seconds since 1970 to time and date. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tickToDate - Convert seconds since 1970 to time and date\n"
  "usage:\n"
  "   tickToDate XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void tickToDate(char *XXX)
/* tickToDate - Convert seconds since 1970 to time and date. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
tickToDate(argv[1]);
return 0;
}
