
/* Copyright (C) 2023 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "obscure.h"
#include "limits.h"
#include "float.h"
#include "asParse.h"
#include "chain.h"
#include "binRange.h"
#include "basicBed.h"
#include "liftOver.h"
#include "hash.h"
#include "bigBed.h"
#include "bbiFile.h"
#include "chainNetDbLoad.h"
#include "hdb.h"
#include "jksql.h"
#include "hgConfig.h"
#include "quickLift.h"
#include "multiRegionSpace.h"
#include "trackHub.h"
#include "genePredReader.h"
#include "bigChain.h"
#include "bigLink.h"
#include "chromAlias.h"
#include "customTrack.h"
#include "encode/encodePeak.h"
#include "rmskOut.h"
#include "rmskJoined.h"
#include "psl.h"
#include "chainToPsl.h"
#include "pslTransMap.h"
#include "maf.h"
#include "errCatch.h"
#include <pthread.h>

/* ---- A chain's blocks, indexed for binary search ----
 *
 * Every item lifted asks its chain where the nearest aligned base is and which chain covers
 * it best, and walking a chain's block list to answer is linear.  In a multi-region space one
 * chain can carry tens of thousands of blocks (36,863 for exon view over hg38 chr1), and a
 * gene track there lifts tens of thousands of items, so the walks alone took knownGene past
 * the load timeout (#38660).  The index is built once per chain and shared by every thread
 * that loads a track, hence the mutex. */

struct blockIndex
/* A chain's blocks as an array in target order. */
    {
    struct cBlock *head;        /* chain->blockList when built:  a reused address differs */
    int count;                  /* number of blocks */
    struct cBlock **blocks;     /* the blocks, in list order */
    boolean sorted;             /* ascending and non-overlapping on target, so searchable */
    };

static struct hash *blockIndexHash = NULL;
static pthread_mutex_t blockIndexMutex = PTHREAD_MUTEX_INITIALIZER;

static boolean blockIndexStale(struct blockIndex *bi, struct chain *chain)
/* TRUE if bi is missing or was not built from chain's current block list:  a different
 * head, or a block added after the last one. */
{
return ((bi == NULL) || (bi->head != chain->blockList)
        || ((bi->count > 0) && (bi->blocks[bi->count - 1]->next != NULL)));
}

static struct blockIndex *chainBlockIndex(struct chain *chain)
/* The block index for chain, built the first time it is asked for, and rebuilt if the
 * chain's block list is not the one it was built from.  The index is keyed by the chain's
 * address and never dropped, so a chain that is indexed must not be freed while the
 * process runs:  hgTracks never frees the chains in a track's chain hash.
 * Nothing that can errAbort runs while the lock is held without an errCatch around it, so
 * an allocation failure in one loader thread cannot leave the lock held for the rest. */
{
char key[64];
safef(key, sizeof key, "%p", chain);
pthread_mutex_lock(&blockIndexMutex);
struct blockIndex *bi = (blockIndexHash == NULL) ? NULL : hashFindVal(blockIndexHash, key);
pthread_mutex_unlock(&blockIndexMutex);
if (!blockIndexStale(bi, chain))
    return bi;

/* Build outside the lock:  the chain belongs to the caller's thread. */
struct blockIndex *fresh;
AllocVar(fresh);
fresh->head = chain->blockList;
fresh->count = slCount(chain->blockList);
AllocArray(fresh->blocks, max(fresh->count, 1));
fresh->sorted = TRUE;
struct cBlock *b;
int i = 0;
for (b = chain->blockList; b != NULL; b = b->next, ++i)
    {
    fresh->blocks[i] = b;
    if ((i > 0) && (b->tStart < fresh->blocks[i-1]->tEnd))
        fresh->sorted = FALSE;
    }

struct errCatch *errCatch = errCatchNew();
pthread_mutex_lock(&blockIndexMutex);
if (errCatchStart(errCatch))
    {
    if (blockIndexHash == NULL)
        blockIndexHash = newHash(10);
    if (hashLookup(blockIndexHash, key) == NULL)
        hashAdd(blockIndexHash, key, fresh);
    else
        hashReplace(blockIndexHash, key, fresh);   /* the old one is left, a CGI's leak */
    }
errCatchEnd(errCatch);
pthread_mutex_unlock(&blockIndexMutex);
if (errCatch->gotError)
    errAbort("%s", errCatch->message->string);
errCatchFree(&errCatch);
return fresh;
}

static int firstBlockEndingAfter(struct blockIndex *bi, int pos, boolean orAt)
/* Index of the first block whose tEnd is past pos (orAt:  at or past it), or bi->count. */
{
int lo = 0, hi = bi->count;
while (lo < hi)
    {
    int mid = (lo + hi) / 2;
    int tEnd = bi->blocks[mid]->tEnd;
    if (orAt ? (tEnd >= pos) : (tEnd > pos))
        hi = mid;
    else
        lo = mid + 1;
    }
return lo;
}

static int chainAlignedOverlap(struct chain *chain, int start, int end)
/* Aligned bases of chain in start-end. */
{
struct blockIndex *bi = chainBlockIndex(chain);
int overlap = 0;
if (!bi->sorted)
    {
    struct cBlock *b;
    for (b = chain->blockList; b != NULL; b = b->next)
        overlap += positiveRangeIntersection(b->tStart, b->tEnd, start, end);
    return overlap;
    }
int i;
for (i = firstBlockEndingAfter(bi, start, FALSE);
     (i < bi->count) && (bi->blocks[i]->tStart < end); ++i)
    overlap += positiveRangeIntersection(bi->blocks[i]->tStart, bi->blocks[i]->tEnd,
                                         start, end);
return overlap;
}

static struct chain *quickLiftBestChain(struct hash *chainHash, char *chrom, int start, int end)
/* The chain in chainHash covering the most aligned bases in start-end, or NULL if none has
 * one.  On a tie, the first one liftOverChainsInRange() returns.  Each chain is searched
 * through its block index, not walked. */
{
struct binElement *el, *binList = liftOverChainsInRange(chainHash, chrom, start, end);
struct chain *best = NULL;
int bestOverlap = 0;
for (el = binList; el != NULL; el = el->next)
    {
    struct chain *chain = el->val;
    int overlap = chainAlignedOverlap(chain, start, end);
    if (overlap > bestOverlap)
        {
        bestOverlap = overlap;
        best = chain;
        }
    }
slFreeList(&binList);
return best;
}

static struct chain *chainForRange(struct hash *chainHash, char *chrom, int start, int end)
/* The chain that best covers chrom:start-end.  With multiRegionSpaces on, the chain is found
 * through its block index by quickLiftBestChain();  otherwise by liftOverChainForRange(),
 * which walks each chain's blocks.  refs #38660 */
{
if (multiRegionSpacesEnabled())
    return quickLiftBestChain(chainHash, chrom, start, end);
return liftOverChainForRange(chainHash, chrom, start, end);
}

/* Every liftOverChainForRange() call below this point goes through chainForRange(), so
 * master's call sites pick up the block index in a space without a second lookup.
 * Remove this with the multiRegionSpaces gate, and rename those callers to
 * quickLiftBestChain().  refs #37788 */
#define liftOverChainForRange(chainHash, chrom, start, end) \
    chainForRange(chainHash, chrom, start, end)

static boolean quickLiftChainRangeIn(struct chain *chain, int tStart, int tEnd,
    int *retQStart, int *retQEnd)
/* The query side range matching tStart..tEnd on the target, rather than the whole extent
 * of the chain's blocks.  One block can be enormous:  hg19 and hg38 run identical for
 * 12.8Mb on chr7, so the whole-block answer would ask the other assembly for millions of
 * bases either side of the window.  Within a block the two sides are colinear, so the
 * part that matters can be worked out exactly.  Returns FALSE if no block overlaps. */
{
struct cBlock *cb;
int qStart = 0, qEnd = 0;
boolean any = FALSE;

for (cb = chain->blockList; cb != NULL; cb = cb->next)
    {
    int s = max(cb->tStart, tStart);
    int e = min(cb->tEnd, tEnd);
    if (s >= e)
        continue;

    int qLo = cb->qStart + (s - cb->tStart);
    int qHi = cb->qStart + (e - cb->tStart);
    if (!any || (qLo < qStart))
        qStart = qLo;
    if (!any || (qHi > qEnd))
        qEnd = qHi;
    any = TRUE;
    }
if (!any)
    return FALSE;

// correct for strand
if (chain->qStrand == '-')
    {
    int saveStart = qStart;
    qStart = chain->qSize - qEnd;
    qEnd = chain->qSize - saveStart;
    }
*retQStart = qStart;
*retQEnd = qEnd;
return TRUE;
}

static void addSourceRun(struct chain *chain, int qStart, int qEnd,
                         struct quickLiftRange **pRangeList)
/* Add one source range, given in the chain's own query coordinates, to *pRangeList. */
{
struct quickLiftRange *range;
AllocVar(range);
range->chrom = cloneString(chain->qName);
if (chain->qStrand == '-')
    {
    range->start = chain->qSize - qEnd;
    range->end = chain->qSize - qStart;
    }
else
    {
    range->start = qStart;
    range->end = qEnd;
    }
slAddHead(pRangeList, range);
}

void quickLiftChainSourceRuns(struct chain *chain, int tStart, int tEnd,
                              struct quickLiftRange **pRangeList)
/* Like quickLiftChainRangeIn(), but cut the source range wherever the chain skips more
 * than QUICKLIFT_SPLIT_GAP source bases, and add each piece to *pRangeList.  The chain
 * behind a multi-region space joins regions that are far apart on one chromosome, and
 * one range from its first block to its last asked the source for everything between
 * them:  195 Mb of chr1 alignment for 13 kb of regions.  refs #37788 */
{
struct cBlock *cb;
int runStart = 0, runEnd = 0;
boolean any = FALSE;

for (cb = chain->blockList; cb != NULL; cb = cb->next)
    {
    int s = max(cb->tStart, tStart);
    int e = min(cb->tEnd, tEnd);
    if (s >= e)
        continue;

    int qLo = cb->qStart + (s - cb->tStart);
    int qHi = cb->qStart + (e - cb->tStart);
    if (any && (qLo - runEnd > QUICKLIFT_SPLIT_GAP))
        {
        addSourceRun(chain, runStart, runEnd, pRangeList);
        any = FALSE;
        }
    if (!any)
        runStart = qLo;
    runEnd = qHi;
    any = TRUE;
    }
if (any)
    addSourceRun(chain, runStart, runEnd, pRangeList);
}

boolean quickLiftMultiChainEnabled()
/* Lift an item through every chain it overlaps, rather than only the best one.  hg.conf
 * quickLiftMultiChain, off by default.  refs #38510 */
/* Where multiRegionSpaces is on, the default is on.  refs #37788 */
{
static int enabled = -1;
if ((enabled < 0) && multiRegionSpacesEnabled())
    enabled = cfgOptionBooleanDefault("quickLiftMultiChain", TRUE);
if (enabled < 0)
    enabled = cfgOptionBooleanDefault("quickLiftMultiChain", FALSE);
return enabled;
}

boolean quickLiftSplitRangesEnabled()
/* Cut each chain's source range at large gaps, merge the ranges where they overlap, and
 * read an item only once when it crosses from one range into the next.  hg.conf
 * quickLiftSplitRanges, off by default.  refs #38510 */
/* Where multiRegionSpaces is on, the default is on.  refs #37788 */
{
static int enabled = -1;
if ((enabled < 0) && multiRegionSpacesEnabled())
    enabled = cfgOptionBooleanDefault("quickLiftSplitRanges", TRUE);
if (enabled < 0)
    enabled = cfgOptionBooleanDefault("quickLiftSplitRanges", FALSE);
return enabled;
}

/* With multi-chain lifting each chain's items are loaded on their own, and the chain an
 * item came from is remembered here, inside the track's own chain hash, under a key no
 * sequence can have.  The chain hash belongs to one track, so this is safe when tracks
 * load in parallel.  Items are keyed by address, which is unique while they are alive,
 * and they are looked up only before anything frees them. */
#define QUICKLIFT_MULTI_KEY "\tquickLiftMulti"

struct quickLiftMulti
/* Which chain each loaded item came from, and the one-chain hash an item is lifted through. */
    {
    struct hash *chainById;     /* chain id -> chain, as it sits in the chain hash */
    struct hash *itemChain;     /* item address -> chain id */
    struct hash *oneChain;      /* holds at most one chain per sequence, the one last asked for */
    struct hash *oneChainBySeq; /* sequence -> the chain oneChain holds on it */
    };

static struct quickLiftMulti *quickLiftMultiFor(struct hash *chainHash)
/* The multi-chain record of this chain hash, made the first time it is asked for. */
{
struct quickLiftMulti *qm = hashFindVal(chainHash, QUICKLIFT_MULTI_KEY);
if (qm == NULL)
    {
    AllocVar(qm);
    qm->chainById = newHash(6);
    qm->itemChain = newHash(12);
    qm->oneChain = newHash(4);
    qm->oneChainBySeq = newHash(4);
    hashAdd(chainHash, QUICKLIFT_MULTI_KEY, qm);
    }
return qm;
}

static void addLiftChain(struct hash *chainHash, struct chain *chain)
/* Add an already swapped chain to chainHash, and with multi-chain lifting remember it by
 * its id. */
{
liftOverAddChainHash(chainHash, chain);
if (quickLiftMultiChainEnabled())
    {
    char key[32];
    safef(key, sizeof key, "%d", chain->id);
    hashStore(quickLiftMultiFor(chainHash)->chainById, key)->val = chain;
    }
}

static void assignItemChain(struct hash *chainHash, void *item, int chainId)
/* Remember that item was loaded for the chain with this id. */
{
char key[32];
safef(key, sizeof key, "%p", item);
hashAddInt(quickLiftMultiFor(chainHash)->itemChain, key, chainId);
}

static struct hash *oneChainHash(struct quickLiftMulti *qm, struct chain *chain)
/* The reusable one-chain hash of qm, holding chain on its sequence. */
{
struct chain *current = hashFindVal(qm->oneChainBySeq, chain->tName);
if (current != chain)
    {
    if (current != NULL)
        liftOverRemoveChainHash(qm->oneChain, current);
    liftOverAddChainHash(qm->oneChain, chain);
    hashReplace(qm->oneChainBySeq, chain->tName, chain);
    }
return qm->oneChain;
}

struct hash *quickLiftChainHashForItem(struct hash *chainHash, void *item)
/* The chain hash to lift item through:  with quickLiftMultiChain, a hash of just the chain
 * it was loaded for, when it was loaded that way, and otherwise chainHash itself.  The
 * one-chain hash is reused for every item, one chain swapped in for another, so a binKeeper
 * is built once per sequence rather than once per chain.  It holds the right chain only
 * until the next call, and it changes on a lookup, which is safe because each chain hash
 * belongs to one track loaded by one thread. */
{
if ((chainHash == NULL) || !quickLiftMultiChainEnabled())
    return chainHash;
struct quickLiftMulti *qm = hashFindVal(chainHash, QUICKLIFT_MULTI_KEY);
if (qm == NULL)
    return chainHash;
char key[32];
safef(key, sizeof key, "%p", item);
struct hashEl *hel = hashLookup(qm->itemChain, key);
if (hel == NULL)
    return chainHash;
safef(key, sizeof key, "%d", ptToInt(hel->val));
struct chain *chain = hashFindVal(qm->chainById, key);
if (chain == NULL)
    return chainHash;
return oneChainHash(qm, chain);
}

char *quickLiftRemapRange(struct hash *chainHash, void *item, char *chrom, int start, int end,
                          char strand, char **retChrom, int *retStart, int *retEnd,
                          char *retStrand)
