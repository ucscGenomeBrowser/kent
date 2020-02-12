/* mafRc - Reverse-complement a MAF. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "jksql.h"
#include "maf.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafRc - Reverse-complement a MAF\n"
  "usage:\n"
  "   mafRc inMafName outMafName\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static void exchangeInts(int *i1, int *i2)
/* exchange two ints */
{
int ihold = *i1;
*i1 = *i2;
*i2 = ihold;
}

static void exchangeChars(char *c1, char *c2)
/* exchange two chars */
{
char chold = *c1;
*c1 = *c2;
*c2 = chold;
}

static void mafRcComponent(struct mafComp *mafComp)
/* reverse-complement a MAF component */
{
mafComp->strand = (mafComp->strand == '+') ? '-' : '+';
mafComp->start = mafComp->srcSize - mafComp-> start;
if (mafComp->text != NULL)
    reverseComplement(mafComp->text, strlen(mafComp->text));
if (mafComp->quality != NULL)
    reverseBytes(mafComp->quality, strlen( mafComp->quality));
exchangeChars(&mafComp->leftStatus, &mafComp->rightStatus);
exchangeInts(&mafComp-> leftLen, &mafComp->rightLen);
}

static void mafRcBlock(struct mafAli *mafAli)
/* reverse-complement a MAF block */
{
struct mafComp *mafComp;
for (mafComp = mafAli->components; mafComp != NULL; mafComp = mafComp->next)
    mafRcComponent(mafComp);
}

static void mafRc(char *inMafName, char *outMafName)
/* mafRc - Reverse-complement a MAF. */
{
struct mafFile* inMaf = mafOpen(inMafName);
FILE* outMafFh = mustOpen(outMafName, "w");
mafWriteStart(outMafFh, inMaf->scoring);

struct mafAli *mafAli;
while ((mafAli = mafNext(inMaf)) != NULL)
    {
    mafRcBlock(mafAli);
    mafWrite(outMafFh, mafAli);
    mafAliFree(&mafAli);
    }
carefulClose(&outMafFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
mafRc(argv[1], argv[2]);
return 0;
}
