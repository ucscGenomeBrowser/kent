/* netSyntenic - Prune net of non-syntenic elements and gather stats. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netSyntenic - Prune net of non-syntenic elements and gather stats\n"
  "usage:\n"
  "   netSyntenic XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void netSyntenic(char *XXX)
/* netSyntenic - Prune net of non-syntenic elements and gather stats. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
netSyntenic(argv[1]);
return 0;
}