/* Lift chrom:start-end through one chain:  with quickLiftMultiChain the one item was loaded
 * for, and otherwise the chain in chainHash with the most aligned bases in the range.  Given
 * all of the chains, liftOverRemapRange() drops a range as "Duplicated in new" when two of
 * them each cover a little of it, which the short chains beside a long one in a duplicated
 * region do.  remapBlockedBed() picks the best chain instead, and so does this. */
{
struct hash *liftHash = quickLiftChainHashForItem(chainHash, item);
if ((chainHash != NULL) && (liftHash == chainHash))
    {
    struct chain *best = liftOverChainForRange(chainHash, chrom, start, max(end, start + 1));
    if (best == NULL)
        return "Deleted in new";
    liftHash = oneChainHash(quickLiftMultiFor(chainHash), best);
    }
return liftOverRemapRange(liftHash, 0.0, chrom, start, end, strand, 0.001,
                          retChrom, retStart, retEnd, retStrand);
}

static int queryRangeCmp(const void *va, const void *vb)
/* Compare two queryRanges by chain, then chrom, then start. */
{
const struct quickLiftQueryRange *a = *((struct quickLiftQueryRange **)va);
const struct quickLiftQueryRange *b = *((struct quickLiftQueryRange **)vb);
int diff = a->chainId - b->chainId;
if (diff == 0)
    diff = strcmp(a->chrom, b->chrom);
if (diff == 0)
    diff = a->start - b->start;
return diff;
}

static struct quickLiftQueryRange *wholeChainRanges(struct chain *chainList, int tStart,
                                                    int tEnd)
/* One source range per chain, from its first block in tStart..tEnd to its last, in chain
 * order and not merged:  the ranges the readers used before quickLiftSplitRanges. */
{
struct quickLiftQueryRange *qrList = NULL, *qr;
struct chain *chain;
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    int qStart, qEnd;
    if ((chain->blockList == NULL)
        || !quickLiftChainRangeIn(chain, tStart, tEnd, &qStart, &qEnd))
        continue;
    AllocVar(qr);
    qr->chrom = cloneString(chain->qName);
    qr->start = qStart;
    qr->end = qEnd;
    qr->chainId = chain->id;
    slAddHead(&qrList, qr);
    }
slReverse(&qrList);
return qrList;
}

struct quickLiftQueryRange *quickLiftQueryRanges(struct chain *chainList, int tStart, int tEnd,
                                                 boolean perChain, boolean split)
/* The source ranges that map into tStart..tEnd.  With split they are cut at large gaps in
 * each chain, sorted, and merged where they overlap:  with perChain each chain keeps its
 * own ranges, so its items are read once for it, and otherwise the ranges of all the
 * chains are merged, so no item is read twice.  Without split there is one range per
 * chain, as the readers had before. */
{
if (!split)
    return wholeChainRanges(chainList, tStart, tEnd);
struct quickLiftQueryRange *qrList = NULL, *qr, *next, *mergedList = NULL;
struct chain *chain;
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    if (chain->blockList == NULL)
        continue;
    struct quickLiftRange *run, *runList = NULL;
    quickLiftChainSourceRuns(chain, tStart, tEnd, &runList);
    for (run = runList; run != NULL; run = run->next)
        {
        AllocVar(qr);
        qr->chrom = run->chrom;         /* ownership moves to the queryRange */
        qr->start = run->start;
        qr->end = run->end;
        qr->chainId = perChain ? chain->id : 0;
        slAddHead(&qrList, qr);
        }
    slFreeList(&runList);
    }
slSort(&qrList, queryRangeCmp);
for (qr = qrList; qr != NULL; qr = next)
    {
    next = qr->next;
    if ((mergedList != NULL) && (mergedList->chainId == qr->chainId) &&
        sameString(mergedList->chrom, qr->chrom) && (qr->start <= mergedList->end))
        {
        mergedList->end = max(mergedList->end, qr->end);
        freeMem(qr->chrom);
        freeMem(qr);
        }
    else
        slAddHead(&mergedList, qr);
    }
slReverse(&mergedList);
return mergedList;
}

static boolean sameQueryGroup(boolean split, struct quickLiftQueryRange *prev,
                              struct quickLiftQueryRange *qr)
/* Were these consecutive ranges split and merged by quickLiftQueryRanges for the same chain
 * and sequence, so that an item starting before prev's end was already returned by prev?
 * Without split every range is a whole chain's, read as the readers always read them. */
{
return split && (prev != NULL) && (prev->chainId == qr->chainId)
    && sameString(prev->chrom, qr->chrom);
}

static boolean chainInWindow(struct chain *chain, int tStart, int tEnd)
/* Does chain have a block in tStart..tEnd, so that it maps something into it? */
{
int qStart, qEnd;
return (chain->blockList != NULL) && quickLiftChainRangeIn(chain, tStart, tEnd, &qStart, &qEnd);
}

void quickLiftQueryRangeFreeList(struct quickLiftQueryRange **pList)
/* Free a list of queryRanges. */
{
struct quickLiftQueryRange *qr;
for (qr = *pList; qr != NULL; qr = qr->next)
    freeMem(qr->chrom);
slFreeList(pList);
}

struct bigBedInterval *quickLiftGetIntervals(char *quickLiftFile, struct bbiFile *bbi,   char *chrom, int start, int end, struct hash **pChainHash)
/* Return intervals from "other" species that will map to the current window.
 * These intervals are NOT YET MAPPED to the current assembly.
 */
{
char *linkFileName = bigChainGetLinkFile(quickLiftFile);
int maxGapBefore = 0;
int maxGapAfter = 0;
struct chain *chain, *chainList = chainLoadIdRangeHub(NULL, quickLiftFile, linkFileName, chrom, start, end, -1);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = NULL, *bb;

// With quickLiftSplitRanges the source ranges, cut at large gaps in each chain and merged
// where they overlap, are read in order, so that an item crossing from one range into the
// next is kept from the first only.  With multi-chain lifting each chain's items are read
// for it, and each item is lifted through the chain it was read for.
boolean multi = quickLiftMultiChainEnabled();
boolean split = quickLiftSplitRangesEnabled();
struct quickLiftQueryRange *qr, *prev = NULL;
struct quickLiftQueryRange *qrList = quickLiftQueryRanges(chainList, start, end, multi, split);
if (multi && (*pChainHash == NULL))
    *pChainHash = newHash(0);
for (qr = qrList; qr != NULL; prev = qr, qr = qr->next)
    {
    int qStart = qr->start, qEnd = qr->end;

    // now grab the items , probably we should parameterize the max number of items, but
    // to what?
    struct bigBedInterval *thisInterval = bigBedIntervalQuery(bbi, qr->chrom, qStart, qEnd,
                                                              1000000, lm);
    struct bigBedInterval *next, *keepList = NULL;
    boolean dropEarly = sameQueryGroup(split, prev, qr);

    // find how much of the items are beyond the viewport
    for(bb=thisInterval; bb; bb = next)
        {
        next = bb->next;
        if (dropEarly && (bb->start < prev->end))
            continue;
        slAddHead(&keepList, bb);
        if (multi)
            assignItemChain(*pChainHash, bb, qr->chainId);
        if (bb->start < qStart)
            {
            int gap = qStart - bb->start;
            if (gap > maxGapBefore)
                maxGapBefore = gap;
            }
        if (bb->end > qEnd)
            {
            int gap = bb->end - qEnd;
            if (gap > maxGapAfter)
                maxGapAfter = gap;
            }
        }
    slReverse(&keepList);
    bbList = slCat(keepList, bbList);
    }
quickLiftQueryRangeFreeList(&qrList);

// We are done with the chains we used to bound the data query;
// release them before loading the wider set for the lift map below.
// Without this, every quickLifted track leaked the cBlocks of every
// chain overlapping the window.
chainFreeList(&chainList);

// now we need to grab the links outside of our viewport so we can map long items
// probably we could reuse the chains from above but for the moment this is easier
// For the moment we use the same padding on both sides so we don't have to worry about strand
if (maxGapBefore > maxGapAfter)
    maxGapAfter = maxGapBefore;
else
    maxGapBefore = maxGapAfter;

// Cap the padding so a single oversized item doesn't drag in chains
// (and their dense cBlock lists) covering many megabases.
#define QUICKLIFT_MAX_GAP_PAD 1000000
if (maxGapBefore > QUICKLIFT_MAX_GAP_PAD)
    maxGapBefore = maxGapAfter = QUICKLIFT_MAX_GAP_PAD;

int newStart = start - maxGapBefore * 2;
if (newStart < 0)
    newStart = 0;
int newEnd = end + maxGapAfter * 2;
chainList = chainLoadIdRangeHub(NULL, quickLiftFile, linkFileName, chrom, newStart, newEnd, -1);
for(chain = chainList; chain; chain = chain->next)
    {
    chainSwap(chain);

    if (*pChainHash == NULL)
        *pChainHash = newHash(0);
    addLiftChain(*pChainHash, chain);
    }

return bbList;
}

static void make12(struct bed *bed)
/* Make a bed12 out of something less than that. */
{
bed->blockCount = 1;
bed->blockSizes = needMem(sizeof(int));
bed->blockSizes[0] = bed->chromEnd - bed->chromStart;
bed->chromStarts = needMem(sizeof(int));
bed->chromStarts[0] = 0;
}

static int snapToBlockLinearInSpace(struct chain *chain, int pos, boolean forward)
/* remapRangeList will only place a coordinate that falls inside an aligned block, so a
 * clipped end has to land on real alignment, not merely inside the chain.  Return pos if
 * it is already aligned, otherwise the nearest aligned coordinate looking forward (for a
 * start) or backward (for an end).  Return -1 if there is no such coordinate.
 * The asymmetry matches remapRangeList: a start needs b->tStart <= start < b->tEnd, so
 * b->tStart is legal; an end needs b->tStart < end <= b->tEnd, so b->tEnd is legal. */
{
struct cBlock *b, *prev = NULL;

for (b = chain->blockList; b != NULL; b = b->next)
    {
    if (forward)
        {
        if ((b->tStart <= pos) && (pos < b->tEnd))
            return pos;
        if (b->tStart > pos)
            return b->tStart;
        }
    else
        {
        /* An end at a block's first base has no aligned base before it in that block,
         * so it is not aligned:  it snaps back to the end of the block before (#38660). */
        if ((b->tStart < pos) && (pos <= b->tEnd))
            return pos;
        if (b->tEnd <= pos)
            prev = b;
        }
    }

return (!forward && (prev != NULL)) ? prev->tEnd : -1;
}

static int snapToBlockInSpace(struct chain *chain, int pos, boolean forward)
/* snapToBlockLinearInSpace()'s answer, by binary search over the chain's block index. */
{
struct blockIndex *bi = chainBlockIndex(chain);
if (!bi->sorted)
    return snapToBlockLinearInSpace(chain, pos, forward);
if (forward)
    {
    /* the first block ending past pos either holds pos or starts after it */
    int i = firstBlockEndingAfter(bi, pos, FALSE);
    if (i >= bi->count)
        return -1;
    return (bi->blocks[i]->tStart <= pos) ? pos : bi->blocks[i]->tStart;
    }
/* the first block ending at or past pos holds pos as an end if it starts before it;
 * otherwise the answer is the end of the block before that one */
int j = firstBlockEndingAfter(bi, pos, TRUE);
if ((j < bi->count) && (bi->blocks[j]->tStart < pos))
    return pos;
return (j > 0) ? bi->blocks[j-1]->tEnd : -1;
}

static struct cBlock *alignedBlockAt(struct chain *chain, int pos, boolean asStart)
/* The block of chain that holds pos, or NULL:  as a start, tStart <= pos < tEnd;  as an
 * end, tStart < pos <= tEnd.  The same test remapRangeList() uses. */
{
struct blockIndex *bi = chainBlockIndex(chain);
if (!bi->sorted)
    {
    struct cBlock *b;
    for (b = chain->blockList; b != NULL; b = b->next)
        if (asStart ? ((b->tStart <= pos) && (pos < b->tEnd))
                    : ((b->tStart < pos) && (pos <= b->tEnd)))
            return b;
    return NULL;
    }
int i = firstBlockEndingAfter(bi, pos, !asStart);
if (i >= bi->count)
    return NULL;
struct cBlock *b = bi->blocks[i];
return (asStart ? (b->tStart <= pos) : (b->tStart < pos)) ? b : NULL;
}

static boolean chainLiftAligned(struct chain *chain, int start, int end, char strand,
                                char **retChrom, int *retStart, int *retEnd, char *retStrand)
/* Lift start-end through chain when start and end both sit on aligned bases:  map each
 * end through the block that holds it.  This is the answer mapThroughChain() gives, without
 * walking or copying the chain's blocks, which in a multi-region space can number tens of
 * thousands.  Return FALSE if either end is not aligned.  *retChrom is a clone. */
{
struct cBlock *sb = alignedBlockAt(chain, start, TRUE);
struct cBlock *eb = alignedBlockAt(chain, end, FALSE);
if ((sb == NULL) || (eb == NULL) || (start >= end))
    return FALSE;
int qStart = sb->qStart + (start - sb->tStart);
int qEnd = eb->qStart + (end - eb->tStart);
if (chain->qStrand == '-')
    {
    int flip = chain->qSize - qEnd;
    qEnd = chain->qSize - qStart;
    qStart = flip;
    }
*retChrom = cloneString(chain->qName);
*retStart = qStart;
*retEnd = qEnd;
/* the strand flip remapRange() makes:  + and - swap, anything else stays */
if ((chain->qStrand == '-') && ((strand == '+') || (strand == '-')))
    strand = (strand == '+') ? '-' : '+';
*retStrand = strand;
return TRUE;
}

static int snapToBlock(struct chain *chain, int pos, boolean forward)
/* remapRangeList will only place a coordinate that falls inside an aligned block, so a
 * clipped end has to land on real alignment, not merely inside the chain.  Return pos if
 * it is already aligned, otherwise the nearest aligned coordinate looking forward (for a
 * start) or backward (for an end).  Return -1 if there is no such coordinate.
 * The asymmetry matches remapRangeList: a start needs b->tStart <= start < b->tEnd, so
 * b->tStart is legal; an end needs b->tStart < end <= b->tEnd, so b->tEnd is legal. */
{
if (multiRegionSpacesEnabled())
    return snapToBlockInSpace(chain, pos, forward);
struct cBlock *b, *prev = NULL;

for (b = chain->blockList; b != NULL; b = b->next)
    {
    if ((b->tStart <= pos) && (pos < b->tEnd))
        return pos;
    if (forward && (b->tStart > pos))
        return b->tStart;
    if (b->tEnd <= pos)
        prev = b;
    }

return (!forward && (prev != NULL)) ? prev->tEnd : -1;
}

static boolean clipBedToChains(struct hash *chainHash, struct bed *bed)
/* An item can be far bigger than the region we loaded chains for -- ClinVar has copy
 * number variants spanning most of a chromosome.  Its ends then sit where no chain
 * reaches, remapRangeList can place neither of them, and the whole item is dropped even
 * though the part on screen maps perfectly well.  Pull the ends in to the nearest aligned
 * base so the visible part can lift.  Return TRUE if the item was clipped. */
{
struct chain *chain = liftOverChainForRange(chainHash, bed->chrom,
                                            bed->chromStart, bed->chromEnd);
if (chain == NULL)
    return FALSE;               // nothing covers it, let it fail the way it used to

int newStart = snapToBlock(chain, bed->chromStart, TRUE);
int newEnd = snapToBlock(chain, bed->chromEnd, FALSE);

if ((newStart < 0) || (newEnd < 0) || (newStart >= newEnd))
    return FALSE;
if ((newStart == bed->chromStart) && (newEnd == bed->chromEnd))
    return FALSE;               // both ends already sit on alignment, nothing to do

/* Trim the blocks to the new range.  Blocks are in ascending order, so walk them and
 * keep the part that survives; a block entirely outside the range is dropped. */
if (bed->blockCount > 0)
    {
    int i, keep = 0;
    for (i = 0;  i < bed->blockCount;  ++i)
        {
        int bStart = bed->chromStart + bed->chromStarts[i];
        int bEnd = bStart + bed->blockSizes[i];

        if (bStart < newStart)
            bStart = newStart;
        if (bEnd > newEnd)
            bEnd = newEnd;
        if (bStart >= bEnd)
            continue;
        bed->chromStarts[keep] = bStart - newStart;
        bed->blockSizes[keep] = bEnd - bStart;
        keep++;
        }
    if (keep == 0)
        return FALSE;
    bed->blockCount = keep;
    }

bed->chromStart = newStart;
bed->chromEnd = newEnd;
if (bed->thickStart < newStart)
    bed->thickStart = newStart;
if (bed->thickEnd > newEnd)
    bed->thickEnd = newEnd;
if (bed->thickStart > bed->thickEnd)
    bed->thickStart = bed->thickEnd;

return TRUE;
}

