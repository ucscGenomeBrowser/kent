/* nt4Frag - Extract a piece of a .nt4 file to .fa format. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "nt4Frag - Extract a piece of a .nt4 file to .fa format\n"
  "usage:\n"
  "   nt4Frag XXX\n");
}

void nt4Frag(char *XXX)
/* nt4Frag - Extract a piece of a .nt4 file to .fa format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
nt4Frag(argv[1]);
return 0;
}
