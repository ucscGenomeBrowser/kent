/* lavToAxt - Convert blastz lav file to an axt file (which includes sequence). */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "lavToAxt - Convert blastz lav file to an axt file (which includes sequence)\n"
  "usage:\n"
  "   lavToAxt XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void lavToAxt(char *XXX)
/* lavToAxt - Convert blastz lav file to an axt file (which includes sequence). */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
lavToAxt(argv[1]);
return 0;
}
