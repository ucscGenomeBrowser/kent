/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "net.h"
#include "rudp.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void test(char *XXX)
/* test - Test something. */
{
rudpTest();
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
test(argv[1]);
return 0;
}