static boolean clipBedToChainsInSpace(struct hash *chainHash, struct bed *bed, boolean keepIntron)
/* An item can be far bigger than the region we loaded chains for:  ClinVar has copy
 * number variants spanning most of a chromosome.  Its ends then sit where no chain
 * reaches, remapRangeList can place neither of them, and the whole item is dropped even
 * though the part on screen maps perfectly well.  Pull the ends in to the nearest aligned
 * base so the visible part can lift.  Return TRUE if the item was clipped.
 * If no block survives, the item is left alone unless keepIntron, which keeps the clipped
 * span with no blocks at all:  in a multi-region space that is a transcript whose only
 * part in view is an intron (#38660). */
{
struct chain *chain = quickLiftBestChain(chainHash, bed->chrom,
                                            bed->chromStart, bed->chromEnd);
if (chain == NULL)
    return FALSE;               // nothing covers it, let it fail the way it used to

int newStart = snapToBlock(chain, bed->chromStart, TRUE);
int newEnd = snapToBlock(chain, bed->chromEnd, FALSE);

if ((newStart < 0) || (newEnd < 0) || (newStart >= newEnd))
    return FALSE;
if ((newStart == bed->chromStart) && (newEnd == bed->chromEnd) && !keepIntron)
    return FALSE;               // both ends already sit on alignment, nothing to do

/* Trim the blocks to the new range.  Blocks are in ascending order, so walk them and
 * keep the part that survives; a block entirely outside the range is dropped.
 * In a multi-region space (keepIntron) a region can also end inside a block, leaving
 * that block's end in the gap to the next region, where remapRangeList cannot place it.
 * There each block's own ends are pulled in to aligned bases too, the way
 * quickLiftClipGenePredToBlocks() does it for a genePred (#38660). */
if (bed->blockCount > 0)
    {
    int i, keep = 0;
    for (i = 0;  i < bed->blockCount;  ++i)
        {
        int bStart = bed->chromStart + bed->chromStarts[i];
        int bEnd = bStart + bed->blockSizes[i];

        if (bStart < newStart)
            bStart = newStart;
        if (bEnd > newEnd)
            bEnd = newEnd;
        if (keepIntron && (bStart < bEnd))
            {
            bStart = snapToBlock(chain, bStart, TRUE);
            bEnd = snapToBlock(chain, bEnd, FALSE);
            if ((bStart < 0) || (bEnd < 0))
                continue;
            }
        if (bStart >= bEnd)
            continue;
        bed->chromStarts[keep] = bStart - newStart;
        bed->blockSizes[keep] = bEnd - bStart;
        keep++;
        }
    if ((keep == 0) && !keepIntron)
        return FALSE;
    bed->blockCount = keep;
    }

bed->chromStart = newStart;
bed->chromEnd = newEnd;
if (bed->thickStart < newStart)
    bed->thickStart = newStart;
if (bed->thickEnd > newEnd)
    bed->thickEnd = newEnd;
if (bed->thickStart > bed->thickEnd)
    bed->thickStart = bed->thickEnd;

return TRUE;
}

static struct bed *quickLiftBed(struct bbiFile *bbi, struct hash *chainHash, struct bigBedInterval *bb, boolean clip);
static struct bed *quickLiftBedInSpace(struct bbiFile *bbi, struct hash *chainHash,
                                       struct bigBedInterval *bb, boolean clip,
                                       boolean keepIntron, boolean *retOutside);

struct bed *quickLiftIntervalsToBed(struct bbiFile *bbi, struct hash *chainHash, struct bigBedInterval *bb)
/* Using chains stored in chainHash, port a bigBedInterval from another assembly to a bed
 * on the reference.
 */
{
return quickLiftBed(bbi, chainHash, bb, FALSE);
}

struct bed *quickLiftIntervalsToBedKeepIntron(struct bbiFile *bbi, struct hash *chainHash,
                                              struct bigBedInterval *bb, boolean *retOutside)
/* Like quickLiftIntervalsToBedClip, for a multi-region space:  an item whose only part in
 * the space is between its blocks comes back with no blocks, which draws as an intron
 * line, where the other lifts drop it.  An item with no aligned base anywhere in its range
 * lies outside the space:  it comes back NULL with *retOutside set, and is not a failed
 * lift.  refs #38660 */
{
if (!multiRegionSpacesEnabled())
    {
    if (retOutside != NULL)
        *retOutside = FALSE;
    return quickLiftIntervalsToBedClip(bbi, chainHash, bb);
    }
boolean clip = cfgOptionBooleanDefault("quickLiftClipToChains", TRUE);
/* Keeping an intron picks a chain over the whole item, introns included;  that is only
 * sure to be the chain the blocks lift through when each item has one chain of its own.
 * quickLiftClipToChains=off keeps no introns either:  the outside test still runs. */
boolean keepIntron = clip && quickLiftMultiChainEnabled();

return quickLiftBedInSpace(bbi, chainHash, bb, clip, keepIntron, retOutside);
}

struct bed *quickLiftIntervalsToBedClip(struct bbiFile *bbi, struct hash *chainHash, struct bigBedInterval *bb)
/* Like quickLiftIntervalsToBed, but an item too big for the chains we loaded is pulled in
 * to what they cover rather than dropped.  Callers that need the item's true extent (the
 * details page) should use quickLiftIntervalsToBed instead. */
{
// quickLiftClipToChains=off restores the old behavior, where an item whose ends fall
// outside the chains we loaded is dropped instead of being pulled in.
boolean clip = cfgOptionBooleanDefault("quickLiftClipToChains", TRUE);

return quickLiftBed(bbi, chainHash, bb, clip);
}

static struct bed *liftIntronOnlyBed(struct hash *chainHash, struct bigBedInterval *bb,
                                     struct bed *bed)
/* Lift a bed that clipBedToChains() left with no blocks by its span alone.  Return bed, or
 * NULL if the span does not lift. */
{
char *newChrom = NULL;
int newStart = 0, newEnd = 0;
char newStrand = bed->strand[0];
if (quickLiftRemapRange(chainHash, bb, bed->chrom, bed->chromStart, bed->chromEnd,
                        bed->strand[0], &newChrom, &newStart, &newEnd, &newStrand) != NULL)
    {
    bedFree(&bed);
    return NULL;
    }
freeMem(bed->chrom);
bed->chrom = newChrom;
bed->strand[0] = newStrand;
bed->chromStart = newStart;
bed->chromEnd = newEnd;
bed->thickStart = bed->thickEnd = newEnd;       /* nothing coding is in view */
return bed;
}

static struct bed *liftInsertionBed(struct hash *chainHash, struct bed *bed)
/* Lift a zero-length bed as a point through chainHash, the item's own chains.  Return bed,
 * or NULL if it does not lift.  The point sits before source base p, and is placed by the
 * lift of that base:  at its start, or on a reversed region at its end, since the base
 * before p then lands after it.  refs #38660 */
{
int p = bed->chromStart;
struct chain *chain = quickLiftBestChain(chainHash, bed->chrom, p, p + 1);
char *newChrom = NULL;
int newStart = 0, newEnd = 0;
char newStrand = bed->strand[0];
if ((chain == NULL)
    || !chainLiftAligned(chain, p, p + 1, bed->strand[0],
                         &newChrom, &newStart, &newEnd, &newStrand))
    {
    bedFree(&bed);
    return NULL;
    }
int pos = (chain->qStrand == '-') ? newEnd : newStart;
freeMem(bed->chrom);
bed->chrom = newChrom;
bed->strand[0] = newStrand;
bed->chromStart = bed->chromEnd = pos;
bed->thickStart = bed->thickEnd = pos;
if (bed->blockCount > 0)
    {
    bed->blockCount = 1;
    bed->chromStarts[0] = 0;
    bed->blockSizes[0] = 0;
    }
return bed;
}

static void widenBedToSpan(struct hash *chainHash, void *item, struct bed *bed, char *chrom,
                           int start, int end, char strand)
/* After the lift of a bed that came from chrom:start-end, widen it to how far that range
 * reaches in the space, so an item that runs out of a region draws its intron line to the
 * region's edge.  The blocks do not move.  refs #38660 */
{
char *spanChrom = NULL;
int spanStart = 0, spanEnd = 0;
char spanStrand = strand;
if (!quickLiftSnappedSpan(chainHash, item, chrom, start, end, strand,
                          &spanChrom, &spanStart, &spanEnd, &spanStrand))
    return;
if (sameString(spanChrom, bed->chrom))
    {
    if (spanStart < bed->chromStart)
        {
        int shift = bed->chromStart - spanStart;
        int i;
        for (i = 0;  i < bed->blockCount;  ++i)
            bed->chromStarts[i] += shift;
        bed->chromStart = spanStart;
        }
    if (spanEnd > bed->chromEnd)
        bed->chromEnd = spanEnd;
    }
freeMem(spanChrom);
}

static boolean rangeOutsideChains(struct hash *chainHash, char *chrom, int start, int end)
/* TRUE if no chain in chainHash has an aligned base in chrom:start-end. */
{
if (end <= start)
    end = start + 1;    /* an insertion point:  ask about the base it sits before */
struct chain *chain = quickLiftBestChain(chainHash, chrom, start, end);
if (chain == NULL)
    return TRUE;
int alignedStart = snapToBlock(chain, start, TRUE);
int alignedEnd = snapToBlock(chain, end, FALSE);
return ((alignedStart < 0) || (alignedEnd < 0) || (alignedStart >= alignedEnd));
}

static struct bed *quickLiftBed(struct bbiFile *bbi, struct hash *chainHash, struct bigBedInterval *bb, boolean clip)
/* Port a bigBedInterval to a bed on the reference.  If clip, an item too big for the
 * chains we loaded is pulled in to what they cover rather than dropped. */
{
char startBuf[16], endBuf[16];
char *bedRow[bbi->fieldCount];
char chromName[256];
static int lastChromId = -1;

bbiCachedChromLookup(bbi, bb->chromId, lastChromId, chromName, sizeof(chromName));

bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));

struct bed *bed = bedLoadN(bedRow, bbi->definedFieldCount);
char *error;
if (bbi->definedFieldCount < 12)
    make12(bed);

chainHash = quickLiftChainHashForItem(chainHash, bb);
if (clip)
    clipBedToChains(chainHash, bed);

if ((error = remapBlockedBed(chainHash, bed, 0.0, 0.1, TRUE, TRUE, NULL, NULL)) == NULL)
    return bed;
//else
    //printf("bed %s error:%s<BR>", bed->name, error);

return NULL;
}

static struct bed *quickLiftBedInSpace(struct bbiFile *bbi, struct hash *chainHash,
                                       struct bigBedInterval *bb, boolean clip,
                                       boolean keepIntron, boolean *retOutside)
/* Port a bigBedInterval to a bed on the reference.  If clip, an item too big for the
 * chains we loaded is pulled in to what they cover rather than dropped.  If keepIntron,
 * one whose only part in view is between its blocks is kept with no blocks.  If
 * retOutside is non-NULL, an item with no aligned base at all returns NULL and sets it. */
{
if (retOutside != NULL)
    *retOutside = FALSE;
char startBuf[16], endBuf[16];
char *bedRow[bbi->fieldCount];
char chromName[256];
static int lastChromId = -1;

bbiCachedChromLookup(bbi, bb->chromId, lastChromId, chromName, sizeof(chromName));

bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));

struct bed *bed = bedLoadN(bedRow, bbi->definedFieldCount);
char *error;
if (bbi->definedFieldCount < 12)
    make12(bed);

struct hash *allChains = chainHash;
char *origChrom = cloneString(bed->chrom);
int origStart = bed->chromStart, origEnd = bed->chromEnd;
char origStrand = bed->strand[0];
chainHash = quickLiftChainHashForItem(chainHash, bb);
if ((retOutside != NULL) && rangeOutsideChains(chainHash, bed->chrom, origStart, origEnd))
    {
    /* Loaded through a padded query range, but no part of it is in the space. */
    *retOutside = TRUE;
    freeMem(origChrom);
    bedFree(&bed);
    return NULL;
    }
if (keepIntron && (origStart == origEnd))
    {
    /* An insertion point.  remapRangeList cannot place a zero-length block at the first
     * base of a region;  the range lift can, and the point keeps its zero length. */
    freeMem(origChrom);
    return liftInsertionBed(chainHash, bed);
    }
if (clip)
    clipBedToChainsInSpace(chainHash, bed, keepIntron);
if (keepIntron && (bed->blockCount == 0))
    {
    freeMem(origChrom);
    return liftIntronOnlyBed(allChains, bb, bed);
    }

if ((error = remapBlockedBed(chainHash, bed, 0.0, 0.1, TRUE, TRUE, NULL, NULL)) == NULL)
    {
    if (keepIntron)
        widenBedToSpan(allChains, bb, bed, origChrom, origStart, origEnd, origStrand);
    freeMem(origChrom);
    return bed;
    }
freeMem(origChrom);
//else
    //printf("bed %s error:%s<BR>", bed->name, error);

return NULL;
}

char *quickLiftGetChainPath(struct cart *cart, char *fromDb, char *toDb)
/* Return the path from the quickLiftChain table for given assemblies. */
{
if (!quickLiftEnabled(cart))
    return 0;

struct sqlConnection *conn = hConnectCentral();
char query[2048];
sqlSafef(query, sizeof(query), "select q.path from %s q  where q.fromDb='%s' and q.toDb='%s'", quickLiftChainTable(), fromDb, toDb);
char *path = sqlQuickString(conn, query);

hDisconnectCentral(&conn);

return path;
}

unsigned quickLiftGetChainId(struct cart *cart, char *fromDb, char *toDb)
/* Return the id from the quickLiftChain table for given assemblies. */
{
if (!quickLiftEnabled(cart))
    return 0;

unsigned ret = 0;
struct sqlConnection *conn = hConnectCentral();
char query[2048];
// this needs to use the hg.conf setting
sqlSafef(query, sizeof(query), "select q.id from quickLiftChain q  where q.fromDb='%s' and q.toDb='%s'", fromDb, toDb);
char *geneId = sqlQuickString(conn, query);

hDisconnectCentral(&conn);

if (geneId)
    ret = atoi(geneId);

return ret;
}

#define QUICKLIFT_RANGE_PAD 100000

static struct chain *quickLiftLoadChains(char *quickLiftFile, char *chrom, int start, int end)
/* Load the chains from quickLiftFile that overlap a padded window around the
 * destination range. */
{
// A track can name the assembly it came from without naming a chain file, since nothing
// stops a hub from setting one of the pair and not the other.  With no chains there is
// nothing to lift, and every caller copes with an empty answer.
if (quickLiftFile == NULL)
    return NULL;

// need to add some padding to these coordinates
int padStart = start - QUICKLIFT_RANGE_PAD;
if (padStart < 0)
    padStart = 0;

char *linkFileName = bigChainGetLinkFile(quickLiftFile);
return chainLoadIdRangeHub(NULL, quickLiftFile, linkFileName, chrom, padStart,
    end + QUICKLIFT_RANGE_PAD, -1);
}

