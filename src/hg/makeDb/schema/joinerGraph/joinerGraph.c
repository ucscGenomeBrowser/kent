/* joinerGraph - Make graph out of joiner. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "joinerGraph - Make graph out of joiner\n"
  "usage:\n"
  "   joinerGraph XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void joinerGraph(char *XXX)
/* joinerGraph - Make graph out of joiner. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
joinerGraph(argv[1]);
return 0;
}
