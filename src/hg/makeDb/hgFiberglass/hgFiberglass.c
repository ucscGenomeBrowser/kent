/* hgFiberglass - Turn Fiberglass Annotations into a BED and load into database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgFiberglass - Turn Fiberglass Annotations into a BED and load into database\n"
  "usage:\n"
  "   hgFiberglass XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgFiberglass(char *XXX)
/* hgFiberglass - Turn Fiberglass Annotations into a BED and load into database. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgFiberglass(argv[1]);
return 0;
}