static struct quickLiftRange *sourceRangesPadded(char *quickLiftFile, char *chrom,
    int start, int end, int pad, struct hash *chainHash)
// The ranges in the other assembly that map into chrom:start-end on the reference, with the
// window widened by pad on each side first.  The chains are added to chainHash.  With no
// pad the ranges are exact, and with quickLiftSplitRanges a chain's range is also cut at
// every large gap.
{
struct chain *chain, *chainList = quickLiftLoadChains(quickLiftFile, chrom, start, end);
struct quickLiftRange *rangeList = NULL;

for(chain = chainList; chain; chain = chain->next)
    {
    if (chain->blockList == NULL)
        continue;

    int qStart, qEnd;
    int padStart = start - pad;
    if (padStart < 0)
        padStart = 0;
    // a chain with no block in the padded window maps nothing into it, and quickLiftSql
    // leaves it out of the chain hash, so leave it out here too:  a lift on a details page
    // then goes through the chains hgTracks drew with.  refs #38512
    if ((pad == 0) && quickLiftSplitRangesEnabled())
        {
        struct quickLiftRange *before = rangeList;
        quickLiftChainSourceRuns(chain, start, end, &rangeList);
        if (rangeList == before)
            continue;
        }
    else
        {
        if (!quickLiftChainRangeIn(chain, padStart, end + pad, &qStart, &qEnd))
            continue;
        struct quickLiftRange *range;
        AllocVar(range);
        range->chrom = cloneString(chain->qName);
        range->start = qStart;
        range->end = qEnd;
        slAddHead(&rangeList, range);
        }

    // the query range was read off the chain as it came, so swap only afterwards
    chainSwap(chain);
    liftOverAddChainHash(chainHash, chain);
    }
slReverse(&rangeList);
return rangeList;
}

struct quickLiftRange *quickLiftSourceRanges(char *quickLiftFile, char *chrom, int start, int end,
    struct hash *chainHash)
// The ranges in the other assembly that map into chrom:start-end on the reference.  The
// chains that do the mapping are added to chainHash, which is the form the lift functions
// read.  Use this when the items cannot be had from a query quickLiftSql knows how to make.
{
// pad the window the same way quickLiftLoadChains does, so an item that reaches into
// the window from just outside it is still found
return sourceRangesPadded(quickLiftFile, chrom, start, end, QUICKLIFT_RANGE_PAD, chainHash);
}

struct quickLiftRange *quickLiftSourceRangesExact(char *quickLiftFile, char *chrom, int start,
    int end, struct hash *chainHash)
// Like quickLiftSourceRanges, but only the source bases that map into the window itself,
// with no padding.  Right for anything read by a range query that returns every item
// overlapping the range, such as maf blocks and their summaries:  an item that lands in the
// window overlaps these bases, and the padding would only read items that land elsewhere,
// 200 kb of extra alignment for every source range.  refs #38513
{
return sourceRangesPadded(quickLiftFile, chrom, start, end, 0, chainHash);
}

static int quickLiftRangeCmp(const void *va, const void *vb)
/* Compare two quickLiftRanges by chrom, then start. */
{
const struct quickLiftRange *a = *((struct quickLiftRange **)va);
const struct quickLiftRange *b = *((struct quickLiftRange **)vb);
int diff = strcmp(a->chrom, b->chrom);
if (diff == 0)
    diff = a->start - b->start;
return diff;
}

struct quickLiftRange *quickLiftSourceRangesMerged(char *quickLiftFile, char *chrom, int start,
    int end, struct hash *chainHash)
// The ranges from quickLiftSourceRangesExact sorted, with overlapping ones merged, so that
// two ranges never ask for the same rows twice.
{
struct quickLiftRange *range, *next, *mergedList = NULL;
struct quickLiftRange *rangeList = quickLiftSourceRangesExact(quickLiftFile, chrom,
                                                              start, end, chainHash);
slSort(&rangeList, quickLiftRangeCmp);
for (range = rangeList; range != NULL; range = next)
    {
    next = range->next;
    if ((mergedList != NULL) && sameString(mergedList->chrom, range->chrom) &&
        (range->start <= mergedList->end))
        {
        mergedList->end = max(mergedList->end, range->end);
        freeMem(range->chrom);
        freeMem(range);
        }
    else
        slAddHead(&mergedList, range);
    }
slReverse(&mergedList);
return mergedList;
}

void quickLiftRangeListFree(struct quickLiftRange **pList)
// Free a list of quickLiftRanges and their chroms.
{
struct quickLiftRange *range, *next;
for (range = *pList; range != NULL; range = next)
    {
    next = range->next;
    freeMem(range->chrom);
    freeMem(range);
    }
*pList = NULL;
}

struct chainBlocks
/* A chain's blocks in an array, in order, for a binary search. */
    {
    int count;
    struct cBlock **blocks;
    };

static struct chainBlocks *chainBlocksFor(struct hash *blockCache, struct chain *chain)
/* The blocks of chain as an array, made the first time they are asked for. */
{
char key[32];
safef(key, sizeof key, "%p", chain);
struct chainBlocks *cb = hashFindVal(blockCache, key);
if (cb == NULL)
    {
    AllocVar(cb);
    cb->count = slCount(chain->blockList);
    AllocArray(cb->blocks, cb->count);
    struct cBlock *b;
    int i = 0;
    for (b = chain->blockList; b != NULL; b = b->next)
        cb->blocks[i++] = b;
    hashAdd(blockCache, key, cb);
    }
return cb;
}

void quickLiftBlockCacheFree(struct hash **pBlockCache)
// Free a blockCache from quickLiftMapToReference() or quickLiftMapToReferenceIn().  The chains
// are not freed.
{
struct hash *blockCache = *pBlockCache;
if (blockCache == NULL)
    return;
struct hashEl *hel, *helList = hashElListHash(blockCache);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    struct chainBlocks *cb = hel->val;
    freeMem(cb->blocks);
    freeMem(cb);
    }
hashElFreeList(&helList);
hashFree(pBlockCache);
}

struct quickLiftRange *quickLiftMapToReference(struct hash *chainHash, struct hash *blockCache,
                                               char *chrom, int start, int end)
// Map chrom:start-end in the other assembly onto the reference through the chains in
// chainHash, one piece for every aligned block it overlaps.  The pieces come back in
// reference coordinates, unsorted, and are freed with slFreeList after freeing each chrom.
// An item lifted this way is not required to map whole, which suits a summary or density
// row:  every part of it that lands on the reference is drawn where it lands.
{
struct quickLiftRange *pieceList = NULL;
struct binElement *el, *elList = liftOverChainsInRange(chainHash, chrom, start, end);

for (el = elList; el != NULL; el = el->next)
    {
    struct chain *chain = el->val;
    struct chainBlocks *cb = chainBlocksFor(blockCache, chain);

    // the chains were swapped going into the hash, so t is the other assembly;  the blocks
    // are in order on t and do not overlap, so find the first one that ends after start
    int lo = 0, hi = cb->count;
    while (lo < hi)
        {
        int mid = (lo + hi) / 2;
        if (cb->blocks[mid]->tEnd <= start)
            lo = mid + 1;
        else
            hi = mid;
        }
    int i;
    for (i = lo; (i < cb->count) && (cb->blocks[i]->tStart < end); i++)
        {
        struct cBlock *b = cb->blocks[i];
        int s = max(start, b->tStart);
        int e = min(end, b->tEnd);
        if (s >= e)
            continue;
        int qs = b->qStart + (s - b->tStart);
        int qe = qs + (e - s);
        struct quickLiftRange *piece;
        AllocVar(piece);
        piece->chrom = cloneString(chain->qName);
        if (chain->qStrand == '-')
            {
            piece->start = chain->qSize - qe;
            piece->end = chain->qSize - qs;
            }
        else
            {
            piece->start = qs;
            piece->end = qe;
            }
        slAddHead(&pieceList, piece);
        }
    }
slFreeList(&elList);
return pieceList;
}

void quickLiftPieceToReference(struct quickLiftRange *piece, int srcStart, int srcEnd,
                               int *retStart, int *retEnd)
// Where srcStart-srcEnd, which lies inside a piece from quickLiftMapToReferenceIn, lands on
// the reference.  On a '-' piece the reference runs backwards.
{
// on a '-' piece the last source base comes first
int refStart = (piece->strand == '-') ? piece->end - (srcEnd - piece->sourceStart)
                                      : piece->start + (srcStart - piece->sourceStart);
*retStart = refStart;
*retEnd = refStart + (srcEnd - srcStart);
}

struct quickLiftRange *quickLiftMapToReferenceIn(struct hash *chainHash, struct hash *blockCache,
                                                 char *chrom, int start, int end,
                                                 char *refChrom, int refStart, int refEnd)
// Map chrom:start-end in the other assembly onto the reference through the chains in
// chainHash, one piece for every aligned block it overlaps, keeping only the pieces on
// refChrom that overlap refStart-refEnd.  The pieces are not clipped to it.  Unlike
// quickLiftMapToReference, each piece says where it came from (sourceStart and strand), for
// quickLiftPieceToReference.  Free them with quickLiftRangeListFree().
// The loop is a copy of quickLiftMapToReference's, kept apart so that function is unchanged
// with browser.quickLiftWig off;  a fix to one belongs in the other.  When the gate is
// retired, quickLiftMapToReference can become a call to this one.
{
struct quickLiftRange *pieceList = NULL;
struct binElement *el, *elList = liftOverChainsInRange(chainHash, chrom, start, end);

for (el = elList; el != NULL; el = el->next)
    {
    struct chain *chain = el->val;
    if (!sameString(chain->qName, refChrom))
        continue;
    struct chainBlocks *cb = chainBlocksFor(blockCache, chain);

    // the chains were swapped going into the hash, so t is the other assembly;  the blocks
    // are in order on t and do not overlap, so find the first one that ends after start
    int lo = 0, hi = cb->count;
    while (lo < hi)
        {
        int mid = (lo + hi) / 2;
        if (cb->blocks[mid]->tEnd <= start)
            lo = mid + 1;
        else
            hi = mid;
        }
    int i;
    for (i = lo; (i < cb->count) && (cb->blocks[i]->tStart < end); i++)
        {
        struct cBlock *b = cb->blocks[i];
        int s = max(start, b->tStart);
        int e = min(end, b->tEnd);
        if (s >= e)
            continue;
        int qs = b->qStart + (s - b->tStart);
        int qe = qs + (e - s);
        int pieceStart = (chain->qStrand == '-') ? chain->qSize - qe : qs;
        int pieceEnd = (chain->qStrand == '-') ? chain->qSize - qs : qe;
        if ((pieceStart >= refEnd) || (pieceEnd <= refStart))
            continue;
        struct quickLiftRange *piece;
        AllocVar(piece);
        piece->chrom = cloneString(chain->qName);
        piece->start = pieceStart;
        piece->end = pieceEnd;
        piece->sourceStart = s;
        piece->strand = chain->qStrand;
        slAddHead(&pieceList, piece);
        }
    }
slFreeList(&elList);
return pieceList;
}

struct hash *quickLiftChainHash(char *quickLiftFile, char *chrom, int start, int end)
// Load the quickLift chains covering chrom:start-end on the reference and return them in a
// hash keyed on the other assembly's sequence names, which is the shape the lift functions
// want.  Use this when the items were fetched some other way, so quickLiftSql was not the
// thing that collected the chains.
{
struct hash *chainHash = newHash(8);

quickLiftSourceRanges(quickLiftFile, chrom, start, end, chainHash);
return chainHash;
}

char *quickLiftRangeWhere(struct dyString *dy, char *startField, int prevEnd, char *extraWhere)
/* The extraWhere for reading a range that follows one ending at prevEnd on the same
 * sequence:  the caller's own, plus leaving out the items the previous range returned.
 * It is built in dy, which is emptied first.  An extraWhere that is only an order or
 * limit clause has to stay at the end. */
{
if (startField == NULL)
    return extraWhere;
dyStringClear(dy);
if (extraWhere == NULL)
    sqlDyStringPrintf(dy, "%s >= %d", startField, prevEnd);
else if (startsWith(NOSQLINJ "order", extraWhere) || startsWith(NOSQLINJ "limit", extraWhere))
    sqlDyStringPrintf(dy, "%s >= %d %-s", startField, prevEnd, extraWhere);
else
    sqlDyStringPrintf(dy, "(%-s) and %s >= %d", extraWhere, startField, prevEnd);
return dyStringContents(dy);
}

static char *tableStartField(struct sqlConnection *conn, char *table, char *buf)
/* The name of table's start field in buf, or NULL if it cannot be found. */
{
char chromField[HDB_MAX_FIELD_STRING], endField[HDB_MAX_FIELD_STRING];
if (!hFindChromStartEndFields(sqlGetDatabase(conn), table, chromField, buf, endField))
    return NULL;
return buf;
}

struct slList *quickLiftSql(struct sqlConnection *conn, char *quickLiftFile, char *table, char *chrom, int start, int end,  char *query, char *extraWhere, ItemLoader2 loader, int numFields,struct hash *chainHash)
// retrieve items for which we have a loader from a SQL database for which we have a set quickLift chains.
// Save the chains we used to map the item back to the current reference.
{
struct chain *chain, *chainList = quickLiftLoadChains(quickLiftFile, chrom, start, end);

struct slList *item, *itemList = NULL;
int rowOffset = 0;
struct sqlResult *sr = NULL;
char **row = NULL;

// Only the parts of the chains that map into the padded window, not the whole extent of
// their blocks, which between two similar assemblies can be millions of bases.  With
// quickLiftSplitRanges the ranges are merged so that no item is read twice, or with
// multi-chain lifting kept per chain so that each item is read once for every chain it
// lifts through.
int padStart = start - QUICKLIFT_RANGE_PAD;
if (padStart < 0)
    padStart = 0;
boolean multi = quickLiftMultiChainEnabled();
boolean split = quickLiftSplitRangesEnabled();
struct quickLiftQueryRange *qr, *prev = NULL;
struct quickLiftQueryRange *qrList = quickLiftQueryRanges(chainList, padStart,
                                                          end + QUICKLIFT_RANGE_PAD, multi,
                                                          split);
char startBuf[HDB_MAX_FIELD_STRING];
char *startField = (split && (query == NULL)) ? tableStartField(conn, table, startBuf) : NULL;
struct dyString *whereDy = dyStringNew(0);

for (qr = qrList; qr != NULL; prev = qr, qr = qr->next)
    {
    // a fixed query does not depend on the range, so with split ranges it is run once for
    // each chain with multi-chain lifting and once in all without it;  with whole-chain
    // ranges it is run once for each chain's one range, as it always was
    if ((query != NULL) && split && (prev != NULL) && (prev->chainId == qr->chainId))
        continue;

    // now grab the items
    if (query == NULL)
        {
        char *where = extraWhere;
        if (sameQueryGroup(split, prev, qr))
            where = quickLiftRangeWhere(whereDy, startField, prev->end, extraWhere);
        sr = hRangeQuery(conn, table, qr->chrom, qr->start, qr->end, where, &rowOffset);
        }
    else
        sr = sqlGetResult(conn, query);

    // numFields is what the loader will read, so it is also the least the row can have.
    // The native loaders check this; without it a table of the wrong type walks off the
    // end of the row.
    if ((numFields > 0) && (sqlCountColumns(sr) < numFields + rowOffset))
        errAbort("table %s in %s has %d columns, need at least %d",
                 table, sqlGetDatabase(conn), sqlCountColumns(sr), numFields + rowOffset);

    while ((row = sqlNextRow(sr)) != NULL)
        {
        item = loader(row + rowOffset, numFields);
        slAddHead(&itemList, item);
        if (multi)
            assignItemChain(chainHash, item, qr->chainId);
        }
    sqlFreeResult(&sr);
    }
quickLiftQueryRangeFreeList(&qrList);
dyStringFree(&whereDy);

// now squirrel the swapped chains we used to use to make the retrieved items back to us;
// a chain with no block in the padded window maps nothing into it and is left out
for(chain = chainList; chain; chain = chain->next)
    {
    if (!chainInWindow(chain, padStart, end + QUICKLIFT_RANGE_PAD))
        continue;
    chainSwap(chain);
    addLiftChain(chainHash, chain);
    }

return itemList;
}

