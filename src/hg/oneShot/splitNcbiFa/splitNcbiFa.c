/* splitNcbiFa - Split up NCBI format fa file into UCSC formatted ones.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "splitNcbiFa - Split up NCBI format fa file into UCSC formatted ones.\n"
  "usage:\n"
  "   splitNcbiFa XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void splitNcbiFa(char *XXX)
/* splitNcbiFa - Split up NCBI format fa file into UCSC formatted ones.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
splitNcbiFa(argv[1]);
return 0;
}
