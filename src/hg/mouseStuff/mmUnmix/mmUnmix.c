/* mmUnmix - Help identify human contamination in mouse and vice versa.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mmUnmix - Help identify human contamination in mouse and vice versa.\n"
  "usage:\n"
  "   mmUnmix XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void mmUnmix(char *XXX)
/* mmUnmix - Help identify human contamination in mouse and vice versa.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
mmUnmix(argv[1]);
return 0;
}
