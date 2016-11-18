/* bigPslToPsl - convert bigPsl file to psl. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigBed.h"
#include "psl.h"
#include "bigPsl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigPslToPsl - convert bigPsl file to psl\n"
  "usage:\n"
  "   bigPslToPsl bigPsl.bb output.psl\n"
  "options:\n"
  "   -collapseStrand   if target strand is '+', don't output it\n"
  );
}

boolean collapseStrand = FALSE;

/* Command line validation table. */
static struct optionSpec options[] = {
    {"collapseStrand", OPTION_BOOLEAN},
    {NULL, 0},
};

void bigPslToPsl(char *bigPslName, char *outputName)
/* bigPslToPsl - convert bigPsl file to psle. */
{
struct bbiFile *bbi = bigBedFileOpen(bigPslName);
int seqTypeField =  bbExtraFieldIndex(bbi, "seqType");
struct lm *lm = lmInit(0);
FILE *f = mustOpen(outputName, "w");
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    int start = 0, end = chrom->size;
    int itemsLeft = 0;
    char *chromName = chrom->name;

    struct bigBedInterval  *bbList = bigBedIntervalQuery(bbi, chromName,
            start, end, itemsLeft, lm);
    
    for(; bbList; bbList = bbList->next)
        {
        struct psl *psl, *pslList = pslFromBigPsl(chromName, bbList, seqTypeField, NULL, NULL);

        for(psl=pslList; psl; psl = psl->next)
            {
            if (collapseStrand && (psl->strand[1] == '+'))
                psl->strand[1] = 0;
            pslTabOut(psl, f);
            }
        }
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
collapseStrand = optionExists("collapseStrand");
bigPslToPsl(argv[1], argv[2]);
return 0;
}
