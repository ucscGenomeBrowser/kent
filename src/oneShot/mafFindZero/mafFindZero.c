/* mafFindZero - Find small mafs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafFindZero - Find small mafs\n"
  "usage:\n"
  "   mafFindZero mafFile(s)\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void mafFindZero(char *file)
/* mafFindZero - Find small mafs. */
{
struct mafFile *mf = mafOpen(file);
struct mafAli *maf;

while ((maf = mafNext(mf)) != NULL)
    {
    if (maf->textSize <= 0)
        printf("%s %d\n", mf->lf->fileName, mf->lf->lineIx-3);
    mafAliFree(&maf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
optionHash(&argc, argv);
if (argc < 2)
    usage();
for (i=1; i<argc; ++i)
    mafFindZero(argv[i]);
return 0;
}
