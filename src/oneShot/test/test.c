/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test a.fa b.fa\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void test(char *aFile, char *bFile)
/* test - Test something. */
{
struct dnaSeq *aSeq = faReadDna(aFile);
struct dnaSeq *bSeq = faReadDna(bFile);
printf("%s %s %d\n", aFile, aSeq->name, aSeq->size);
printf("%s %s %d\n", bFile, bSeq->name, bSeq->size);
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
