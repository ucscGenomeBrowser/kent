/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"

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

char *testRa = 
"a alpha\n"
"b beta\n"
"c gamma\n"
"abc alpha beta gamma\n"
;

void test(char *s)
/* test - Test something. */
{
struct hash *ra = raFromString(testRa);
struct hashEl *elList = hashElListHash(ra), *el;
for (el = elList; el != NULL; el = el->next)
    printf("%s '%s'\n", el->name, (char*)el->val);
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
