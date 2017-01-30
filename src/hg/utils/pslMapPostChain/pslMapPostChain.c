/* pslMapPostChain - Post genomic pslMap (TransMap) chaining. */

#include "common.h"
#include "linefile.h"
#include "options.h"
#include "hash.h"
#include "psl.h"
#include <string.h>

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s:\n\n"
         "postTransMapChain [options] inPsl outPsl\n"
         "\n"
         "Post genomic pslMap (TransMap) chaining.  This takes transcripts\n"
         "that have been mapped via genomic chains adds back in\n"
         "blocks that didn't get include in genomic chains due\n"
         "to complex rearrangements or other issues.\n"
         "\n"
         "This program has not seen much use and may not do what you want\n", msg);
}

static struct optionSpec optionSpecs[] =
{
    {"h", OPTION_BOOLEAN},
    {"help", OPTION_BOOLEAN},
    {NULL, 0}
};
/* max distance to attempt to chain over */
static int maxAdjacentDistance = 50*1000*1000;  // 50mb

/* special status used to in place of chain */
static const int CHAIN_NOT_HERE = -1;
static const int CHAIN_CAN_NOT = -2;

static struct hash* loadPslByQname(char* inPslFile)
/* load PSLs in to hash by qName.  Make sure target strand is positive
 * to make process easier later. */
{
struct hash* pslsByQName = hashNew(0);
struct psl *psls = pslLoadAll(inPslFile);
struct psl *psl;
while ((psl = slPopHead(&psls)) != NULL)
    {
    if (pslTStrand(psl) != '+')
        pslRc(psl);
    struct hashEl *hel = hashStore(pslsByQName, psl->qName);
    struct psl** queryPsls = (struct psl**)&hel->val;
    slAddHead(queryPsls, psl);
    }
return pslsByQName;
}

static int pslTSpan(const struct psl* psl)
/* get target span */
{
return psl->tEnd - psl->tStart;
}

static int pslCmpTargetAndStrandSpan(const void *va, const void *vb)
/* Compare to sort based on target, strand,  tStart, and then span */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
int dif;
dif = strcmp(a->tName, b->tName);
if (dif == 0)
    dif = strcmp(a->strand, b->strand);
if (dif == 0)
    dif = -(pslTSpan(a) - pslTSpan(b));
return dif;
}

static char normStrand(char strand)
/* return strand as stored in psl, converting implicit `\0' to `+' */
{
return (strand == '\0') ? '+' : strand;
}

static unsigned pslQStartStrand(struct psl *psl, int blkIdx, char strand)
/* return query start for the given block, mapped to specified strand,
 * which can be `\0' for `+' */
{
if (psl->strand[0] == normStrand(strand))
    return psl->qStarts[blkIdx];
else
    return psl->qSize - pslQEnd(psl, blkIdx);
}

static unsigned pslQEndStrand(struct psl *psl, int blkIdx, char strand)
/* return query end for the given block, mapped to specified strand,
 * which can be `\0' for `+' */
{
if (psl->strand[0] == normStrand(strand))
    return pslQEnd(psl, blkIdx);
else
    return psl->qSize - pslQStart(psl, blkIdx);
}

static unsigned pslTStartStrand(struct psl *psl, int blkIdx, char strand)
/* return target start for the given block, mapped to specified strand,
 * which can be `\0' for `+' */
{
if (normStrand(psl->strand[1]) == normStrand(strand))
    return psl->tStarts[blkIdx];
else
    return psl->tSize - pslTEnd(psl, blkIdx);
}

static unsigned pslTEndStrand(struct psl *psl, int blkIdx, char strand)
/* return target end for the given block, mapped to specified strand,
 * which can be `\0' for `+' */
{
if (normStrand(psl->strand[1]) == normStrand(strand))
    return pslTEnd(psl, blkIdx);
else
    return psl->tSize - pslTStart(psl, blkIdx);
}

static int findChainPointUpstream(struct psl* chainedPsl,
                                  struct psl* nextPsl)
