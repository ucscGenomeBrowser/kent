/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"
#include "axt.h"
#include "fuzzyFind.h"
#include "bandExt.h"
#include "localmem.h"

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

void test(char *a)
/* test - Test something. */
{
FILE *f = mustOpen(a, "w");
char *s = "Hiya boss!\n";
mustWrite(f, s, strlen(s));
sleep(10);
carefulClose(&f);
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
