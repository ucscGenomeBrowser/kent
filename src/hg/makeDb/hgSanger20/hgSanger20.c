/* hgSanger20 - Load extra info from Sanger Chromosome 20 annotations.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSanger20 - Load extra info from Sanger Chromosome 20 annotations.\n"
  "usage:\n"
  "   hgSanger20 XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgSanger20(char *XXX)
/* hgSanger20 - Load extra info from Sanger Chromosome 20 annotations.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgSanger20(argv[1]);
return 0;
}
