/* bigChainArrange - output inversions and duplications from bigChain. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigBed.h"
#include "chain.h"
#include "bigChain.h"
#include "chainNetDbLoad.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigChainArrange - output inversions and duplications from bigChain\n"
  "usage:\n"
  "   bigChainArrange bigChain.bb bigLinks.bb label inversions.bed dups.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


int chainCmp(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct chain *a = *((struct chain **)va);
const struct chain *b = *((struct chain **)vb);
int ret = strcmp(a->qName, b->qName);
if (ret)
    return ret;
ret = a->tStart - b->tStart;
if (ret)
    return ret;

return  a->qStart - b->qStart;

}

static boolean isInversion(struct chain *chain, struct chain *enclosingChain)
{
if (chain->qStrand == enclosingChain->qStrand)
    return FALSE;
struct cBlock *cb = enclosingChain->blockList;
struct cBlock *prevCb = NULL;
for(; cb; prevCb = cb, cb = cb->next)
    {
    if (cb->tStart >= chain->tEnd)
        {
        if (prevCb->tEnd <= chain->tStart)
            {
            int qStart =  chain->qSize - chain->qEnd;
            int qEnd =  chain->qSize - chain->qStart;
            if ((cb->qStart >= qEnd) &&
                (prevCb->qEnd <= qStart))
                {
                return TRUE;
                }
            }
        break;
        }
    }
return FALSE;
}

static void parseChrom(struct chain *chains,  char *label, FILE *inversions, FILE *duplications)
{
slSort(&chains, chainCmp);

struct chain *chain;
char *lastName = "";
//int tStart, tEnd, qStart, qEnd;
int  tEnd=0, qStart=0, qEnd=0;
struct chain *enclosingChain = NULL;
for(chain = chains; chain ; chain = chain->next)
    {
    // is this chain from a new qName or outside the "current" one
    if (strcmp(lastName, chain->qName) || (chain->tStart > tEnd))
        {
        // new qName or disjoint chain, initialize variables
        enclosingChain = chain;
        lastName = chain->qName;
        //tStart = chain->chromStart;
        tEnd = chain->tEnd;
        qStart = chain->qStart;
        qEnd = chain->qEnd;
        }
    else
        {
        // is chain inside the current chain on both target and query?
        if ((chain->tEnd <= tEnd) &&
            (chain->qStart >= qStart) &&
            (chain->qEnd <= qEnd))
                {
                //bigChainTabOut(chain, f);
                if (isInversion(chain, enclosingChain))
                    fprintf(inversions, "%s %d %d %s\n", chain->tName, chain->tStart, chain->tEnd, label);
                else
                    fprintf(duplications, "%s %d %d %s\n", chain->tName, chain->tStart, chain->tEnd, label);
                }

        }
    }
}

void bigChainArrange(char *inChain, char *inLinks, char *label, char *inversions, char *duplications)
/* bigChainArrange - output a set of rearrangement breakpoints. */
{
struct bbiFile *chainBbi = bigBedFileOpen(inChain);
FILE *inversionsF = mustOpen(inversions, "w");
FILE *duplicationsF = mustOpen(duplications, "w");
struct bbiChromInfo *chrom, *chromList = bbiChromList(chainBbi);

for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct chain  *chains = chainLoadIdRangeHub(NULL, inChain, inLinks, chrom->name, 0, chrom->size, -1);
    parseChrom(chains,  label, inversionsF, duplicationsF);

    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
bigChainArrange(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
