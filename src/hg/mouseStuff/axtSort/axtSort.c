/* axtSort - Sort axt files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtSort - Sort axt files\n"
  "usage:\n"
  "   axtSort XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtSort(char *XXX)
/* axtSort - Sort axt files. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtSort(argv[1]);
return 0;
}
