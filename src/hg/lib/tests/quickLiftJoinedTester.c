/* quickLiftJoinedTester - check what quickLiftJoinedRepeat() makes of a joined RepeatMasker
 * item, the block layout shared by rmskJoined rows and bigRmsk records.
 *
 * An item is a run of blocks.  An aligned block has a relStart and a size in the genome.  An
 * unaligned block has relStart -1 and a size that is a consensus length, which can be
 * negative, and the drawing code expects unaligned and aligned blocks to alternate.  So each
 * case prints the lifted item and checks that it still has that shape:  chromStart <=
 * alignStart <= alignEnd <= chromEnd, every aligned block inside the item with a positive
 * size, and no two unaligned blocks side by side.
 *
 * The chains are in input/quickLift/joined.chain, read the way liftOver reads them, so
 * src1 is the assembly the item came from:
 *   1  src1 0-2000 -> ref1 1000-2900 '+', with src1 500-600 in a gap
 *   2  src1 3000-4000 -> ref2 '-' (ref2 is 5000 long, so src1 p lands near 5000-p)
 *   3  src1 5000-5200 -> ref1 10000-10200
 *   4  src1 5300-5500 -> ref1 9000-9200, so a block on 3 and a block on 4 land out of order
 *   5  src1 6000-6500 -> ref3 100-600
 * refs #37437 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "errCatch.h"
#include "jksql.h"
#include "cart.h"
#include "trackDb.h"
#include "liftOver.h"
#include "quickLift.h"

static void usage()
/* Explain usage and exit. */
{
errAbort("quickLiftJoinedTester - check quickLiftJoinedRepeat() on a small chain file\n"
         "usage:\n"
         "   quickLiftJoinedTester joined.chain\n");
}

static int badCount = 0;

static char *shapeProblem(unsigned chromStart, unsigned chromEnd, unsigned alignStart,
                          unsigned alignEnd, int blockCount, int inCount, int *sizes,
                          int *relStarts)
/* What is wrong with the shape of a lifted item, or NULL if nothing is. */
{
int i;
if (!((chromStart <= alignStart) && (alignStart <= alignEnd) && (alignEnd <= chromEnd)))
    return "ends out of order";
if ((blockCount < 1) || (blockCount > inCount))
    return "block count";
for (i = 0; i < blockCount; i++)
    {
    if (relStarts[i] < -1)
        return "relStart below -1";
    if ((relStarts[i] >= 0) && (sizes[i] <= 0))
        return "aligned block with no size";
    if ((relStarts[i] >= 0) && ((unsigned)(relStarts[i] + sizes[i]) > chromEnd - chromStart))
        return "aligned block past chromEnd";
    if ((i > 0) && (relStarts[i] < 0) && (relStarts[i-1] < 0))
        return "two unaligned blocks side by side";
    }
return NULL;
}

static void liftOne(struct hash *chainHash, char *label, char *chrom, unsigned chromStart,
                    unsigned chromEnd, char strand, unsigned alignStart, unsigned alignEnd,
                    int blockCount, int *sizesIn, int *relStartsIn)
/* Lift one item and print what came back.  The block arrays are copied to the heap at
 * exactly blockCount, so a memory checker sees a read or write past them. */
{
int *sizes, *relStarts;
int i;
AllocArray(sizes, max(blockCount, 1));
AllocArray(relStarts, max(blockCount, 1));
for (i = 0; i < blockCount; i++)
    {
    sizes[i] = sizesIn[i];
    relStarts[i] = relStartsIn[i];
    }
char *liftChrom = cloneString(chrom);
char liftStrand[2] = {strand, 0};
int liftCount = blockCount;
boolean ok = FALSE;

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    ok = quickLiftJoinedRepeat(chainHash, &liftChrom, &chromStart, &chromEnd, liftStrand,
                               &alignStart, &alignEnd, &liftCount, sizes, relStarts);
errCatchEnd(errCatch);

printf("%s:  ", label);
if (errCatch->gotError)
    printf("error %s", errCatch->message->string);
else if (!ok)
    printf("dropped\n");
else
    {
    printf("%s:%u-%u align %u-%u %s [", liftChrom, chromStart, chromEnd, alignStart, alignEnd,
           liftStrand);
    for (i = 0; i < liftCount; i++)
        printf("%s%d@%d", (i > 0) ? " " : "", sizes[i], relStarts[i]);
    printf("]");
    char *problem = shapeProblem(chromStart, chromEnd, alignStart, alignEnd, liftCount,
                                 blockCount, sizes, relStarts);
    if (problem != NULL)
        {
        printf("  BAD SHAPE: %s", problem);
        badCount++;
        }
    printf("\n");
    }
errCatchFree(&errCatch);
freeMem(liftChrom);
freeMem(sizes);
freeMem(relStarts);
}