/* findChainPoint upstream check. */
{
// check next being query upstream of chain
if ((pslQEnd(nextPsl, nextPsl->blockCount-1) <= pslQStart(chainedPsl, 0))
    && (pslTEnd(nextPsl, nextPsl->blockCount-1) <= pslTStart(chainedPsl, 0)))
    {
    if ((pslTStart(chainedPsl, 0) - pslTEnd(nextPsl, nextPsl->blockCount-1)) > maxAdjacentDistance)
        return CHAIN_CAN_NOT;  // too far before
    else
        return 0;
    }
else
    return CHAIN_NOT_HERE;
}

static int findChainPointDownstream(struct psl* chainedPsl,
                                    struct psl* nextPsl)
/* findChainPoint downstream check. */
{
if ((pslQStart(nextPsl, 0) >= pslQEnd(chainedPsl, chainedPsl->blockCount-1))
    && (pslTStart(nextPsl, 0) >= pslTEnd(chainedPsl, chainedPsl->blockCount-1)))
    {
    if ((pslTStart(nextPsl, 0) - pslTEnd(chainedPsl, chainedPsl->blockCount-1)) > maxAdjacentDistance)
        return CHAIN_CAN_NOT;  // too far after
    else
        return chainedPsl->blockCount;
    }
else
    return CHAIN_NOT_HERE; 
}

static int isBetweenBlocks(struct psl* chainedPsl,
                           struct psl* nextPsl,
                           int insertIdx)
/* does nextPsl fit between two chained PSL blocks */
{
return (pslQEnd(chainedPsl, insertIdx-1) <= pslQStart(nextPsl, 0))
    && (pslTEnd(chainedPsl, insertIdx-1) <= pslTStart(nextPsl, 0))
    && (pslQEnd(nextPsl, nextPsl->blockCount-1) <= pslQStart(chainedPsl, insertIdx))
    && (pslTEnd(nextPsl, nextPsl->blockCount-1) <= pslTStart(chainedPsl, insertIdx));
}

static int findChainPointInternal(struct psl* chainedPsl,
                                  struct psl* nextPsl)
/* find internal insertion point between block of chainedPsl. */
{
int insertIdx;
for (insertIdx = 1; insertIdx < chainedPsl->blockCount; insertIdx++)
    {
    if (isBetweenBlocks(chainedPsl, nextPsl, insertIdx))
        return insertIdx;
    }
return CHAIN_CAN_NOT;
}

static int findChainPoint(struct psl* chainedPsl,
                          struct psl* nextPsl)
/* Return the position in the chained PSL where a PSL can be added.  This
 * returns the block-index of where to insert the PSL, blockCount if to added
 * after, or one of the negative constants if it should not be chained.
 * assumes it chainedPsl has longest target span so we only look for insert
 * in chainPsl and not the other way around. */
{
if (!(sameString(chainedPsl->tName, nextPsl->tName)
      && sameString(chainedPsl->strand, nextPsl->strand)))
        return CHAIN_CAN_NOT;  // not on same seq/strand
int insertIdx = findChainPointUpstream(chainedPsl, nextPsl);
if (insertIdx != CHAIN_NOT_HERE)
    return insertIdx;
insertIdx = findChainPointDownstream(chainedPsl, nextPsl);
if (insertIdx != CHAIN_NOT_HERE)
    return insertIdx;
insertIdx = findChainPointInternal(chainedPsl, nextPsl);
return insertIdx;
}

static void pslArrayInsert(unsigned **destArray,
                           int numDestBlocks,
                           unsigned *srcArray,
                           int numSrcBlocks,
                           int insertIdx)
/* Grow one of the psl arrays and move contents down
 * to make room, and copy new entries */
{
*destArray = needMoreMem(*destArray, numDestBlocks*sizeof(unsigned), (numDestBlocks+numSrcBlocks)*sizeof(unsigned));
int iBlk;
for (iBlk = numDestBlocks-1; iBlk >= insertIdx; iBlk--)
    {
    assert(iBlk+numSrcBlocks < numDestBlocks+numSrcBlocks);
    (*destArray)[iBlk+numSrcBlocks] = (*destArray)[iBlk];
    }
for (iBlk = 0; iBlk < numSrcBlocks; iBlk++)
    {
    assert(iBlk+insertIdx < numDestBlocks+numSrcBlocks);
    (*destArray)[iBlk+insertIdx] = srcArray[iBlk];
    }
}

