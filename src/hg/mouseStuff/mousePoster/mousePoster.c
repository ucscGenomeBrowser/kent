/* mousePoster - Put together data for mouse poster. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mousePoster - Put together data for mouse poster\n"
  "usage:\n"
  "   mousePoster XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void mousePoster(char *XXX)
/* mousePoster - Put together data for mouse poster. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
mousePoster(argv[1]);
return 0;
}
