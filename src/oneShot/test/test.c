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
FILE *f = mustOpen(word, "wb");
float x = 1.23456;
mustWrite(f, &x, sizeof(x));
}

int main(int argc, char *argv[], char *env[])
/* Process command line. */
{
if (argc != 2)
   usage();
test(argv[1]);
return 0;
}
