/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "memalloc.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test something\n"
  "options:\n"
  );
}

void test(char *s)
/* test - Test something. */
{
printf("%s\n", s);
s = makeQuotedString(s, '"');
printf("%s\n", s);
freez(&s);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(100000);
optionHash(&argc, argv);
if (argc != 2)
    usage();
test(argv[1]);
carefulCheckHeap();
return 0;
}
