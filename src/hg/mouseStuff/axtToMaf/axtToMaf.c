/* axtToMaf - Convert from axt to maf format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtToMaf - Convert from axt to maf format\n"
  "usage:\n"
  "   axtToMaf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtToMaf(char *XXX)
/* axtToMaf - Convert from axt to maf format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtToMaf(argv[1]);
return 0;
}
