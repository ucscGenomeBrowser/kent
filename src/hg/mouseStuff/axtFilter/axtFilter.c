/* axtFilter - Filter axt files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtFilter - Filter axt files\n"
  "usage:\n"
  "   axtFilter XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtFilter(char *XXX)
/* axtFilter - Filter axt files. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtFilter(argv[1]);
return 0;
}
