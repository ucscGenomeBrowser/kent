/* hgLoadAxt - Load an axt file index into the database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadAxt - Load an axt file index into the database\n"
  "usage:\n"
  "   hgLoadAxt XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgLoadAxt(char *XXX)
/* hgLoadAxt - Load an axt file index into the database. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
hgLoadAxt(argv[1]);
return 0;
}
