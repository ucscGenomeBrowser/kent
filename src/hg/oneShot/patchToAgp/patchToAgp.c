/* patchToAgp - Convert Elia's patch file that describe NT clone position to AGP file. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "patchToAgp - Convert Elia's patch file that describe NT clone position to AGP file\n"
  "usage:\n"
  "   patchToAgp XXX\n");
}

void patchToAgp(char *XXX)
/* patchToAgp - Convert Elia's patch file that describe NT clone position to AGP file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
patchToAgp(argv[1]);
return 0;
}
