/* newStitch3 - Another stitching experiment - with kd-trees.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "newStitch3 - Another stitching experiment - with kd-trees.\n"
  "usage:\n"
  "   newStitch3 XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void newStitch3(char *XXX)
/* newStitch3 - Another stitching experiment - with kd-trees.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
newStitch3(argv[1]);
return 0;
}
