/* borfBig - Run Victor Solovyev's bestorf repeatedly. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "borfBig - Run Victor Solovyev's bestorf repeatedly\n"
  "usage:\n"
  "   borfBig XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void borfBig(char *XXX)
/* borfBig - Run Victor Solovyev's bestorf repeatedly. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
borfBig(argv[1]);
return 0;
}
