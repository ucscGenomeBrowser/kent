/* hgSanger22 - Load up database with Sanger 22 annotations. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSanger22 - Load up database with Sanger 22 annotations\n"
  "usage:\n"
  "   hgSanger22 XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgSanger22(char *XXX)
/* hgSanger22 - Load up database with Sanger 22 annotations. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgSanger22(argv[1]);
return 0;
}
