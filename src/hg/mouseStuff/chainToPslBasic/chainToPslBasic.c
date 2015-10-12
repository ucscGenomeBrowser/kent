/* chainToPslBasic - Convert chain to psl format. 
 * This file is copyright 2015 UCSC Browser, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "chainBlock.h"
#include "psl.h"
#include "verbose.h"


/* command line options */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainToPslBasic - Basic conversion chain file to psl format\n"
  "usage:\n"
  "   chainToPsl in.chain out.psl\n"
  "If you need match and mismatch stats updated, pipe output through pslRecalcMatch\n"
  );
}

static void pslAddBlock(struct psl* psl, unsigned qStart, unsigned tStart, int blockSize)
/* add a psl block to a psl being constructed, updating counts */
{
int iBlk = psl->blockCount;
psl->qStarts[iBlk] = qStart;
psl->tStarts[iBlk] = tStart;
psl->blockSizes[iBlk] = blockSize;
psl->match += blockSize;
if (iBlk > 0)
    {
    if (pslQStart(psl, iBlk) > pslQEnd(psl, iBlk-1))
        {
        psl->qNumInsert++;
        psl->qBaseInsert += pslQStart(psl, iBlk)-pslQEnd(psl, iBlk-1);
        }
    if (pslTStart(psl, iBlk) > pslTEnd(psl, iBlk-1))
        {
        psl->tNumInsert++;
        psl->tBaseInsert += pslTStart(psl, iBlk)-pslTEnd(psl, iBlk-1);
        }
    }
psl->blockCount++;
}


struct psl* chainToPsl(struct chain *ch)
/* convert a chain to a psl, ignoring match counts, etc */
{
int qStart = ch->qStart, qEnd = ch->qEnd;
if (ch->qStrand == '-')
    reverseIntRange(&qStart, &qEnd, ch->qSize);
char strand[2] = {ch->qStrand, '\0'};
struct psl* psl = pslNew(ch->qName, ch->qSize, qStart, qEnd,
                         ch->tName, ch->tSize, ch->tStart, ch->tEnd,
                         strand, slCount(ch->blockList), 0);
int iBlk = 0;
struct cBlock *cBlk = NULL;
for (cBlk = ch->blockList; cBlk != NULL; cBlk = cBlk->next, iBlk++)
    pslAddBlock(psl, cBlk->qStart, cBlk->tStart, (cBlk->tEnd - cBlk->tStart));
return psl;
}

void chainToPslBasic(char *inChain, char *outPsl)
/* chainToPslBasic - Convert chain file to psl format. */
{
struct lineFile *inChainFh = lineFileOpen(inChain, TRUE);
FILE *outPslFh = mustOpen(outPsl, "w");

struct chain *chain;
while ((chain = chainRead(inChainFh)) != NULL)
    {
    struct psl* psl = chainToPsl(chain);
    pslTabOut(psl, outPslFh);
    pslFree(&psl);
    chainFree(&chain);
    }
lineFileClose(&inChainFh);
carefulClose(&outPslFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
chainToPslBasic(argv[1], argv[2]);
return 0;
}
