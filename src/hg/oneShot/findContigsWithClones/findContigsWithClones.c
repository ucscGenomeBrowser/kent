/* findContigsWithClones - find contigs that contain clones. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "findContigsWithClones - find contigs that contain clones\n"
  "usage:\n"
  "   findContigsWithClones XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void findContigsWithClones(char *XXX)
/* findContigsWithClones - find contigs that contain clones. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
findContigsWithClones(argv[1]);
return 0;
}
