/* agpCloneCheck - Check that have all clones in an agp file (and the right version too). */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpCloneCheck - Check that have all clones in an agp file (and the right version too)\n"
  "usage:\n"
  "   agpCloneCheck XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void agpCloneCheck(char *XXX)
/* agpCloneCheck - Check that have all clones in an agp file (and the right version too). */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
agpCloneCheck(argv[1]);
return 0;
}
