/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "memgfx.h"

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
MgFont *font = mgSmallFont();
printf("small font height = %d\n", mgFontLineHeight(font));
font = mgSmallishFont();
printf("smallish font height = %d\n", mgFontLineHeight(font));
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
