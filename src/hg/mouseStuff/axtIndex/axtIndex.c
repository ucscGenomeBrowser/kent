/* axtIndex - Create summary file for axt. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtIndex - Create summary file for axt\n"
  "usage:\n"
  "   axtIndex XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtIndex(char *XXX)
/* axtIndex - Create summary file for axt. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtIndex(argv[1]);
return 0;
}
