/* newStitch4 - Further frontiers in stitching. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "newStitch4 - Further frontiers in stitching\n"
  "usage:\n"
  "   newStitch4 XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void newStitch4(char *XXX)
/* newStitch4 - Further frontiers in stitching. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
newStitch4(argv[1]);
return 0;
}
