/* pslCrudeCmp - Crudely compare two psl files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslCrudeCmp - Crudely compare two psl files\n"
  "usage:\n"
  "   pslCrudeCmp XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void pslCrudeCmp(char *XXX)
/* pslCrudeCmp - Crudely compare two psl files. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
pslCrudeCmp(argv[1]);
return 0;
}
