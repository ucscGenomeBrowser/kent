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
  "   test a b\n"
  "options:\n"
  );
}

void test(char *a, char *b)
/* test - Test something. */
{
int aSize = strlen(a);
int bSize = strlen(b);
int maxSize = max(aSize, bSize)*2;
int symCount;
char *aSym, *bSym;
int aStart, bStart;
struct axtScoreScheme *ss = axtScoreSchemeRnaDefault();

AllocArray(aSym, maxSize);
AllocArray(bSym, maxSize);
bandExt(TRUE, ss, 2, a, aSize, b, bSize, 1,
	maxSize, &symCount, aSym, bSym, &aStart, &bStart);
printf("score %d\n", axtScoreSym(ss, symCount, aSym, bSym));
puts(aSym);
puts(bSym);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
test(argv[1], argv[2]);
return 0;
}