static void addPslToChained(struct psl* chainedPsl,
                            struct psl* nextPsl,
                            int insertIdx)
/* add blocks form a psl to the chained psl at the given point */
{
assert(insertIdx <= chainedPsl->blockCount);
// expand arrays and copy in entries
pslArrayInsert(&chainedPsl->blockSizes, chainedPsl->blockCount, nextPsl->blockSizes, nextPsl->blockCount, insertIdx);
pslArrayInsert(&chainedPsl->qStarts, chainedPsl->blockCount, nextPsl->qStarts, nextPsl->blockCount, insertIdx);
pslArrayInsert(&chainedPsl->tStarts, chainedPsl->blockCount, nextPsl->tStarts, nextPsl->blockCount, insertIdx);
chainedPsl->blockCount += nextPsl->blockCount;

// update bounds if needed
if (pslQStrand(chainedPsl) == '+')
    {
    chainedPsl->qStart = pslQStartStrand(chainedPsl, 0, '+');
    chainedPsl->qEnd = pslQEndStrand(chainedPsl, chainedPsl->blockCount-1, '+');
    }
else
    {
    chainedPsl->qStart = pslQStartStrand(chainedPsl, chainedPsl->blockCount-1, '+');
    chainedPsl->qEnd = pslQEndStrand(chainedPsl, 0, '+');
    }
assert(pslTStrand(chainedPsl) == '+');
chainedPsl->tStart = pslTStartStrand(chainedPsl, 0, '+');
chainedPsl->tEnd = pslTEndStrand(chainedPsl, chainedPsl->blockCount-1, '+');

// update counts
chainedPsl->match += nextPsl->match;
chainedPsl->misMatch += nextPsl->misMatch;
chainedPsl->repMatch += nextPsl->repMatch;
chainedPsl->nCount += nextPsl->nCount;
}

static void chainToLongest(struct psl** queryPsls,
                           FILE* outPslFh)
/* pull off one or more psls from the list and chain them */
{
struct psl* chainedPsl = slPopHead(queryPsls);
struct psl* unchainedPsls = NULL;  // ones not included
struct psl* nextPsl;
while ((nextPsl = slPopHead(queryPsls)) != NULL)
    {
    int insertIdx = findChainPoint(chainedPsl, nextPsl);
    if (insertIdx >= 0)
        {
        addPslToChained(chainedPsl, nextPsl, insertIdx);
        pslFree(&nextPsl);
        }
    else
        {
        slAddHead(&unchainedPsls, nextPsl);
        }
    }
pslComputeInsertCounts(chainedPsl);
pslTabOut(chainedPsl, outPslFh);
pslFree(&chainedPsl);
slReverse(&unchainedPsls); // preserve longest to shortest order
*queryPsls = unchainedPsls;
}

static void chainQuery(struct psl** queryPsls,
                       FILE* outPslFh)
/* make chains for a single query. Takes over ownership
 * of PSLs */
{
// sort by genomic location, then pull of a group to
// to chain
slSort(queryPsls, pslCmpTargetAndStrandSpan);
while (*queryPsls != NULL)
    chainToLongest(queryPsls, outPslFh);
}

static void pslMapPostChain(char* inPslFile,
                            char* outPslFile)
/* do chaining */
{
struct hash* pslsByQName = loadPslByQname(inPslFile);
FILE* outPslFh = mustOpen(outPslFile, "w");
struct hashEl *hel;
struct hashCookie cookie = hashFirst(pslsByQName);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct psl** queryPsls = (struct psl**)&hel->val;
    chainQuery(queryPsls, outPslFh);
    }
carefulClose(&outPslFh);
}

int main(int argc, char **argv)
/* entry */
{
optionInit(&argc, argv, optionSpecs);
if (optionExists("h") || optionExists("help"))
    usage("usage");
if (argc != 3)
    usage("wrong # of args");
pslMapPostChain(argv[1], argv[2]);
return 0;
}


