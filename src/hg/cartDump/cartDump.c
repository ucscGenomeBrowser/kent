/* cartDump - Dump contents of cart. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cartDump - Dump contents of cart\n"
  "usage:\n"
  "   cartDump XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void cartDump(char *XXX)
/* cartDump - Dump contents of cart. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
cartDump(argv[1]);
return 0;
}
