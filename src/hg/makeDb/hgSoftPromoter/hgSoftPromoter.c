/* hgSoftPromoter - Slap Softberry promoter file into database.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSoftPromoter - Slap Softberry promoter file into database.\n"
  "usage:\n"
  "   hgSoftPromoter XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgSoftPromoter(char *XXX)
/* hgSoftPromoter - Slap Softberry promoter file into database.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgSoftPromoter(argv[1]);
return 0;
}
