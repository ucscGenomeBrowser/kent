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
  "   test a b\n"
  "options:\n"
  );
}


void test(char *a, char *b)
/* test - Test something. */
{
char *match = memMatch(a, strlen(a), b, strlen(b));
if (match == NULL)
    printf("No match\n");
else
    printf("Match at char %d\n", match - b);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
   usage();
dnaUtilOpen();
test(argv[1], argv[2]);
return 0;
}
