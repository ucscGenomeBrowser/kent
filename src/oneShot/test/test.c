/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test in out\n"
  "options:\n"
  );
}


void test(char *in)
/* test - Test something. */
{
int len = strlen(in);
int trimSize;
trimSize = maskHeadPolyT(in, len);
printf("%s - trimmed %d\n", in, trimSize);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
dnaUtilOpen();
test(argv[1]);
return 0;
}
