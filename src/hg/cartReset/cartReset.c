/* cartReset - Reset cart. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cartReset - Reset cart\n"
  "usage:\n"
  "   cartReset XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void cartReset(char *XXX)
/* cartReset - Reset cart. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
cartReset(argv[1]);
return 0;
}