struct genePred *quickLiftGenePreds(struct sqlConnection *conn, char *quickLiftFile, char *table, char *chrom, int start, int end, char *extraWhere, struct hash *chainHash)
// Like quickLiftSql, but load genePreds with a genePredReader so the actual set
// of (extended) genePred columns in the table is honored.  A fixed 15-column
// loader misreads classic knownGene-style tables, whose trailing proteinID and
// alignID columns are not extended genePred fields.
{
struct chain *chain, *chainList = quickLiftLoadChains(quickLiftFile, chrom, start, end);

struct genePred *gpList = NULL;

// the ranges are read the way quickLiftSql reads them
int padStart = start - QUICKLIFT_RANGE_PAD;
if (padStart < 0)
    padStart = 0;
boolean multi = quickLiftMultiChainEnabled();
boolean split = quickLiftSplitRangesEnabled();
struct quickLiftQueryRange *qr, *prev = NULL;
struct quickLiftQueryRange *qrList = quickLiftQueryRanges(chainList, padStart,
                                                          end + QUICKLIFT_RANGE_PAD, multi,
                                                          split);
struct dyString *whereDy = dyStringNew(0);

for (qr = qrList; qr != NULL; prev = qr, qr = qr->next)
    {
    char *where = extraWhere;
    if (sameQueryGroup(split, prev, qr))
        where = quickLiftRangeWhere(whereDy, "txStart", prev->end, extraWhere);
    struct genePredReader *gpr = genePredReaderRangeQuery(conn, table, qr->chrom,
                                                          qr->start, qr->end, where);
    struct genePred *gp;
    while ((gp = genePredReaderNext(gpr)) != NULL)
        {
        slAddHead(&gpList, gp);
        if (multi)
            assignItemChain(chainHash, gp, qr->chainId);
        }
    genePredReaderFree(&gpr);
    }
quickLiftQueryRangeFreeList(&qrList);
dyStringFree(&whereDy);

// now squirrel the swapped chains we used to use to map the retrieved items back to us;
// a chain with no block in the padded window maps nothing into it and is left out
for(chain = chainList; chain; chain = chain->next)
    {
    if (!chainInWindow(chain, padStart, end + QUICKLIFT_RANGE_PAD))
        continue;
    chainSwap(chain);
    addLiftChain(chainHash, chain);
    }

return gpList;
}

void quickLiftCalcGenePreds(struct genePred *gpList, struct hash *chainHash,
                            double minMatch, double minBlocks, bool fudgeThick,
                            boolean multiple, bool preserveInput)
/* calcLiftOverGenePreds() for genePreds read by quickLiftGenePreds():  each one is lifted
 * through the chain it was read for when that is known, and through chainHash otherwise.
 * As there, a genePred that does not lift comes back with a NULL chrom. */
{
if (!quickLiftMultiChainEnabled())
    {
    calcLiftOverGenePreds(gpList, chainHash, minMatch, minBlocks, fudgeThick, NULL, NULL,
                          multiple, preserveInput);
    return;
    }
struct genePred *gp, *next;
for (gp = gpList; gp != NULL; gp = next)
    {
    next = gp->next;
    gp->next = NULL;
    calcLiftOverGenePreds(gp, quickLiftChainHashForItem(chainHash, gp), minMatch, minBlocks,
                          fudgeThick, NULL, NULL, multiple, preserveInput);
    gp->next = next;
    }
}

static int codingBases5Prime(struct genePred *gp, unsigned *starts, unsigned *ends, int count,
                            int pos)
/* How many coding bases of the exons starts/ends lie 5' of pos, in the direction of
 * transcription.  On '+' pos is the first coding base of an exon, on '-' one past its last. */
{
int i, total = 0;
for (i = 0; i < count; i++)
    {
    int s = max(starts[i], gp->cdsStart), e = min(ends[i], gp->cdsEnd);
    if (gp->strand[0] == '-')
        s = max(s, pos);
    else
        e = min(e, pos);
    if (e > s)
        total += e - s;
    }
return total;
}

int *quickLiftClipGenePredToBlocks(struct hash *chainHash, struct genePred *gp)
/* Before quickLiftCalcGenePreds() in a multi-region space:  pull each exon's ends in to the
 * nearest aligned base of the chain gp lifts through, and drop the exons with nothing aligned.
 * A region can end inside an exon, and liftOver places only an exon whose two ends are both
 * aligned.  Not for a caller that needs a gene's true extent, such as a details page.
 * Return, for each exon kept, how many coding bases of the whole gene lie 5' of its first
 * coding base (-1 for an exon with none), so that codons can keep their numbers;  NULL if
 * nothing was clipped. */
{
if (!multiRegionSpacesEnabled())
    return NULL;
struct chain *chain = quickLiftBestChain(chainHash, gp->chrom, gp->txStart, gp->txEnd);
if (chain == NULL)
    return NULL;
int count = gp->exonCount;
if (count <= 0)
    return NULL;	/* nothing to clip, and needMem(0) aborts */
unsigned *newStarts, *newEnds;
int *codonBase;
AllocArray(newStarts, count);
AllocArray(newEnds, count);
AllocArray(codonBase, count);
int i, keep = 0;
boolean clipped = FALSE;
for (i = 0; i < count; i++)
    {
    int newStart = snapToBlock(chain, gp->exonStarts[i], TRUE);
    int newEnd = snapToBlock(chain, gp->exonEnds[i], FALSE);
    if ((newStart < 0) || (newEnd < 0) || (newStart >= newEnd))
        {
        clipped = TRUE;
        continue;
        }
    if ((newStart != gp->exonStarts[i]) || (newEnd != gp->exonEnds[i]))
        clipped = TRUE;
    int codingStart = max(newStart, gp->cdsStart), codingEnd = min(newEnd, gp->cdsEnd);
    if (codingStart < codingEnd)
        {
        int pos = (gp->strand[0] == '-') ? codingEnd : codingStart;
        codonBase[keep] = codingBases5Prime(gp, gp->exonStarts, gp->exonEnds, count, pos);
        }
    else
        codonBase[keep] = -1;
    newStarts[keep] = newStart;
    newEnds[keep] = newEnd;
    keep++;
    }
if (clipped && (keep > 0))
    {
    for (i = 0; i < keep; i++)
        {
        gp->exonStarts[i] = newStarts[i];
        gp->exonEnds[i] = newEnds[i];
        if (gp->exonFrames != NULL)
            gp->exonFrames[i] = (codonBase[i] >= 0) ? codonBase[i] % 3 : -1;
        }
    }
freeMem(newStarts);
freeMem(newEnds);
if (!clipped || (keep == 0))
    {
    /* nothing to do, or nothing aligned:  then it fails the lift the way it used to */
    freeMem(codonBase);
    return NULL;
    }
gp->exonCount = keep;
gp->txStart = gp->exonStarts[0];
gp->txEnd = gp->exonEnds[keep - 1];
gp->cdsStart = max(gp->cdsStart, gp->txStart);
gp->cdsEnd = min(gp->cdsEnd, gp->txEnd);
if (gp->cdsStart > gp->cdsEnd)
    gp->cdsStart = gp->cdsEnd;
return codonBase;
}

boolean quickLiftSnappedSpan(struct hash *chainHash, void *item, char *chrom, int start,
                             int end, char strand, char **retChrom, int *retStart, int *retEnd,
                             char *retStrand)
/* In a multi-region space:  pull chrom:start-end in to its first and last aligned base on
 * the chain item lifts through, and lift that span.  For an item that runs out of a region,
 * this is how far its line reaches in the space, which the lift of its blocks alone stops
 * short of.  Return FALSE if no part of the range is aligned or it does not lift.
 * refs #38660 */
{
/* Without quickLiftMultiChain the chain is picked over the whole span, introns included,
 * and can be another region's than the one the item's blocks lift through. */
if (!multiRegionSpacesEnabled() || !quickLiftMultiChainEnabled())
    return FALSE;
struct hash *itemHash = quickLiftChainHashForItem(chainHash, item);
struct chain *chain = quickLiftBestChain(itemHash, chrom, start, end);
if (chain == NULL)
    return FALSE;
int spanStart = snapToBlock(chain, start, TRUE);
int spanEnd = snapToBlock(chain, end, FALSE);
if ((spanStart < 0) || (spanEnd < 0) || (spanStart >= spanEnd))
    return FALSE;
return chainLiftAligned(chain, spanStart, spanEnd, strand,
                        retChrom, retStart, retEnd, retStrand);
}

boolean quickLiftIntronOnlyGenePred(struct hash *chainHash, struct genePred *gp)
/* Before quickLiftCalcGenePreds() in a multi-region space:  if no exon of gp has an aligned
 * base but part of the span between its exons does, lift that part by itself and leave gp
 * with no exons, which draws as an intron line.  The old multi-region view drew such a
 * transcript, and liftOver cannot place a genePred with nothing aligned in its exons.
 * Return TRUE if gp was lifted here:  it is then in the space's coordinates and must not
 * go through quickLiftCalcGenePreds().  FALSE leaves gp untouched.  refs #38660 */
{
if (!multiRegionSpacesEnabled() || !quickLiftMultiChainEnabled())
    return FALSE;       /* see quickLiftSnappedSpan() */
struct hash *itemHash = quickLiftChainHashForItem(chainHash, gp);
struct chain *chain = quickLiftBestChain(itemHash, gp->chrom, gp->txStart, gp->txEnd);
if (chain == NULL)
    return FALSE;
int i;
for (i = 0; i < gp->exonCount; i++)
    {
    int exonStart = snapToBlock(chain, gp->exonStarts[i], TRUE);
    int exonEnd = snapToBlock(chain, gp->exonEnds[i], FALSE);
    if ((exonStart >= 0) && (exonEnd >= 0) && (exonStart < exonEnd))
        return FALSE;   /* an exon is in the space, so the ordinary lift handles it */
    }

char *newChrom = NULL;
int newStart = 0, newEnd = 0;
char newStrand = gp->strand[0];
if (!quickLiftSnappedSpan(chainHash, gp, gp->chrom, gp->txStart, gp->txEnd, gp->strand[0],
                          &newChrom, &newStart, &newEnd, &newStrand))
    return FALSE;

freeMem(gp->chrom);
gp->chrom = newChrom;
gp->strand[0] = newStrand;
gp->txStart = newStart;
gp->txEnd = newEnd;
gp->cdsStart = gp->cdsEnd = newEnd;     /* nothing coding is in view */
gp->exonCount = 0;
return TRUE;
}

struct bed *quickLiftBeds(struct bed *bedList, struct hash *chainHash, boolean blocked)
// Map a list of bedd in query coordinates to our current reference
{
struct bed *liftedBedList = NULL;
struct bed *nextBed;
struct bed *bed;
for(bed = bedList; bed; bed = nextBed)
    {
    // remapBlockedBed may want to add new beds after this bed if the region maps to more than one location
    nextBed = bed->next;
    bed->next = NULL;

    char *error;
    if (!blocked)
        {
        error = quickLiftRemapRange(chainHash, bed, bed->chrom, bed->chromStart,
                                    bed->chromEnd, bed->strand[0], &bed->chrom,
                                    (int *)&bed->chromStart, (int *)&bed->chromEnd,
                                    &bed->strand[0]);

        // probably this should keep track of cases where the input does NOT have thickStart == chromStart
        bed->thickStart = bed->chromStart;
        bed->thickEnd = bed->chromEnd;
        }
    else
        error = remapBlockedBed(quickLiftChainHashForItem(chainHash, bed), bed, 0.0, 0.1,
                                TRUE, TRUE, NULL, NULL);

    if (error == NULL)
        {
        slAddHead(&liftedBedList, bed);
        }
    }
return liftedBedList;
}

static long pslAlignedBases(struct psl *psl)
/* Total size of the alignment's blocks, in whatever units the blocks are in. */
{
long total = 0;
int i;

for (i = 0; i < psl->blockCount; i++)
    total += psl->blockSizes[i];
return total;
}

static void quickLiftPslCounts(struct psl *psl, struct psl *lifted)
/* Put the original match, mismatch, repeat and N counts back on a lifted alignment,
 * scaled by how much of it survived the lift.  pslTransMap recounts them off the blocks,
 * which reads every lifted alignment as a perfect match:  the details page then claims
 * 100% identity and the browser draws every item at full shade. */
{
// A protein alignment comes back from the lift in nucleotide space, so its block sizes,
// and therefore its counts, are in different units than the ones we started with.
double protMul = (pslIsProtein(psl) && !pslIsProtein(lifted)) ? 3.0 : 1.0;
long origBases = pslAlignedBases(psl);
long newBases = pslAlignedBases(lifted);

if ((origBases <= 0) || (newBases <= 0))
    return;

double survived = newBases / (origBases * protMul);
if (survived > 1.0)
    survived = 1.0;

lifted->match = round(psl->match * protMul * survived);
lifted->misMatch = round(psl->misMatch * protMul * survived);
lifted->repMatch = round(psl->repMatch * protMul * survived);
lifted->nCount = round(psl->nCount * protMul * survived);
}

static struct psl *mapPslForChain(struct hash **pMapPsls, struct chain *chain)
/* The mapping alignment for one chain, made once and kept.  The chains in a quickLift
 * chainHash have the other assembly on the target side, which is what remapBlockedBed
 * wants.  pslTransMap wants it the other way round: query on the other assembly, target
 * on the reference.
 * Building this per item costs nothing at gene zoom, where a chain covers a handful of
 * blocks, and a great deal zoomed out, where the chain covering the window carries
 * thousands of blocks and an alignment track can have hundreds of thousands of items. */
{
char key[32];

if (*pMapPsls == NULL)
    *pMapPsls = newHash(8);
safef(key, sizeof key, "%p", chain);

struct psl *mapPsl = hashFindVal(*pMapPsls, key);
if (mapPsl == NULL)
    {
    mapPsl = chainToPsl(chain);
    pslSwap(mapPsl, FALSE);
    hashAdd(*pMapPsls, key, mapPsl);
    }
return mapPsl;
}

static boolean quickLiftPslBackToProtein(struct psl *lifted)
/* pslTransMap puts a protein alignment into nucleotide space to do the mapping and leaves
 * it there, so the query start, end and size come back three times too large and the base
 * alignment view refuses the alignment ("size of rna X is 604, has changed since alignment
 * was performed when it was 1812").  Put the query side back into protein units.  Returns
 * FALSE, leaving the alignment alone, when the lift split a codon so the query side no
 * longer divides evenly. */
{
int i;

if ((lifted->qStart % 3) || (lifted->qEnd % 3) || (lifted->qSize % 3) ||
    (lifted->qBaseInsert % 3))
    return FALSE;
for (i = 0; i < lifted->blockCount; i++)
    if ((lifted->blockSizes[i] % 3) || (lifted->qStarts[i] % 3))
        return FALSE;

// A protein psl always has its query on the forward strand, "++" or "+-".  pslTransMap can
// hand back strand[0] == '-' (it reverse complements the input when the two alignments
// disagree about the shared sequence's strand), and "-+" would tell pslShow to reverse
// complement the protein as though it were DNA.  Turn it over so the minus lands on the
// target side, where the protein display expects it.
// pslRc makes the target strand explicit as it turns the alignment over, so the
// assignment below is the other half of this test, not something to do as well.
if (lifted->strand[0] == '-')
    pslRc(lifted);
else
    {
    // A protein psl carries the target strand explicitly, and pslTransMap normalized the
    // target onto the forward strand on the way out.
    lifted->strand[1] = '+';
    lifted->strand[2] = 0;
    }

lifted->qStart /= 3;
lifted->qEnd /= 3;
lifted->qSize /= 3;
lifted->qBaseInsert /= 3;
for (i = 0; i < lifted->blockCount; i++)
    {
    lifted->blockSizes[i] /= 3;
    lifted->qStarts[i] /= 3;
    }
return TRUE;
}

