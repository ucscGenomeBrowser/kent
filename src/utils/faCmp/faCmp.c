/* faCmp - Compare two .fa files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faCmp - Compare two .fa files\n"
  "usage:\n"
  "   faCmp XXX\n"
  "options:\n"
  "   -xxx=XXX\n");
}

void faCmp(char *XXX)
/* faCmp - Compare two .fa files. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
faCmp(argv[1]);
return 0;
}
