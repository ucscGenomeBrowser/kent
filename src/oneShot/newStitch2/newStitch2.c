/* newStitch2 - Further experiments in stitching. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "newStitch2 - Further experiments in stitching\n"
  "usage:\n"
  "   newStitch2 XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void newStitch2(char *XXX)
/* newStitch2 - Further experiments in stitching. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
newStitch2(argv[1]);
return 0;
}