static struct psl *pslWithoutEmptyBlocks(struct psl *psl)
/* A copy of a protein alignment with its zero-length blocks taken out, or NULL if it has
 * none.  The copy can be left with no blocks at all.  The UniProt bigPsl files store block sizes in bases, and a block shorter than a
 * codon comes out of pslFromBigPsl with size 0.  Drawing does not mind, but pslTransMap
 * checks the alignment after converting it to nucleotides and aborts on the empty block,
 * which takes down every item in the track.  About one alignment in eight has one.
 *
 * Dead since #38300: the UniProt files no longer declare amino acid coordinates, so
 * pslFromBigPsl stops dividing their block sizes by three and none come out empty. */
{
int i, j;

for (i = 0; i < psl->blockCount; i++)
    if (psl->blockSizes[i] == 0)
        break;
if (i == psl->blockCount)
    return NULL;

struct psl *copy = pslClone(psl);
for (i = 0, j = 0; i < copy->blockCount; i++)
    {
    if (copy->blockSizes[i] == 0)
        continue;
    copy->blockSizes[j] = copy->blockSizes[i];
    copy->qStarts[j] = copy->qStarts[i];
    copy->tStarts[j] = copy->tStarts[i];
    j++;
    }
copy->blockCount = j;
if (j == 0)
    return copy;

// Removing an end block moves the bounds, and pslIsProtein compares tEnd with the last
// block, so put them back in step.  pslRecalcBounds does not know about protein units.
int last = j - 1;
int tStart = copy->tStarts[0];
int tEnd = copy->tStarts[last] + 3 * copy->blockSizes[last];
if (copy->strand[1] == '-')
    reverseIntRange(&tStart, &tEnd, copy->tSize);
copy->tStart = tStart;
copy->tEnd = tEnd;
copy->qStart = copy->qStarts[0];
copy->qEnd = copy->qStarts[last] + copy->blockSizes[last];
pslComputeInsertCounts(copy);
return copy;
}

struct psl *quickLiftPsl(struct hash *chainHash, struct hash **pMapPsls, struct psl *psl)
// Map the target side of an alignment from the other assembly onto our current reference.
// The query side (the mRNA, EST or protein the alignment is to) is left alone.  Returns
// NULL if the alignment doesn't map.  pMapPsls points at a hash of mapping alignments the
// caller keeps across a run of items; point it at a NULL hash to start.
{
struct chain *chain = liftOverChainForRange(chainHash, psl->tName, psl->tStart, psl->tEnd);
if (chain == NULL)
    return NULL;

struct psl *mapPsl = mapPslForChain(pMapPsls, chain);

// pslTransMap aborts when the two alignments disagree about the size of the sequence they
// share.  That means the chain and the track were built against different versions of the
// other assembly, so drop the item rather than taking the CGI down with it.
if (psl->tSize != mapPsl->qSize)
    return NULL;

struct psl *trimmed = pslIsProtein(psl) ? pslWithoutEmptyBlocks(psl) : NULL;
if (trimmed && (trimmed->blockCount == 0))
    {
    pslFree(&trimmed);
    return NULL;
    }
struct psl *lifted = pslTransMap(pslTransMapNoOpts, trimmed ? trimmed : psl,
                                 pslTypeUnspecified, mapPsl, pslTypeUnspecified);
pslFree(&trimmed);
if (lifted != NULL)
    {
    // before counting, so quickLiftPslCounts sees both sides in the same units
    if (pslIsProtein(psl))
        quickLiftPslBackToProtein(lifted);
    quickLiftPslCounts(psl, lifted);
    }
return lifted;
}

static struct chain *chainFromPsl(struct psl *psl)
/* The inverse of chainToPsl.  Score and id are the caller's to fill in, since an alignment
 * does not carry them. */
{
struct chain *chain;
struct cBlock *blockList = NULL, *b;
int i;

AllocVar(chain);
chain->tName = cloneString(psl->tName);
chain->tSize = psl->tSize;
chain->tStart = psl->tStart;
chain->tEnd = psl->tEnd;
chain->qName = cloneString(psl->qName);
chain->qSize = psl->qSize;
chain->qStrand = psl->strand[0];

// chainToPsl turns the chain's query bounds the right way up for a psl, so turn them back
if (chain->qStrand == '-')
    {
    chain->qStart = psl->qSize - psl->qEnd;
    chain->qEnd = psl->qSize - psl->qStart;
    }
else
    {
    chain->qStart = psl->qStart;
    chain->qEnd = psl->qEnd;
    }

for (i = psl->blockCount - 1; i >= 0; i--)
    {
    AllocVar(b);
    b->tStart = psl->tStarts[i];
    b->tEnd = b->tStart + psl->blockSizes[i];
    b->qStart = psl->qStarts[i];
    b->qEnd = b->qStart + psl->blockSizes[i];
    slAddHead(&blockList, b);
    }
chain->blockList = blockList;
return chain;
}

static int mafRefStartCmp(const void *va, const void *vb)
/* Compare two maf blocks by where their first row starts. */
{
const struct mafAli *a = *((struct mafAli **)va);
const struct mafAli *b = *((struct mafAli **)vb);
return a->components->start - b->components->start;
}

static void quickLiftMafRefBases(struct mafAli *mafList, char *refDb, char *refChrom)
/* Put the reference assembly's own bases into the first row of each lifted block.  Inside a
 * chain block the two assemblies run in step, but they need not agree base for base, and
 * the row still carries the other assembly's letters.  The gaps stay where they are, since
 * the other rows are lined up against them.  The sequence is read once for the whole span. */
{
struct mafAli *maf;
int spanStart = INT_MAX, spanEnd = 0;

for (maf = mafList; maf != NULL; maf = maf->next)
    {
    struct mafComp *ref = maf->components;
    spanStart = min(spanStart, ref->start);
    spanEnd = max(spanEnd, ref->start + ref->size);
    }
if (spanStart >= spanEnd)
    return;

struct dnaSeq *seq = hDnaFromSeq(refDb, refChrom, spanStart, spanEnd, dnaMixed);
for (maf = mafList; maf != NULL; maf = maf->next)
    {
    struct mafComp *ref = maf->components;
    char *base = seq->dna + (ref->start - spanStart);
    int left = ref->size;   // a maf from a hub can claim fewer bases than its text holds
    char *text;
    for (text = ref->text; (*text != 0) && (left > 0); text++)
        if (*text != '-')
            {
            *text = *base++;
            left--;
            }
    }
dnaSeqFree(&seq);
}

struct mafAli *quickLiftMafs(struct hash *chainHash, struct mafAli *mafList,
    char *sourceDb, char *refDb, char *refChrom, char *refSrc, int refSrcSize)
// Map MAF blocks from the other assembly onto our current reference.
//
// A MAF block has to be one contiguous run on its first row, and the lift does not keep
// the reference contiguous:  where the reference assembly has lost bases the columns for
// them go away, and where it has gained bases the alignment says nothing about them.  So a
// block is cut at every chain block boundary.  Inside one chain block the two assemblies
// run in step, which is what lets the columns be carried over untouched:  only the first
// row's coordinates change, and mafSubset does the rest of the arithmetic.
//
// refSrc is the name the browser expects on the reference row, "<db>.<chrom>", with no hub
// prefix.  The bases on that row are read from refDb, which is the reference's real database
// name and may carry a hub prefix.  Blocks whose reference does not map are dropped.
{
struct mafAli *outList = NULL;
struct mafAli *maf, *nextMaf;

for (maf = mafList; maf != NULL; maf = nextMaf)
    {
    nextMaf = maf->next;
    maf->next = NULL;

    // The first row of a MAF is its reference, and a reference row is always forward.
    struct mafComp *ref = maf->components;
    if ((ref == NULL) || (ref->strand != '+') || (ref->size <= 0))
        {
        mafAliFree(&maf);
        continue;
        }

    // the chains are keyed on the sequence name in the other assembly
    // mafSplitSrcGetChrom writes into what it is given, so it needs a copy, and the copy
    // has to be allocated:  a maf component name comes from a hub and safecpy into a
    // fixed buffer would abort on a long one rather than truncate.
    char *srcBuf = cloneString(ref->src);
    char *srcChrom = mafSplitSrcGetChrom(srcBuf, sourceDb);
    int refStart = ref->start;
    int refEnd = refStart + ref->size;

    // With multi-chain lifting the block is cut out of every chain it overlaps, and
    // otherwise out of the best one.
    struct binElement *el, *elList = NULL;
    if (quickLiftMultiChainEnabled())
        elList = liftOverChainsInRange(chainHash, srcChrom, refStart, refEnd);
    else
        {
        struct chain *best = liftOverChainForRange(chainHash, srcChrom, refStart, refEnd);
        if (best != NULL)
            {
            AllocVar(elList);
            elList->val = best;
            }
        }

    for (el = elList; el != NULL; el = el->next)
        {
        struct chain *chain = el->val;
        struct cBlock *cb;
        for (cb = chain->blockList; cb != NULL; cb = cb->next)
            {
            int runStart = max(cb->tStart, refStart);
            int runEnd = min(cb->tEnd, refEnd);
            if (runStart >= runEnd)
                continue;

            struct mafAli *sub = mafSubset(maf, ref->src, runStart, runEnd);
            if (sub == NULL)
                continue;

            int destStart = cb->qStart + (runStart - cb->tStart);
            if (chain->qStrand == '-')
                {
                // The lift turns the block over, so turn every row over with it.  A chain
                // keeps its query side reverse complemented, so the forward start of the run
                // comes from the far end of it.
                mafFlipStrand(sub);
                destStart = chain->qSize - (cb->qStart + (runEnd - cb->tStart));
                }

            struct mafComp *subRef = sub->components;
            freeMem(subRef->src);
            subRef->src = cloneString(refSrc);
            subRef->srcSize = refSrcSize;
            subRef->strand = '+';
            subRef->start = destStart;
            slAddHead(&outList, sub);
            }
        }
    slFreeList(&elList);
    freeMem(srcBuf);
    mafAliFree(&maf);
    }
// a chain that turns the alignment over hands the blocks back last to first
slSort(&outList, mafRefStartCmp);
quickLiftMafRefBases(outList, refDb, refChrom);
return outList;
}

boolean quickLiftIsLifted(struct trackDb *tdb)
// TRUE when this track's data comes from another assembly and there is enough to lift it.
// Both halves have to be there:  the chain file that does the lifting and the assembly the
// data came from.  A hub can set either one on its own, and half the pair is no use.
{
return (tdb != NULL) &&
       (trackDbSetting(tdb, "quickLiftUrl") != NULL) &&
       (trackDbSetting(tdb, "quickLiftDb") != NULL);
}

boolean quickLiftIsOwnChainTrack(struct trackDb *tdb)
// TRUE when this is the chain track quickLift builds to show the lift itself.  That stanza
// carries quickLiftUrl and quickLiftDb like any lifted track, but its data is already in
// reference coordinates and must not be lifted a second time.  The giveaway is that its
// bigDataUrl IS the quickLift chain file.
{
char *quickLiftFile = trackDbSetting(tdb, "quickLiftUrl");

if (quickLiftFile == NULL)
    return FALSE;
if (startsWithNoCase("bigQuickLiftChain", tdb->type))
    return TRUE;

char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
return (bigDataUrl != NULL) && sameString(bigDataUrl, quickLiftFile);
}

struct chain *quickLiftChain(struct hash *chainHash, struct hash **pMapPsls, struct chain *chain)
// Map a chain's target side from the other assembly onto our current reference.  A chain is
// an alignment between that assembly and some other species, so this composes the two and
// leaves a chain between the reference and that species.  The query side is left alone.
// Returns NULL if the chain doesn't map.  The chain handed in is not modified.
{
// chainToPsl copies the header, and every chain loader leaves the header describing the
// whole chain while loading only the blocks that overlap the range asked for.  Correct it
// for the conversion, then put it back:  callers still want the whole-chain header, which
// is what the native details page reports.
int saveTStart = chain->tStart, saveTEnd = chain->tEnd;
int saveQStart = chain->qStart, saveQEnd = chain->qEnd;
struct cBlock *b = chain->blockList;

if (b == NULL)
    return NULL;

int tStart = b->tStart, tEnd = b->tEnd, qStart = b->qStart, qEnd = b->qEnd;
for (; b != NULL; b = b->next)
    {
    if (b->tStart < tStart)
        tStart = b->tStart;
    if (b->tEnd > tEnd)
        tEnd = b->tEnd;
    if (b->qStart < qStart)
        qStart = b->qStart;
    if (b->qEnd > qEnd)
        qEnd = b->qEnd;
    }
chain->tStart = tStart;
chain->tEnd = tEnd;
chain->qStart = qStart;
chain->qEnd = qEnd;

struct psl *psl = chainToPsl(chain);

chain->tStart = saveTStart;
chain->tEnd = saveTEnd;
chain->qStart = saveQStart;
chain->qEnd = saveQEnd;

struct psl *lifted = quickLiftPsl(chainHash, pMapPsls, psl);
pslFree(&psl);
if (lifted == NULL)
    return NULL;

struct chain *out = chainFromPsl(lifted);
out->score = chain->score;
out->id = chain->id;
pslFree(&lifted);
return out;
}

struct psl *quickLiftPsls(struct hash *chainHash, struct psl *pslList)
// Map a list of alignments in the other assembly's coordinates onto our current reference.
// Alignments that don't map are dropped.
{
struct psl *liftedList = NULL;
struct psl *psl, *nextPsl;
struct hash *mapPsls = NULL;

for(psl = pslList; psl; psl = nextPsl)
    {
    nextPsl = psl->next;
    psl->next = NULL;

    struct psl *lifted = quickLiftPsl(quickLiftChainHashForItem(chainHash, psl), &mapPsls, psl);
    if (lifted != NULL)
        slAddHead(&liftedList, lifted);
    pslFree(&psl);
    }
slReverse(&liftedList);
return liftedList;
}

struct encodePeak *quickLiftPeaks(struct encodePeak *peakList, struct hash *chainHash)
// Map a list of encodePeaks in query coordinates to our current reference.  These can't go
// through quickLiftBeds:  the thickStart and thickEnd it assigns overlay signalValue and
// pValue in struct encodePeak.
{
struct encodePeak *liftedList = NULL;
struct encodePeak *nextPeak;
struct encodePeak *peak;
for(peak = peakList; peak; peak = nextPeak)
    {
    nextPeak = peak->next;
    peak->next = NULL;

    char *error = quickLiftRemapRange(chainHash, peak, peak->chrom, peak->chromStart,
                                      peak->chromEnd, peak->strand[0], &peak->chrom,
                                      (int *)&peak->chromStart, (int *)&peak->chromEnd,
                                      &peak->strand[0]);

    if (error == NULL)
        slAddHead(&liftedList, peak);
    }
return liftedList;
}

boolean quickLiftEnabled(struct cart *cart)
/* Return TRUE if feature is available */
{
char *cfgEnabled = cartOrCfgOption(cart, "browser.quickLift");
return cfgEnabled && (sameString(cfgEnabled, "on") || sameString(cfgEnabled, "true")) ;
}

static int cartGate(struct cart *cart, char *name)
/* What the cart says about the gate name:  1 for on, 0 for off, -1 when it says nothing and
 * hg.conf decides.  The hg.conf half stays a literal cfgOptionBooleanDefault in each caller,
 * which is what the hg.conf catalog's harvester finds. */
{
char *cartEnabled = cartOptionalString(cart, name);
if (cartEnabled == NULL)
    return -1;
return sameString(cartEnabled, "on") || sameString(cartEnabled, "true") ||
       sameString(cartEnabled, "yes");
}

