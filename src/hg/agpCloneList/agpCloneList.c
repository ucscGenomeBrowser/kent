/* agpCloneList - Make simple list of all clones in agp file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpCloneList - Make simple list of all clones in agp file\n"
  "usage:\n"
  "   agpCloneList XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void agpCloneList(char *XXX)
/* agpCloneList - Make simple list of all clones in agp file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
agpCloneList(argv[1]);
return 0;
}
