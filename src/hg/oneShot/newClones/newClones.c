/* newClones - Find new clones. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "newClones - Find new clones\n"
  "usage:\n"
  "   newClones XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void newClones(char *XXX)
/* newClones - Find new clones. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
newClones(argv[1]);
return 0;
}
