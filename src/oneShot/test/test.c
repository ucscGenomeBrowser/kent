/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"

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

double mafScoreParts(struct mafAli *maf, int stepSize)
/* Figure score by doing it by parts. */
{
int textSize = maf->textSize;
double acc = 0;
int i, end;
for (i=0; i<textSize; i = end)
    {
    end = i + stepSize;
    if (end > textSize) end = textSize;
    acc += mafScoreRangeMultiz(maf, i, end - i);
    }
return acc;
}

void test(char *fileName)
/* test - Test something. */
{
struct mafFile *mf = mafOpen(fileName);
struct mafAli *maf;
while ((maf = mafNext(mf)) != NULL)
    {
    printf("%6d %8.1f %8.1f %8.1f %8.1f\n", maf->textSize, maf->score, mafScoreMultiz(maf),
        mafScoreParts(maf, 1),  mafScoreParts(maf, 10));
    mafAliFree(&maf);
    }
mafFileFree(&mf);
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
