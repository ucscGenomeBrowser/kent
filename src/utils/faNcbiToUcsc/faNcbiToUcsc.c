/* faNcbiToUcsc - Convert FA file from NCBI to UCSC format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faNcbiToUcsc - Convert FA file from NCBI to UCSC format.\n"
  "usage:\n"
  "   faNcbiToUcsc XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void faNcbiToUcsc(char *XXX)
/* faNcbiToUcsc - Convert FA file from NCBI to UCSC format.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
faNcbiToUcsc(argv[1]);
return 0;
}
