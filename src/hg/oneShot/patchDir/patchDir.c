/* patchDir - Patch directions of some ESTs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "patchDir - Patch directions of some ESTs\n"
  "usage:\n"
  "   patchDir XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void patchDir(char *XXX)
/* patchDir - Patch directions of some ESTs. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
patchDir(argv[1]);
return 0;
}
