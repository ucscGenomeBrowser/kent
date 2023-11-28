
/* chainInDel - output a bed file with indels within the given chain. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainInDel - output a bed file with indels within the given chain\n"
  "usage:\n"
  "   chainIndel file.chain label indels.bed\n"
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

static void parseChrom(struct chain *chains,  char *label, FILE *inDelF)
{
slSort(&chains, chainCmp);

struct chain *chain;
for(chain = chains; chain ; chain = chain->next)
    {
    struct cBlock *cb = chain->blockList;
    for(; cb->next != NULL;  cb = cb->next)
        {
        fprintf(inDelF,"%s %d %d %s %d\n", chain->tName,
            cb->tEnd, cb->next->tStart, label,
            cb->next->qStart - cb->qEnd);

        }
    }
}

struct chainHead
{
struct chainHead *next;
struct chain *chains;
};

struct chainHead *readChains(char *inChain)
{
struct lineFile *lf = lineFileOpen(inChain, TRUE);
struct hash *hash = newHash(0);
struct chainHead *chainHead, *chainHeads = NULL;
struct chain *chain;

while ((chain = chainRead(lf)) != NULL)
    {
    if ((chainHead = hashFindVal(hash, chain->tName)) == NULL)
        {
        AllocVar(chainHead);
        slAddHead(&chainHeads, chainHead);
        hashAdd(hash, chain->tName, chainHead);
        }

    slAddHead(&chainHead->chains,  chain);
    }

return chainHeads;
}

void chainInDel(char *inChain, char *label, char *inDels)
/* chainInDel - output a set of rearrangement breakpoints. */
{
struct chainHead *chainHeads = readChains(inChain);
FILE *inDelF = mustOpen(inDels, "w");

for (; chainHeads != NULL; chainHeads = chainHeads->next)
    {
    struct chain  *chains = chainHeads->chains;
    parseChrom(chains,  label, inDelF);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
chainInDel(argv[1], argv[2], argv[3]);
return 0;
}
