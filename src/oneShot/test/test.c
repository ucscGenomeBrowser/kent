/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test a\n"
  "options:\n"
  );
}

void test(char *s)
/* test - Test something. */
{
struct slName *list = stringToSlNames(s), *el;
for (el = list; el != NULL; el = el->next)
    printf("%s\n", el->name);
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
