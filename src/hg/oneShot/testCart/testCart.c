/* testCart - Test cart routines.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testCart - Test cart routines.\n"
  "usage:\n"
  "   testCart XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void testCart(char *XXX)
/* testCart - Test cart routines.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
testCart(argv[1]);
return 0;
}
