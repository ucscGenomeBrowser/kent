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

int max = 10;
void loop()
{
int i;
for (i=0; i<max; ++i)
    printf("%d\n", i);
}

int main(int argc, char *argv[], char *env[])
/* Process command line. */
{
int i;
if (argc != 2)
   usage();
test(argv[1]);
return 0;
}
