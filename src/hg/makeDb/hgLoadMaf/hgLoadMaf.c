/* hgLoadMaf - Load a maf file index into the database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadMaf - Load a maf file index into the database\n"
  "usage:\n"
  "   hgLoadMaf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgLoadMaf(char *XXX)
/* hgLoadMaf - Load a maf file index into the database. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
hgLoadMaf(argv[1]);
return 0;
}
