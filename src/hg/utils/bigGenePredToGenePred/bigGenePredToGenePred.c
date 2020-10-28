/* bigGenePredToGenePred - convert bigGenePred file to genePred. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigGenePred.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigGenePredToGenePred - convert bigGenePred file to genePred.\n"
  "usage:\n"
  "   bigGenePredToGenePred bigGenePred.bb genePred.gp\n"
  "\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void bigGenePredToGenePred(char *bigGenePredFile, char *genePredFile)
/* bigGenePredToGenePred - convert bigGenePred file to genePred. */
{
struct bbiFile *bbi = bigBedFileOpen(bigGenePredFile);
struct lm *lm = lmInit(0);
FILE *gpFh = mustOpen(genePredFile, "w");
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom->name, 0, chrom->size, 0, lm);
    for(; bbList != NULL; bbList = bbList->next)
        {
        struct genePredExt *gp = genePredFromBigGenePred(chrom->name, bbList);
        genePredTabOut((struct genePred *)gp, gpFh);
        }
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bigGenePredToGenePred(argv[1], argv[2]);
return 0;
}
