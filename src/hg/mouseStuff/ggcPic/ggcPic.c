/* ggcPic - Generic picture of conservation of features near a gene. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ggcPic - Generic picture of conservation of features near a gene\n"
  "usage:\n"
  "   ggcPic XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void ggcPic(char *XXX)
/* ggcPic - Generic picture of conservation of features near a gene. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
ggcPic(argv[1]);
return 0;
}