/* One case:  the item, then its blocks as {size, relStart} pairs. */
#define LIFT(label, chrom, cs, ce, st, as, ae, ...) \
    do \
        { \
        int blocks[][2] = { __VA_ARGS__ }; \
        int n = ArraySize(blocks), sizes[ArraySize(blocks)], rels[ArraySize(blocks)], k; \
        for (k = 0; k < n; k++) \
            { \
            sizes[k] = blocks[k][0]; \
            rels[k] = blocks[k][1]; \
            } \
        liftOne(chainHash, label, chrom, cs, ce, st, as, ae, n, sizes, rels); \
        } \
    while (0)

static void quickLiftJoinedTester(char *chainFile)
/* Run every case through the chains in chainFile. */
{
struct hash *chainHash = newHash(8);
readLiftOverMap(chainFile, chainHash);

// The usual shape, flank, aligned, flank:  moved 1000 along chain 1.
LIFT("plus", "src1", 90, 410, '+', 100, 400, {10,-1}, {300,10}, {10,-1});

// Two fragments joined by a negative consensus gap.  The second fragment runs into the
// chain gap at src1 500-600, so it lifts to the part outside the gap.
LIFT("two fragments", "src1", 50, 1050, '+', 100, 1000,
     {50,-1}, {300,50}, {-20,-1}, {550,450}, {50,-1});

// No flank at one end.
LIFT("no left flank", "src1", 100, 410, '+', 100, 400, {300,0}, {10,-1});
LIFT("no right flank", "src1", 90, 400, '+', 100, 400, {10,-1}, {300,10});

// An aligned block entirely in the chain gap becomes part of the unaligned run beside it.
LIFT("block in gap", "src1", 90, 710, '+', 100, 700,
     {10,-1}, {300,10}, {50,420}, {100,520}, {10,-1});

// An aligned block of size 0 is folded into the unaligned run too.
LIFT("empty block", "src1", 90, 410, '+', 100, 400, {10,-1}, {0,10}, {300,10}, {10,-1});

// The second fragment lands on ref3, not ref1:  it joins the right flank.
LIFT("other sequence", "src1", 1900, 6200, '+', 1900, 6200, {100,0}, {50,-1}, {100,4200});

// A '-' chain:  blocks reversed, strand flipped, the flanks change sides.
LIFT("minus chain", "src1", 3090, 3610, '+', 3100, 3600,
     {10,-1}, {200,10}, {30,-1}, {200,310}, {10,-1});
LIFT("minus chain, minus item", "src1", 3100, 3300, '-', 3100, 3300, {200,0});
LIFT("minus chain, ends past flanks", "src1", 3050, 3650, '+', 3100, 3600,
     {10,-1}, {500,50}, {10,-1});

// Negative flank sizes reach nowhere.
LIFT("negative flanks", "src1", 0, 300, '+', 0, 300, {-5000,-1}, {300,0}, {-7,-1});

// A flank longer than the distance to the start of the sequence stops at 0.
LIFT("flank past 0", "src1", 0, 300, '+', 0, 300, {5000,-1}, {300,0});

// No aligned block lands, but some of the item does:  what lands is kept as one unaligned
// block, refs #38613.  The aligned block is in the chain gap at src1 500-600, with both
// flanks around it lifting.
LIFT("aligned in gap", "src1", 400, 700, '+', 500, 600, {100,-1}, {100,100}, {100,-1});
// The aligned block is past the end of chain 1, and only the left flank lands.
LIFT("aligned off chain", "src1", 1900, 2300, '+', 2100, 2200, {200,-1}, {100,200}, {100,-1});
// The same on a '-' chain, with the right flank landing:  the strand flips.
LIFT("aligned off minus chain", "src1", 2800, 3200, '+', 2900, 2990,
     {100,-1}, {90,100}, {210,-1});
// No aligned block at all.
LIFT("only unaligned", "src1", 100, 400, '+', 100, 400, {10,-1}, {20,-1});

// Dropped:  nothing that maps, out of order, no blocks.
LIFT("no chain", "srcX", 100, 400, '+', 100, 400, {300,0});
LIFT("aligned and flanks off chain", "src1", 2100, 2900, '+', 2200, 2800,
     {100,-1}, {600,100}, {100,-1});
LIFT("out of order", "src1", 5000, 5500, '+', 5000, 5500, {100,0}, {100,-1}, {100,350});
liftOne(chainHash, "no blocks", "src1", 100, 400, '+', 100, 400, 0, NULL, NULL);

printf("bad shapes: %d\n", badCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
quickLiftJoinedTester(argv[1]);
return 0;
}
