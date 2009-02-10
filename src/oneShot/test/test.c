/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "xp.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test a \n"
  "options:\n"
  );
}


void test(char *word)
{
printf("%d\n", word[0]);
}

int main(int argc, char *argv[], char *env[])
/* Process command line. */
{
if (argc != 2)
   usage();
test(argv[1]);
return 0;
}
