/* testXap - Initial test harness for Xap XML Parser. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testXap - Initial test harness for Xap XML Parser\n"
  "usage:\n"
  "   testXap XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void testXap(char *XXX)
/* testXap - Initial test harness for Xap XML Parser. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
testXap(argv[1]);
return 0;
}
