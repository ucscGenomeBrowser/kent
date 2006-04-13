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

struct refString
    {
    int refCount;
    int type;
    char *s;
    int size;
    int alloced;
    bool isConst;
    };

struct refString refString = {0, 0, "Hello world", 11, 11, TRUE};

void test(char *s)
{
printf("%s\n", refString.s);
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