boolean quickLiftAlignmentsEnabled(struct cart *cart)
/* Return TRUE if quickLift is allowed to lift alignment tracks: psl, bigPsl, chain,
 * bigChain, maf, bigMaf and wigMaf.  Off unless hg.conf says
 * browser.quickLiftAlignments=on, and a cart variable of the same name overrides that so
 * one machine can show both answers.  The hg.conf half is read with a literal
 * cfgOptionBooleanDefault rather than cartOrCfgOption because harvestHgConf.py only sees
 * the cfgOption* accessors, which is why browser.quickLift itself is missing from the
 * hg.conf catalog. */
{
int fromCart = cartGate(cart, "browser.quickLiftAlignments");
if (fromCart >= 0)
    return fromCart;
return cfgOptionBooleanDefault("browser.quickLiftAlignments", FALSE);
}

boolean quickLiftWigTableOk(char *liftDb, char *table)
// TRUE if table in liftDb may be read as a lifted wig.  liftDb and table come from trackDb
// settings a hub can write, and a wiggle row names a file that gets opened, so liftDb has to
// be an assembly this server has in its own database (not customTrash, whose tables users
// fill, and not a hub's) and table a wiggle table in it.
{
if ((liftDb == NULL) || (table == NULL) || trackHubDatabase(liftDb) || !hDbExists(liftDb) ||
    !hTableExists(liftDb, table))
    return FALSE;
struct sqlConnection *conn = hAllocConn(liftDb);
boolean ok = (sqlFieldIndex(conn, table, "span") >= 0) &&
             (sqlFieldIndex(conn, table, "offset") >= 0) &&
             (sqlFieldIndex(conn, table, "file") >= 0) &&
             (sqlFieldIndex(conn, table, "lowerLimit") >= 0);
hFreeConn(&conn);
return ok;
}

boolean quickLiftWigEnabled(struct cart *cart)
/* Return TRUE if quickLift is allowed to lift wig tracks, the kind kept in a table and a
 * .wib file.  Off unless hg.conf says browser.quickLiftWig=on, and a cart variable of the
 * same name overrides that, the same way as quickLiftAlignmentsEnabled. */
{
int fromCart = cartGate(cart, "browser.quickLiftWig");
if (fromCart >= 0)
    return fromCart;
return cfgOptionBooleanDefault("browser.quickLiftWig", FALSE);
}

boolean quickLiftRmskEnabled(struct cart *cart)
/* Return TRUE if quickLift is allowed to lift rmsk (RepeatMasker) tracks.  Off unless hg.conf
 * says browser.quickLiftRmsk=on, and a cart variable of the same name overrides that, the
 * same way as quickLiftAlignmentsEnabled. */
{
int fromCart = cartGate(cart, "browser.quickLiftRmsk");
if (fromCart >= 0)
    return fromCart;
return cfgOptionBooleanDefault("browser.quickLiftRmsk", FALSE);
}

boolean quickLiftBarChartEnabled(struct cart *cart)
/* Return TRUE if quickLift is allowed to lift bigBarChart tracks.  Off unless hg.conf says
 * browser.quickLiftBarChart=on, and a cart variable of the same name overrides that, the
 * same way as quickLiftAlignmentsEnabled. */
{
int fromCart = cartGate(cart, "browser.quickLiftBarChart");
if (fromCart >= 0)
    return fromCart;
return cfgOptionBooleanDefault("browser.quickLiftBarChart", FALSE);
}

boolean quickLiftGtexEnabled(struct cart *cart)
/* Return TRUE if a quickLifted GTEx gene track is drawn as GTEx, with its bar charts, from
 * genes and gene models read out of the assembly it came from through the chain.  Off, it
 * is drawn as a plain bed.  Off unless hg.conf says browser.quickLiftGtex=on,
 * and a cart variable of the same name overrides that, the same way as
 * quickLiftBarChartEnabled.  refs #38512 */
{
int fromCart = cartGate(cart, "browser.quickLiftGtex");
if (fromCart >= 0)
    return fromCart;
return cfgOptionBooleanDefault("browser.quickLiftGtex", FALSE);
}

boolean quickLiftMafSummaryEnabled(struct cart *cart)
/* Return TRUE if a quickLifted maf track reads its summary table above the summary window
 * size, lifted from the assembly it came from, and reads its blocks with no padding around
 * the window.  Off unless hg.conf says browser.quickLiftMafSummary=on, and a cart variable
 * of the same name overrides that, the same way as quickLiftBarChartEnabled.  refs #38513 */
{
int fromCart = cartGate(cart, "browser.quickLiftMafSummary");
if (fromCart >= 0)
    return fromCart;
return cfgOptionBooleanDefault("browser.quickLiftMafSummary", FALSE);
}

boolean quickLiftIsLiftedGtex(struct cart *cart, struct trackDb *tdb)
/* Return TRUE if tdb is a quickLifted GTEx gene track to draw and click as GTEx:  it is
 * lifted, its name past the hub prefix starts with gtexGene, and browser.quickLiftGtex is on.
 * refs #38512 */
{
return quickLiftIsLifted(tdb) && startsWith("gtexGene", trackHubSkipHubName(tdb->track)) &&
       quickLiftGtexEnabled(cart);
}

boolean quickLiftIsLiftedRmskJoined(struct cart *cart, struct trackDb *tdb)
/* Return TRUE if tdb is a quickLifted RepeatMasker Viz. track (rmskJoined) to draw and click
 * as one:  it is lifted, its name past the hub prefix starts with rmskJoined, and
 * browser.quickLiftRmsk is on.  Off, it is drawn as the plain bed its type says. */
{
return quickLiftIsLifted(tdb) && startsWith("rmskJoined", trackHubSkipHubName(tdb->track)) &&
       quickLiftRmskEnabled(cart);
}

static int hrCmp(const void *va, const void *vb)
/* Compare to sort based on chromStart. */
{
const struct quickLiftRegions *a = *((struct quickLiftRegions **)va);
const struct quickLiftRegions *b = *((struct quickLiftRegions **)vb);
return a->chromStart - b->chromStart;
}

struct quickLiftRegions *getMismatches(char *ourDb, char strand, char *chrom, char *liftDb, char *liftChrom,  struct bigLink *bl, int querySize,  int seqStart, int seqEnd, char * chainId)
// Helper function to calculate mismatches in a bigLink block
{
struct quickLiftRegions *hrList = NULL, *hr;

int tStart = bl->chromStart;
int tEnd = bl->chromEnd;
int width = tEnd - tStart;
int qStart = bl->qStart;
int qEnd = qStart + width;

if (strand == '-')
    {
    int saveStart = qStart;
    qStart = querySize - qEnd;
    qEnd = querySize - saveStart;
    }

// grab that DNA
struct dnaSeq *tSeq = hDnaFromSeq(ourDb, chrom, tStart, tEnd, dnaUpper);
struct dnaSeq *qSeq = hDnaFromSeq(liftDb, liftChrom, qStart, qEnd, dnaUpper);
if (strand == '-')
    reverseComplement(qSeq->dna, qSeq->size);

// now step through looking for mismatches
char *tDna = tSeq->dna;
char *qDna = qSeq->dna;
unsigned tAddr = tStart;
unsigned qAddr = qStart;
for(; tAddr < tEnd; tAddr++, qAddr++, tDna++, qDna++)
    {
    if (tAddr < seqStart)
        continue;
    if (tAddr > seqEnd)
        break;
    if (*tDna != *qDna)
        {
        AllocVar(hr);
        slAddHead(&hrList, hr);
        hr->chrom = cloneString(chrom);
        hr->oChrom = cloneString(liftChrom);
        hr->chromStart = tAddr;
        hr->chromEnd = tAddr + 1;
        hr->oChromStart = qAddr;
        hr->oChromEnd = qAddr + 1;
        hr->bases = tDna;
        hr->otherBases = qDna;
        hr->baseCount = 1;
        hr->otherBaseCount = 1;
        hr->type = QUICKTYPE_MISMATCH;
        hr->id = chainId;
        }
    }
return hrList;
}

struct quickLiftRegions *fillWithGap(struct bigChain *bc, unsigned previousTEnd, unsigned tStart, unsigned previousQEnd, unsigned qStart)
{
struct quickLiftRegions *hr;

AllocVar(hr);
hr->id = bc->name;
hr->chrom = cloneString(bc->chrom);
hr->oChrom = cloneString(bc->qName);
hr->chromStart = previousTEnd;
hr->chromEnd = tStart;
if (bc->strand[0] == '-')
    {
    hr->oChromStart = bc->qSize - qStart;
    hr->oChromEnd = bc->qSize - previousQEnd;
    }
else
    {
    hr->oChromStart = previousQEnd;
    hr->oChromEnd = qStart;
    }
return hr;
}

struct quickLiftRegions *quickLiftGetRegions(char *ourDb, char *liftDb, char *quickLiftFile, char *chrom, int seqStart, int seqEnd)
/* Figure out the highlight regions and cache them. */
{
static struct hash *highLightsHash = NULL;
struct quickLiftRegions *hrList = NULL;

unsigned lengthLimit = atoi(cfgOptionDefault("quickLift.lengthLimit", "10000"));
if (seqEnd - seqStart > lengthLimit)
    return hrList;

if (highLightsHash != NULL)
    {
    if ((hrList = (struct quickLiftRegions *)hashFindVal(highLightsHash, quickLiftFile)) != NULL)
        return hrList;
    }
else
    {
    highLightsHash = newHash(0);
    }

struct bbiFile *bbiChain = bigBedFileOpenAlias(quickLiftFile, chromAliasFindAliases);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbChain, *bbChainList =  bigBedIntervalQuery(bbiChain, chrom, seqStart, seqEnd, 0, lm);
char *links = bigChainGetLinkFile(quickLiftFile);
struct bbiFile *bbiLink = bigBedFileOpenAlias(links, chromAliasFindAliases);
struct bigBedInterval  *bbLink, *bbLinkList =  bigBedIntervalQuery(bbiLink, chrom, seqStart, seqEnd, 0, lm);

char *chainRow[1024];
char *linkRow[1024];
char startBuf[16], endBuf[16];

for (bbChain = bbChainList; bbChain != NULL; bbChain = bbChain->next)
    {
    bigBedIntervalToRow(bbChain, chrom, startBuf, endBuf, chainRow, ArraySize(chainRow));
    struct bigChain *bc = bigChainLoad(chainRow);

    int previousTEnd = -1;
    int previousQEnd = -1;
    for (bbLink = bbLinkList; bbLink != NULL; bbLink = bbLink->next)
        {
        bigBedIntervalToRow(bbLink, chrom, startBuf, endBuf, linkRow, ArraySize(linkRow));
        struct bigLink *bl = bigLinkLoad(linkRow);

        if (!sameString(bl->name, bc->name))
            continue;

        int tStart = bl->chromStart;
        int tEnd = bl->chromEnd;
        int qStart = bl->qStart;
        int qEnd = qStart + (tEnd - tStart);

        struct quickLiftRegions *hr;

        if ((previousTEnd != -1) && (previousTEnd == tStart))
            {
            hr = fillWithGap(bc, previousTEnd, tStart, previousQEnd, qStart);
            slAddHead(&hrList, hr);
            hr->type = QUICKTYPE_DEL;
            struct dnaSeq *qSeq = NULL;
            if (bc->strand[0] == '-')
                {
                qSeq = hDnaFromSeq(liftDb, bc->qName, bc->qSize - hr->oChromEnd, bc->qSize - hr->oChromStart, dnaUpper);
                reverseComplement(qSeq->dna, qSeq->size);
                }
            else
                qSeq = hDnaFromSeq(liftDb, bc->qName, hr->oChromStart, hr->oChromEnd, dnaUpper);
            hr->otherBases = qSeq->dna;
            hr->otherBaseCount = hr->oChromEnd - hr->oChromStart;
            }
        else if ( (previousQEnd != -1) && (previousQEnd == qStart))
            {
            hr = fillWithGap(bc, previousTEnd, tStart, previousQEnd, qStart);
            slAddHead(&hrList, hr);
            hr->type = QUICKTYPE_INSERT;
            struct dnaSeq *tSeq = hDnaFromSeq(ourDb, chrom, hr->chromStart, hr->chromEnd, dnaUpper);
            hr->bases = tSeq->dna;
            hr->baseCount = hr->chromEnd - hr->chromStart;
            }
        else if ( ((previousQEnd != -1) && (previousQEnd != qStart)) 
             && ((previousTEnd != -1) && (previousTEnd != tStart)))
            {
            hr = fillWithGap(bc, previousTEnd, tStart, previousQEnd, qStart);
            hr->type = QUICKTYPE_DOUBLE;
            hr->baseCount = hr->chromEnd - hr->chromStart;
            hr->otherBaseCount = hr->oChromEnd - hr->oChromStart;
            slAddHead(&hrList, hr);
            }

        previousQEnd = qEnd;
        previousTEnd = tEnd;

        // now find the mismatches in this block
        struct quickLiftRegions *mismatches = getMismatches(ourDb, bc->strand[0], chrom, liftDb, bc->qName, bl, bc->qSize, seqStart, seqEnd, bc->name);
        hrList = slCat(mismatches, hrList);
        }
    }

slSort(&hrList,  hrCmp);
hashAdd(highLightsHash, quickLiftFile, hrList);

return hrList;
}

void quickLiftResolveTable(struct trackDb *tdb, char *trackTable, char **retTable, char **retLiftDb)
/* Resolve the table name and liftDb for a quickLift track.  For custom tracks,
 * sets *retLiftDb to CUSTOM_TRASH and *retTable to the dbTableName setting;
 * otherwise sets *retTable to trackTable. Caller should have already set
 * *retLiftDb to trackDbSetting(tdb, "quickLiftDb"). */
{
if (isCustomTrack(trackTable))
    {
    *retLiftDb = CUSTOM_TRASH;
    *retTable = trackDbSetting(tdb, "dbTableName");
    }
else
    *retTable = trackTable;
}

struct bed *quickLiftSqlLoadBeds(struct trackDb *tdb, char *trackTable, char *liftDb,
    char *chrom, int start, int end, char *extraWhere,
    ItemLoader2 loader, int numFields, boolean blocked)
/* Load items from another assembly via quickLift SQL, map them back to the reference,
 * and return the lifted beds that overlap chrom:start-end.  Handles custom track table
 * resolution internally.  Caller provides liftDb from trackDbSetting(tdb, "quickLiftDb"). */
{
char *table;
quickLiftResolveTable(tdb, trackTable, &table, &liftDb);
struct hash *chainHash = newHash(8);
struct sqlConnection *conn = hAllocConn(liftDb);
char *quickLiftFile = cloneString(trackDbSetting(tdb, "quickLiftUrl"));
struct bed *bed = (struct bed *)quickLiftSql(conn, quickLiftFile, table, chrom, start, end,
    NULL, extraWhere, loader, numFields, chainHash);
struct bed *liftedBeds = quickLiftBeds(bed, chainHash, blocked);
hFreeConn(&conn);

/* the chains are read for a padded window, so a bed can lift to somewhere off screen */
struct bed *nextBed, *inWindow = NULL;
for (bed = liftedBeds; bed != NULL; bed = nextBed)
    {
    nextBed = bed->next;
    if ((bed->chromEnd > start) && (bed->chromStart < end))
        slAddHead(&inWindow, bed);
    }
slReverse(&inWindow);
return inWindow;
}

/* The one-argument loader quickLiftSqlLoadItems was handed;  quickLiftSql wants a loader
 * that also takes a field count.  Each track is loaded by one thread. */
static __thread struct slList *(*rowLoader)(char **row);

