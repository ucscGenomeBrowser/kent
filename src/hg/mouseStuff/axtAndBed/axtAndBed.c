/* axtAndBed - Intersect an axt with a bed file and output axt.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtAndBed - Intersect an axt with a bed file and output axt.\n"
  "usage:\n"
  "   axtAndBed XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtAndBed(char *XXX)
/* axtAndBed - Intersect an axt with a bed file and output axt.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtAndBed(argv[1]);
return 0;
}
