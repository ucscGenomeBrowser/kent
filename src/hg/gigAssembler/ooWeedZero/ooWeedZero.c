/* ooWeedZero - Weed out phase zero clones from geno.lst. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ooWeedZero - Weed out phase zero clones from geno.lst\n"
  "usage:\n"
  "   ooWeedZero XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void ooWeedZero(char *XXX)
/* ooWeedZero - Weed out phase zero clones from geno.lst. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
ooWeedZero(argv[1]);
return 0;
}
