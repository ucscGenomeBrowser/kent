/* regionPicker - Code to pick regions to annotate deeply.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regionPicker - Code to pick regions to annotate deeply.\n"
  "usage:\n"
  "   regionPicker XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void regionPicker(char *XXX)
/* regionPicker - Code to pick regions to annotate deeply.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
regionPicker(argv[1]);
return 0;
}
