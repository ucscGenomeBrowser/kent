/* bigChainBreaks - output a set of rearrangement breakpoints. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigBed.h"
#include "bigChain.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigChainBreaks - output a set of rearrangement breakpoints\n"
  "usage:\n"
  "   bigChainBreaks bigChain.bb label breaks.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

int bigChainCmp(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct bigChain *a = *((struct bigChain **)va);
const struct bigChain *b = *((struct bigChain **)vb);
int ret = strcmp(a->qName, b->qName);
if (ret)
    return ret;
ret = a->chromStart - b->chromStart;
if (ret)
    return ret;

return  a->qStart - b->qStart;

}

static void parseChrom(struct bigChain *chains, char *label, FILE *f)
{
slSort(&chains, bigChainCmp);

struct bigChain *chain;
char *lastName = "";
//int tStart, tEnd, qStart, qEnd;
int  tEnd=0, qStart=0, qEnd=0;
for(chain = chains; chain ; chain = chain->next)
    {
    // is this chain from a new qName or outside the "current" one
    if (strcmp(lastName, chain->qName) || (chain->chromStart > tEnd))
        {
        // new qName or disjoint chain, initialize variables
        lastName = chain->qName;
        //tStart = chain->chromStart;
        tEnd = chain->chromEnd;
        qStart = chain->qStart;
        qEnd = chain->qEnd;
        }
    else
        {
        // is chain inside the current chain on both target and query?
        if ((chain->chromEnd < tEnd) &&
            (chain->qStart >= qStart) &&
            (chain->qEnd < qEnd))
                {
                //bigChainTabOut(chain, f);
                fprintf(f, "%s %d %s\n", chain->chrom, chain->chromStart, label);
                fprintf(f, "%s %d %s\n", chain->chrom, chain->chromEnd, label);
                }

        }
    }
}

void bigChainBreaks(char *inFile, char *label, char *outFile)
/* bigChainBreaks - output a set of rearrangement breakpoints. */
{
struct bbiFile *bbi = bigBedFileOpen(inFile);
FILE *f = mustOpen(outFile, "w");
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
char startBuf[16], endBuf[16];
char *bedRow[bbi->fieldCount];

for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct lm *lm = lmInit(0);
    struct bigBedInterval *interval, *intervalList = bigBedIntervalQuery(bbi, chrom->name,
        0, chrom->size, 0, lm);
    struct bigChain *chains = NULL;

    for (interval = intervalList; interval != NULL; interval = interval->next)
        {
        bigBedIntervalToRow(interval, chrom->name, startBuf, endBuf, bedRow, ArraySize(bedRow));
        slAddHead(&chains, bigChainLoad(bedRow));
        }
    parseChrom(chains, label, f);

    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
bigChainBreaks(argv[1], argv[2], argv[3]);
return 0;
}
