/* hgLoadBed - Load a generic bed file into database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadBed - Load a generic bed file into database\n"
  "usage:\n"
  "   hgLoadBed XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgLoadBed(char *XXX)
/* hgLoadBed - Load a generic bed file into database. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgLoadBed(argv[1]);
return 0;
}