static struct slList *rowLoaderAdapter(char **row, int numFields)
/* Call the one-argument loader. */
{
return rowLoader(row);
}

struct slList *quickLiftSqlLoadItems(struct trackDb *tdb, char *trackTable,
    char *chrom, int start, int end, char *extraWhere,
    struct slList *(*loader)(char **row),
    void (*adjust)(void *item, boolean flipped, int clippedStart), boolean keepSize)
/* Load items of any type whose struct begins like a struct bed3 (next, chrom, chromStart,
 * chromEnd) from the track's quickLiftDb, and lift those three fields in place to the
 * reference.  The rest of each item is left as loaded, so a track with its own loader
 * and drawing keeps both.  Items that do not lift, or lift outside chrom:start-end, are
 * dropped.  The list is not sorted.
 * An item is normally clipped to what lifts.  With keepSize it is instead placed whole
 * through the first aligned block it has in the window, so that positions counted from
 * its chromStart (blocks, an aligned part) stay right where it runs past a region's edge;
 * only a start before 0 is clipped, by clippedStart bases.
 * If adjust is not NULL it is called on each lifted item, with flipped TRUE when the lift
 * turned the item around, as one placed in reverse in a multi-region space is. */
{
char *liftDb = trackDbSetting(tdb, "quickLiftDb");
char *table;
quickLiftResolveTable(tdb, trackTable, &table, &liftDb);
struct hash *chainHash = newHash(8);
struct sqlConnection *conn = hAllocConn(liftDb);
char *quickLiftFile = cloneString(trackDbSetting(tdb, "quickLiftUrl"));
rowLoader = loader;
struct bed3 *item, *nextItem, *itemList = (struct bed3 *)quickLiftSql(conn, quickLiftFile,
    table, chrom, start, end, NULL, extraWhere, rowLoaderAdapter, 0, chainHash);
rowLoader = NULL;
hFreeConn(&conn);

struct bed3 *liftedList = NULL;
struct hash *blockCache = keepSize ? newHash(0) : NULL;
for (item = itemList; item != NULL; item = nextItem)
    {
    nextItem = item->next;
    struct hash *itemChainHash = quickLiftChainHashForItem(chainHash, item);
    char strand = '+';
    char *error = NULL;
    int clippedStart = 0;
    if (keepSize)
        {
        struct quickLiftRange *pieceList = quickLiftMapToReferenceIn(itemChainHash, blockCache,
                                 item->chrom, item->chromStart, item->chromEnd, chrom, start, end);
        if (pieceList == NULL)
            error = "not in window";
        else
            {
            int liftStart, liftEnd;
            quickLiftPieceToReference(pieceList, item->chromStart, item->chromEnd,
                                      &liftStart, &liftEnd);
            strand = pieceList->strand;
            quickLiftRangeListFree(&pieceList);
            if (liftStart < 0)
                {
                clippedStart = -liftStart;
                liftStart = 0;
                }
            item->chrom = cloneString(chrom);
            item->chromStart = liftStart;
            item->chromEnd = liftEnd;
            }
        }
    else
        error = liftOverRemapRange(itemChainHash, 0.0, item->chrom, item->chromStart,
                                   item->chromEnd, '+', 0.001, &item->chrom,
                                   (int *)&item->chromStart, (int *)&item->chromEnd, &strand);
    /* the chains are read for a padded window, so an item can lift to somewhere off screen */
    if ((error == NULL) && ((item->chromEnd <= start) || (item->chromStart >= end)))
        error = "outside window";
    if (error == NULL)
        {
        if (adjust != NULL)
            adjust(item, (strand == '-'), clippedStart);
        slAddHead(&liftedList, item);
        }
    }
if (blockCache != NULL)
    quickLiftBlockCacheFree(&blockCache);
return (struct slList *)liftedList;
}

static struct slList *rmskOutLoadItem(char **row, int numFields)
/* quickLiftSql()'s loader signature around rmskOutLoad(). */
{
return (struct slList *)rmskOutLoad(row);
}

struct rmskOut *quickLiftSqlLoadRmsk(struct trackDb *tdb, char *trackTable, char *chrom,
    int start, int end, struct hash *chainHash)
/* Read the rows of a quickLifted rmsk track from the assembly it came from, over the source
 * ranges that map into chrom:start-end on the reference.  The rows are NOT lifted:  lift each
 * one with quickLiftRmskOut() through quickLiftChainHashForItem(chainHash, row).  chainHash,
 * from newHash(), is filled with the chains that do the lifting. */
{
char *liftDb = trackDbSetting(tdb, "quickLiftDb");
char *table;
quickLiftResolveTable(tdb, trackTable, &table, &liftDb);
struct sqlConnection *conn = hAllocConn(liftDb);
char *quickLiftFile = trackDbSetting(tdb, "quickLiftUrl");
// rmskOutLoad reads 16 columns, row[0] through row[15]
struct rmskOut *roList = (struct rmskOut *)quickLiftSql(conn, quickLiftFile, table, chrom,
    start, end, NULL, NULL, rmskOutLoadItem, 16, chainHash);
hFreeConn(&conn);
return roList;
}

boolean quickLiftRmskOut(struct hash *chainHash, struct rmskOut *ro)
/* Lift an rmsk row read from the assembly it came from onto the reference, in place:
 * genoName, genoStart, genoEnd and strand.  repStart, repEnd and repLeft are left in the
 * convention of the source strand, so save ro->strand first if they are to be read.  Returns
 * FALSE, with the row unchanged, if it does not map. */
{
char *liftedChrom;
int liftedStart, liftedEnd;
char liftedStrand;
if (liftOverRemapRange(chainHash, 0.0, ro->genoName, ro->genoStart, ro->genoEnd, ro->strand[0],
                       0.001, &liftedChrom, &liftedStart, &liftedEnd, &liftedStrand) != NULL)
    return FALSE;
// liftOverRemapRange hands back its own copy of the chrom name
freeMem(ro->genoName);
ro->genoName = liftedChrom;
ro->genoStart = liftedStart;
ro->genoEnd = liftedEnd;
ro->strand[0] = liftedStrand;
return TRUE;
}

static struct slList *rmskJoinedLoadItem(char **row, int numFields)
/* quickLiftSql()'s loader signature around rmskJoinedLoad(). */
{
return (struct slList *)rmskJoinedLoad(row);
}

struct rmskJoined *quickLiftSqlLoadRmskJoined(struct trackDb *tdb, char *trackTable,
    char *chrom, int start, int end, struct hash *chainHash)
/* Read the rows of a quickLifted rmskJoined track from the assembly it came from, over the
 * source ranges that map into chrom:start-end on the reference.  The rows are NOT lifted:
 * lift each one with quickLiftJoinedRepeat() through quickLiftChainHashForItem(chainHash, row).
 * chainHash, from newHash(), is filled with the chains that do the lifting. */
{
char *liftDb = trackDbSetting(tdb, "quickLiftDb");
char *table;
quickLiftResolveTable(tdb, trackTable, &table, &liftDb);
struct sqlConnection *conn = hAllocConn(liftDb);
char *quickLiftFile = trackDbSetting(tdb, "quickLiftUrl");
// rmskJoinedLoad reads 13 columns, row[0] through row[12]
struct rmskJoined *rmList = (struct rmskJoined *)quickLiftSql(conn, quickLiftFile, table, chrom,
    start, end, NULL, NULL, rmskJoinedLoadItem, 13, chainHash);
hFreeConn(&conn);
return rmList;
}

static void addUnaligned(int *blockSizes, int *blockRelStarts, int *pCount, int size)
/* Add an unaligned block of size to the blocks written so far, joining it to the one before
 * if that is unaligned too. */
{
int n = *pCount;
if ((n > 0) && (blockRelStarts[n-1] < 0))
    blockSizes[n-1] += size;
else
    {
    blockSizes[n] = size;
    blockRelStarts[n] = -1;
    *pCount = n + 1;
    }
}

boolean quickLiftJoinedRepeat(struct hash *chainHash, char **pChrom, unsigned *pChromStart,
    unsigned *pChromEnd, char *strand, unsigned *pAlignStart, unsigned *pAlignEnd,
    int *pBlockCount, int *blockSizes, int *blockRelStarts)
/* Lift a joined RepeatMasker item (an rmskJoined row or a bigRmsk record) onto the reference,
 * in place.  Each aligned block (blockRelStarts not -1) is lifted on its own;  the unaligned
 * blocks hold consensus lengths, not genome positions, and are kept.  An aligned block that
 * does not map, or maps to another sequence or strand than the first one that does, becomes
 * part of the unaligned run beside it, so the blocks may get fewer.  On a '-' chain the
 * blocks are reversed and the strand flipped.  If no aligned block maps, the item's extent is
 * lifted instead, and what lands becomes a single unaligned block, with chromStart-chromEnd
 * and alignStart-alignEnd both set to it.  *pChrom is freed and replaced with a copy of the
 * reference chrom.  Returns FALSE, with the blocks partly rewritten, if there are no blocks,
 * nothing of the item maps, or the aligned blocks land out of order:  drop the item then. */
{
int blockCount = *pBlockCount;
if (blockCount <= 0)
    return FALSE;
// the unaligned flanks are drawn past the aligned part:  keep whatever they reach beyond
// their own sizes, which is how far the item's ends sit from the aligned part
int first = (blockRelStarts[0] < 0) ? max(blockSizes[0], 0) : 0;
int last = (blockRelStarts[blockCount-1] < 0) ? max(blockSizes[blockCount-1], 0) : 0;
int extraLeft = max((int)(*pAlignStart - *pChromStart) - first, 0);
int extraRight = max((int)(*pChromEnd - *pAlignEnd) - last, 0);
int *newStarts, *newEnds;
AllocArray(newStarts, blockCount);
AllocArray(newEnds, blockCount);
char *newChrom = NULL;
char newStrand = 0;
boolean ok = TRUE;
int i, n = 0;

// the blocks are rewritten in place:  n never passes i, so a block is read before it is
// written over
for (i = 0; i < blockCount; i++)
    {
    int size = blockSizes[i];
    if (blockRelStarts[i] < 0)
        {
        addUnaligned(blockSizes, blockRelStarts, &n, size);
        continue;
        }
    int start = *pChromStart + blockRelStarts[i];
    int end = start + size;
    char *liftChrom;
    char liftStrand;
    int liftStart, liftEnd;
    boolean lifted = (end > start) &&
        (liftOverRemapRange(chainHash, 0.0, *pChrom, start, end, '+', 0.001, &liftChrom,
                            &liftStart, &liftEnd, &liftStrand) == NULL);
    boolean keep = lifted &&
        ((newChrom == NULL) || (sameString(newChrom, liftChrom) && (newStrand == liftStrand)));
    // liftOverRemapRange hands back its own copy of the chrom name:  keep the first block's
    // and free the rest
    if (keep && (newChrom == NULL))
        newChrom = liftChrom;
    else if (lifted)
        freeMem(liftChrom);
    if (keep)
        {
        newStrand = liftStrand;
        blockSizes[n] = size;
        blockRelStarts[n] = 0;
        newStarts[n] = liftStart;
        newEnds[n] = liftEnd;
        n++;
        }
    else
        addUnaligned(blockSizes, blockRelStarts, &n, size);
    }
if (newChrom == NULL)
    {
    // no aligned block lands, but the flanks may:  the native track draws them as the dashed
    // extension, so lift the item's own extent and keep what lands as one unaligned block.
    // alignStart-alignEnd covers it too, since the click box and the details page go by them.
    // refs #38613
    char *liftChrom;
    char liftStrand;
    int liftStart, liftEnd;
    ok = (*pChromEnd > *pChromStart) &&
        (liftOverRemapRange(chainHash, 0.0, *pChrom, *pChromStart, *pChromEnd, '+', 0.001,
                            &liftChrom, &liftStart, &liftEnd, &liftStrand) == NULL);
    if (ok)
        {
        if (liftStrand == '-')
            strand[0] = (strand[0] == '-') ? '+' : '-';
        blockSizes[0] = liftEnd - liftStart;
        blockRelStarts[0] = -1;
        *pBlockCount = 1;
        *pChromStart = *pAlignStart = liftStart;
        *pChromEnd = *pAlignEnd = liftEnd;
        freeMem(*pChrom);
        *pChrom = liftChrom;
        }
    freeMem(newStarts);
    freeMem(newEnds);
    return ok;
    }

if (ok)
    {
    if (newStrand == '-')
        {
        reverseInts(blockSizes, n);
        reverseInts(blockRelStarts, n);
        reverseInts(newStarts, n);
        reverseInts(newEnds, n);
        int tmp = extraLeft;
        extraLeft = extraRight;
        extraRight = tmp;
        strand[0] = (strand[0] == '-') ? '+' : '-';
        }

    // the aligned blocks have to stay in order on the reference
    int alignStart = 0, alignEnd = 0, prevStart = -1;
    for (i = 0; i < n; i++)
        {
        if (blockRelStarts[i] < 0)
            continue;
        if (newStarts[i] < prevStart)
            ok = FALSE;
        if (prevStart < 0)
            alignStart = newStarts[i];
        prevStart = newStarts[i];
        alignEnd = max(alignEnd, newEnds[i]);
        }

    if (ok)
        {
        first = (blockRelStarts[0] < 0) ? max(blockSizes[0], 0) : 0;
        last = (blockRelStarts[n-1] < 0) ? max(blockSizes[n-1], 0) : 0;
        *pAlignStart = alignStart;
        *pAlignEnd = alignEnd;
        *pChromStart = max(alignStart - first - extraLeft, 0);
        *pChromEnd = alignEnd + last + extraRight;
        for (i = 0; i < n; i++)
            {
            if (blockRelStarts[i] < 0)
                continue;
            blockRelStarts[i] = newStarts[i] - *pChromStart;
            blockSizes[i] = newEnds[i] - newStarts[i];
            }
        *pBlockCount = n;
        freeMem(*pChrom);
        *pChrom = newChrom;
        newChrom = NULL;
        }
    }
freeMem(newChrom);
freeMem(newStarts);
freeMem(newEnds);
return ok;
}

char *quickLiftChainTable()
/* Return the name of the quickLiftChain table. */
{
static char *quickLiftChainTable = NULL;
if (quickLiftChainTable == NULL)
    quickLiftChainTable = cfgOptionEnvDefault("QUICKLIFTCHAINNAME",
	    quickLiftChainTableConfVariable, defaultQuickLiftChainTableName);
return quickLiftChainTable;
}

boolean quickLiftLiftPos(char *sourceDb, char *destDb,
    char *chrom, int start, int end,
    char **retChrom, int *retStart, int *retEnd)
/* Map a position from source (sourceDb) coords to destination (destDb) coords
 * using the liftOver chain for sourceDb -> destDb.  This is used to remap
 * hgFind results from quickLifted bigBed tracks (which return hits in the
 * source assembly's coordinates) back to the destination assembly the user
 * is viewing.  Returns TRUE on success. */
{
static struct hash *fileToChainHash = NULL;
if (fileToChainHash == NULL)
    fileToChainHash = newHash(0);

char key[1024];
safef(key, sizeof(key), "%s->%s", sourceDb, destDb);
struct hash *chainHash = hashFindVal(fileToChainHash, key);
if (chainHash == NULL)
    {
    char *chainFile = liftOverChainFile(sourceDb, destDb);
    if (chainFile == NULL)
        return FALSE;
    chainHash = newHash(0);
    // This reads every chain in the file up front.  A bigChain-format
    // liftOver chain indexed on the source (fromDb) side would let us
    // load just the chains overlapping the hit; worth revisiting if the
    // upfront cost becomes an issue.
    readLiftOverMap(chainFile, chainHash);
    hashAdd(fileToChainHash, key, chainHash);
    }

char strand = '+';
char *error = liftOverRemapRange(chainHash, 0.0, chrom, start, end, strand,
                                 0.001, retChrom, retStart, retEnd, &strand);
return (error == NULL);
}
