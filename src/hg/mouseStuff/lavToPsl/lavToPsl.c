/* lavToPsl - Convert blastz lav to psl format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "lavToPsl - Convert blastz lav to psl format\n"
  "usage:\n"
  "   lavToPsl XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void lavToPsl(char *XXX)
/* lavToPsl - Convert blastz lav to psl format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
lavToPsl(argv[1]);
return 0;
}
