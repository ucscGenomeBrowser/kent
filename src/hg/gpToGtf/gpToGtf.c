/* gpToGtf - Convert gp table to GTF. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gpToGtf - Convert gp table to GTF\n"
  "usage:\n"
  "   gpToGtf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void gpToGtf(char *XXX)
/* gpToGtf - Convert gp table to GTF. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
gpToGtf(argv[1]);
return 0;
}
