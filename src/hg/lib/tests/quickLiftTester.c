/* quickLiftTester - check what quickLiftPsl hands back for a protein alignment.
 *
 * pslTransMap does the mapping in nucleotide space and leaves the result there, so
 * quickLiftPsl has to put the query side back into protein units afterwards.  That fixup
 * has to agree with pslIsProtein(), because every reader downstream asks that question and
 * then multiplies block sizes by three or by one on the answer.  A psl that says "++" while
 * its blocks sit in minus-strand target coordinates passes no check and draws at a third of
 * its length in the wrong place, which is what #38349 found.
 *
 * So each case here runs pslCheck2() over the lifted alignment as well as printing it.
 * PSL_CHECK_IGNORE_INSERT_CNTS is on because pslCheck2's own comment says protein psls do
 * not compute the insert counts consistently; everything else is checked, including the
 * target range, which is where a wrong strand shows up.
 *
 * No database and no files: the chains are built here in the shape quickLiftSourceRanges
 * leaves in the hash, which is the reference on the query side after a chainSwap.
 * refs #38349, #38249
 *
 * The second half checks the pieces the readers use to decide what to ask the source
 * assembly for:  quickLiftChainSourceRuns, quickLiftQueryRanges, quickLiftRangeWhere,
 * quickLiftMapToReference, and the one-chain hash behind quickLiftChainHashForItem.
 *
 * "quickLiftTester intervals chain.bb items.bb" instead reads items through
 * quickLiftGetIntervals out of the small files in input/quickLift, which lets the
 * makefile run it under each setting of quickLiftSplitRanges and quickLiftMultiChain.
 * refs #38510 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "chain.h"
#include "psl.h"
#include "liftOver.h"
#include "binRange.h"
#include "dystring.h"
#include "bigBed.h"
/* for struct sqlConnection, struct cart and struct trackDb, named in prototypes in
 * quickLift.h, which does not declare them itself */
#include "jksql.h"
#include "cart.h"
#include "trackDb.h"
#include "quickLift.h"

#define refChrom "chrRef"
#define srcChrom "chrSrc"
#define seqSize 1000

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "quickLiftTester - check quickLiftPsl's protein handling\n"
  "usage:\n"
  "   quickLiftTester\n"
  "   quickLiftTester intervals chain.bb items.bb\n"
  "With no arguments, writes one block of output per case to stdout and reads nothing.\n"
  "With intervals, reads the items in chrRef:0-10000 through quickLiftGetIntervals,\n"
  "under whatever quickLiftSplitRanges and quickLiftMultiChain the hg.conf sets.\n");
}

static struct chain *loadedChain(int id, char *qName, int size, char qStrand, int blockCount,
    int *refStarts, int *srcStarts, int *sizes)
/* One chain as it is loaded, with the reference chrRef on the target side.  The block
 * coordinates are the chain's own, so with qStrand '-' the source side is counted from the
 * far end, the same as in a chain file.  Blocks must be given in ascending target order. */
{
struct chain *chain;
struct cBlock *blockList = NULL, *b;
int i;

AllocVar(chain);
chain->tName = cloneString(refChrom);
chain->tSize = size;
chain->qName = cloneString(qName);
chain->qSize = size;
chain->qStrand = qStrand;
chain->id = id;
chain->score = 1000;
for (i = blockCount - 1; i >= 0; i--)
    {
    AllocVar(b);
    b->tStart = refStarts[i];
    b->tEnd = refStarts[i] + sizes[i];
    b->qStart = srcStarts[i];
    b->qEnd = srcStarts[i] + sizes[i];
    slAddHead(&blockList, b);
    }
chain->blockList = blockList;
struct cBlock *last = slLastEl(blockList);
chain->tStart = blockList->tStart;
chain->tEnd = last->tEnd;
chain->qStart = blockList->qStart;
chain->qEnd = last->qEnd;
return chain;
}

static struct chain *chainFromBlocks(char qStrand, int blockCount, int *refStarts,
    int *srcStarts, int *sizes)
/* One chain in the shape the lift functions read it: loaded with the reference on the
 * target side, then swapped so the assembly the data came from is the target. */
{
struct chain *chain = loadedChain(1, srcChrom, seqSize, qStrand, blockCount,
                                  refStarts, srcStarts, sizes);
chainSwap(chain);
return chain;
}

static struct psl *pslOnSource(char *qName, int aaCount, int tStart, boolean isProtein)
/* One ungapped alignment of qName to the source assembly, in the units its type implies:
 * a protein psl counts its query and its blocks in amino acids and its target in bases, an
 * mRNA psl counts everything in bases. */
{
int qSize = isProtein ? aaCount : aaCount * 3;
int tLen = aaCount * 3;
struct psl *psl = pslNew(qName, qSize, 0, qSize, srcChrom, seqSize, tStart, tStart + tLen,
                         isProtein ? "++" : "+", 1, 0);

psl->blockCount = 1;
psl->blockSizes[0] = qSize;
psl->qStarts[0] = 0;
psl->tStarts[0] = tStart;
psl->match = qSize;
return psl;
}

static void reportChain(struct chain *chain)
/* Print the chain as the lift functions see it, so a change in chainSwap shows up here
 * rather than as an unexplained change in the alignment below it. */
{
struct cBlock *b;

printf("chain: t=%s:%d-%d q=%s:%d-%d qStrand=%c blocks",
       chain->tName, chain->tStart, chain->tEnd,
       chain->qName, chain->qStart, chain->qEnd, chain->qStrand);
for (b = chain->blockList; b != NULL; b = b->next)
    printf(" %d-%d/%d-%d", b->tStart, b->tEnd, b->qStart, b->qEnd);
printf("\n");
}

static void reportPsl(char *label, struct psl *psl)
/* Print one alignment and what the readers downstream make of it. */
{
printf("%-8s ", label);
if (psl == NULL)
    {
    printf("did not lift\n");
    return;
    }
pslTabOut(psl, stdout);
printf("%-8s strand='%s' isProtein=%d qSize=%d blockSizes[0]=%d pslCheck errors=%d\n",
       "", psl->strand, pslIsProtein(psl), psl->qSize, psl->blockSizes[0],
       pslCheck2(PSL_CHECK_IGNORE_INSERT_CNTS, label, stdout, psl));
}

static void runCase(char *name, struct chain *chain, struct psl *psl)
/* Lift one alignment over one chain and report both sides. */
{
struct hash *chainHash = newHash(8);
struct hash *mapPsls = NULL;

liftOverAddChainHash(chainHash, chain);
printf("== %s\n", name);
reportChain(chain);
reportPsl("before", psl);
reportPsl("lifted", quickLiftPsl(chainHash, &mapPsls, psl));
printf("\n");
}

static void sameStrandProtein()
/* The plain case: the two assemblies run the same way, so the lifted protein keeps "++". */
{
int refStarts[] = {100}, srcStarts[] = {100}, sizes[] = {300};
runCase("protein, chain on the same strand",
        chainFromBlocks('+', 1, refStarts, srcStarts, sizes),
        pslOnSource("prot", 100, 100, TRUE));
}

static void oppositeStrandProtein()
/* #38349.  The chain turns the alignment over, so pslTransMap hands back strand[0] == '-'
 * and quickLiftPsl calls pslRc to move the minus onto the target side.  The result has to
 * stay "+-": with "++" over reverse-complemented blocks pslIsProtein answers no, and the
 * whole item is then drawn at a third of its length at a mirrored position. */
{
int refStarts[] = {100}, srcStarts[] = {100}, sizes[] = {300};
runCase("protein, chain on the opposite strand",
        chainFromBlocks('-', 1, refStarts, srcStarts, sizes),
        pslOnSource("prot", 100, 600, TRUE));
}

static void targetGap()
/* A gap on the reference side alone costs the query nothing, so the alignment is still a
 * whole number of codons and the fixup goes through. */
{
int refStarts[] = {100, 251}, srcStarts[] = {100, 250}, sizes[] = {150, 150};
runCase("protein, chain gaps the reference",
        chainFromBlocks('+', 2, refStarts, srcStarts, sizes),
        pslOnSource("prot", 100, 100, TRUE));
}

static void splitCodon()
/* A chain that drops a base on the source side takes that base out of the protein's query,
 * so the query side is no longer divisible by three.  The fixup gives up and leaves the
 * alignment in nucleotide space rather than reporting a codon count it cannot honour. */
{
int refStarts[] = {100, 250}, srcStarts[] = {100, 251}, sizes[] = {150, 150};
runCase("protein, lift splits a codon",
        chainFromBlocks('+', 2, refStarts, srcStarts, sizes),
        pslOnSource("prot", 100, 100, TRUE));
}

static void mrnaBothStrands()
/* The same two chains over an mRNA alignment, which the protein fixup must not touch. */
{
int refStarts[] = {100}, srcStarts[] = {100}, sizes[] = {300};
runCase("mRNA, chain on the same strand",
        chainFromBlocks('+', 1, refStarts, srcStarts, sizes),
        pslOnSource("mrna", 100, 100, FALSE));
runCase("mRNA, chain on the opposite strand",
        chainFromBlocks('-', 1, refStarts, srcStarts, sizes),
        pslOnSource("mrna", 100, 600, FALSE));
}

static void emptyBlock()
/* #38249.  The UniProt bigPsl files store block sizes in bases and pslFromBigPsl divides
 * them by three, so a block shorter than a codon loads with size 0.  pslTransMap rejects
 * such an alignment and aborts, which took down the whole lifted SwissProt track.  The lift
 * has to leave the empty block out and map the rest. */
{
int refStarts[] = {100}, srcStarts[] = {100}, sizes[] = {600};
// The shape of Q96ME1-2 in hg19, whose first block is a single base:  query on the minus
// strand, so the empty block sits at the far end of the query, where pslCheck notices it.
struct psl *psl = pslNew("prot", 100, 0, 100, srcChrom, seqSize, 100, 400, "-+", 2, 0);

psl->blockCount = 2;
psl->blockSizes[0] = 0;
psl->qStarts[0] = 0;
psl->tStarts[0] = 100;
psl->blockSizes[1] = 90;
psl->qStarts[1] = 10;
psl->tStarts[1] = 130;
psl->match = 90;
runCase("protein, a block shorter than a codon",
        chainFromBlocks('+', 1, refStarts, srcStarts, sizes), psl);
}

static void noChain()
/* Nothing covers the alignment, so it is dropped rather than half-lifted. */
{
int refStarts[] = {100}, srcStarts[] = {100}, sizes[] = {300};
runCase("no chain over the alignment",
        chainFromBlocks('+', 1, refStarts, srcStarts, sizes),
        pslOnSource("prot", 100, 700, TRUE));
}

#define bigSize 100000

static void reportRanges(struct quickLiftRange *rangeList)
/* Print a list of ranges, one to a line. */
{
struct quickLiftRange *range;
if (rangeList == NULL)
    printf("  (none)\n");
for (range = rangeList; range != NULL; range = range->next)
    printf("  %s:%d-%d\n", range->chrom, range->start, range->end);
}

static void runSourceRuns(char *name, struct chain *chain, int tStart, int tEnd)
/* Print the source runs of one chain under the reference window tStart..tEnd. */
{
struct quickLiftRange *runList = NULL;
printf("== source runs: %s, window %s:%d-%d\n", name, refChrom, tStart, tEnd);
quickLiftChainSourceRuns(chain, tStart, tEnd, &runList);
slReverse(&runList);
reportRanges(runList);
quickLiftRangeListFree(&runList);
printf("\n");
}

static void sourceRuns()
/* A chain's source range is cut where the chain skips more than 10 kb of source, and only
 * there.  A minus-strand chain counts its source from the far end, and the run has to come
 * back on the plus strand. */
{
int refStarts[] = {100, 300}, sizes[] = {100, 100};
int smallGap[] = {100, 5300}, exactGap[] = {100, 10200}, overGap[] = {100, 10201};
int minusGap[] = {100, 20200};

runSourceRuns("a 5 kb source gap is read across",
              loadedChain(1, srcChrom, bigSize, '+', 2, refStarts, smallGap, sizes), 0, 1000);
runSourceRuns("a source gap of exactly 10000 is read across",
              loadedChain(1, srcChrom, bigSize, '+', 2, refStarts, exactGap, sizes), 0, 1000);
runSourceRuns("a source gap of 10001 splits the range",
              loadedChain(1, srcChrom, bigSize, '+', 2, refStarts, overGap, sizes), 0, 1000);
runSourceRuns("a minus-strand chain, split, on the plus strand",
              loadedChain(1, srcChrom, bigSize, '-', 2, refStarts, minusGap, sizes), 0, 1000);
runSourceRuns("the window clips both blocks",
              loadedChain(1, srcChrom, bigSize, '+', 2, refStarts, smallGap, sizes), 150, 350);
runSourceRuns("the window misses every block",
              loadedChain(1, srcChrom, bigSize, '+', 2, refStarts, smallGap, sizes), 500, 1000);
}

static struct chain *queryRangeChains()
/* Five chains under chrRef:0-10000, in id order:
 *   1  chrSrc 1000-2000 and 30000-31000, a 28 kb source gap between them
 *   2  chrSrc 1000-2000 again, a second copy of the same source on the reference
 *   3  chrSrc2 0-500, another source sequence
 *   4  no blocks
 *   5  chrSrc 2000-2500, touching the end of chains 1 and 2 */
{
struct chain *chainList = NULL, *empty;
int aRef[] = {1000, 2000}, aSrc[] = {1000, 30000}, aSize[] = {1000, 1000};
int bRef[] = {5000}, bSrc[] = {1000}, bSize[] = {1000};
int cRef[] = {7000}, cSrc[] = {0}, cSize[] = {500};
int eRef[] = {8000}, eSrc[] = {2000}, eSize[] = {500};

slAddHead(&chainList, loadedChain(5, srcChrom, bigSize, '+', 1, eRef, eSrc, eSize));
AllocVar(empty);
empty->tName = cloneString(refChrom);
empty->qName = cloneString(srcChrom);
empty->id = 4;
slAddHead(&chainList, empty);
slAddHead(&chainList, loadedChain(3, "chrSrc2", bigSize, '+', 1, cRef, cSrc, cSize));
slAddHead(&chainList, loadedChain(2, srcChrom, bigSize, '+', 1, bRef, bSrc, bSize));
slAddHead(&chainList, loadedChain(1, srcChrom, bigSize, '+', 2, aRef, aSrc, aSize));
return chainList;
}

static void runQueryRanges(char *name, struct chain *chainList, boolean perChain,
                           boolean split)
/* Print the ranges quickLiftQueryRanges makes of chainList under chrRef:0-10000. */
{
struct quickLiftQueryRange *qr, *qrList = quickLiftQueryRanges(chainList, 0, 10000,
                                                              perChain, split);
printf("== query ranges: %s (perChain=%d split=%d)\n", name, perChain, split);
for (qr = qrList; qr != NULL; qr = qr->next)
    printf("  %s:%d-%d chain=%d\n", qr->chrom, qr->start, qr->end, qr->chainId);
quickLiftQueryRangeFreeList(&qrList);
printf("\n");
}

static void queryRanges()
/* Without split, one range per chain, in chain order, as the readers had before #38510.
 * With split, each chain is cut at its gap, and the ranges are sorted and merged:  across
 * all the chains when not perChain, so the two copies of chrSrc 1000-2000 are read once and
 * the touching chain 5 joins them, and within each chain when perChain. */
{
struct chain *chainList = queryRangeChains();
runQueryRanges("one range per chain", chainList, FALSE, FALSE);
runQueryRanges("one range per chain, multi-chain", chainList, TRUE, FALSE);
runQueryRanges("split and merged across chains", chainList, FALSE, TRUE);
runQueryRanges("split and merged within each chain", chainList, TRUE, TRUE);
}

static void runRangeWhere(char *name, char *startField, int prevEnd, char *extraWhere)
/* Print the where clause for a range that follows one ending at prevEnd. */
{
struct dyString *dy = dyStringNew(0);
char *where = quickLiftRangeWhere(dy, startField, prevEnd, extraWhere);
printf("  %-22s %s%s\n", name, (where == NULL) ? "(null)" : where,
       ((where != NULL) && (where == extraWhere)) ? "  [the caller's own]" : "");
dyStringFree(&dy);
}

static void rangeWhere()
/* The clause that leaves out what the previous range returned has to go after a
 * condition, but before an order or limit clause, the way hRangeQuery places them. */
{
char nameWhere[256], orderWhere[256], limitWhere[256];
sqlSafef(nameWhere, sizeof nameWhere, "score > %d", 100);
sqlSafef(orderWhere, sizeof orderWhere, "order by score");
sqlSafef(limitWhere, sizeof limitWhere, "limit 10");

printf("== quickLiftRangeWhere\n");
runRangeWhere("no start field", NULL, 500, nameWhere);
runRangeWhere("no extraWhere", "chromStart", 500, NULL);
runRangeWhere("a condition", "chromStart", 500, nameWhere);
runRangeWhere("an order clause", "txStart", 500, orderWhere);
runRangeWhere("a limit clause", "txStart", 500, limitWhere);
printf("\n");
}

static void runMapToReference(char *name, struct hash *chainHash, int start, int end)
/* Print where chrSrc:start-end lands on the reference. */
{
struct hash *blockCache = newHash(4);
struct quickLiftRange *pieceList = quickLiftMapToReference(chainHash, blockCache, srcChrom,
                                                           start, end);
printf("== map to reference: %s, %s:%d-%d\n", name, srcChrom, start, end);
slReverse(&pieceList);
reportRanges(pieceList);
quickLiftRangeListFree(&pieceList);
printf("\n");
}

static void mapToReference()
/* One piece for every block a source range overlaps, found by binary search, and a
 * minus-strand chain turns the range around. */
{
int refStarts[] = {100, 300, 500}, srcStarts[] = {100, 5300, 9000}, sizes[] = {100, 100, 100};
struct chain *plus = loadedChain(1, srcChrom, bigSize, '+', 3, refStarts, srcStarts, sizes);
struct hash *plusHash = newHash(4);
chainSwap(plus);
liftOverAddChainHash(plusHash, plus);
runMapToReference("two blocks", plusHash, 150, 5350);
runMapToReference("the last block only", plusHash, 9050, 9200);
runMapToReference("only the gap", plusHash, 250, 5300);

int mRef[] = {100}, mSrc[] = {100}, mSize[] = {100};
struct chain *minus = loadedChain(1, srcChrom, bigSize, '-', 1, mRef, mSrc, mSize);
struct hash *minusHash = newHash(4);
chainSwap(minus);
liftOverAddChainHash(minusHash, minus);
runMapToReference("minus-strand chain", minusHash, 99850, 99900);
}

static void printChainIds(struct hash *chainHash, int start, int end)
/* Print the ids of the chains in chainHash over chrSrc:start-end, smallest first. */
{
struct binElement *el, *elList = liftOverChainsInRange(chainHash, srcChrom, start, end);
int ids[16], count = 0, i, j;
for (el = elList; (el != NULL) && (count < ArraySize(ids)); el = el->next)
    ids[count++] = ((struct chain *)el->val)->id;
slFreeList(&elList);
for (i = 1; i < count; i++)
    for (j = i; (j > 0) && (ids[j - 1] > ids[j]); j--)
        {
        int t = ids[j];
        ids[j] = ids[j - 1];
        ids[j - 1] = t;
        }
printf("chains=");
if (count == 0)
    printf("none");
for (i = 0; i < count; i++)
    printf("%s%d", (i == 0) ? "" : ",", ids[i]);
}

static void oneChainHash()
/* quickLiftChainHashForItem falls back to the whole chain hash when it knows nothing about
 * an item, and liftOverRemoveChainHash takes out only the chain it is given. */
{
int refStarts[] = {100}, srcStarts[] = {100}, sizes[] = {100};
struct chain *one = loadedChain(1, srcChrom, bigSize, '+', 1, refStarts, srcStarts, sizes);
struct chain *two = loadedChain(2, srcChrom, bigSize, '+', 1, refStarts, srcStarts, sizes);
struct hash *chainHash = newHash(4);
int item;

chainSwap(one);
chainSwap(two);
liftOverAddChainHash(chainHash, one);
liftOverAddChainHash(chainHash, two);
printf("== one-chain hash\n");
printf("  no chain hash:           %s\n",
       (quickLiftChainHashForItem(NULL, &item) == NULL) ? "NULL" : "not NULL");
struct hash *forItem = quickLiftChainHashForItem(chainHash, &item);
printf("  an item never assigned:  %s\n", (forItem == chainHash) ? "the whole hash" : "another");
printf("  both added:              ");
printChainIds(chainHash, 100, 200);
liftOverRemoveChainHash(chainHash, one);
printf("\n  chain 1 removed:         ");
printChainIds(chainHash, 100, 200);
liftOverRemoveChainHash(chainHash, one);
printf("\n  chain 1 removed again:   ");
printChainIds(chainHash, 100, 200);
printf("\n\n");
}

static void intervals(char *chainFile, char *itemsFile)
/* Read the items under chrRef:0-10000 through quickLiftGetIntervals, and print each one
 * with the chains quickLiftChainHashForItem would lift it through. */
{
struct bbiFile *bbi = bigBedFileOpen(itemsFile);
struct hash *chainHash = NULL;
struct bigBedInterval *bb, *bbList = quickLiftGetIntervals(chainFile, bbi, refChrom, 0, 10000,
                                                           &chainHash);
printf("quickLiftSplitRanges=%d quickLiftMultiChain=%d\n", quickLiftSplitRangesEnabled(),
       quickLiftMultiChainEnabled());
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    printf("  %s:%d-%d %-14s ", srcChrom, bb->start, bb->end, bb->rest);
    printChainIds(quickLiftChainHashForItem(chainHash, bb), bb->start, bb->end);
    printf("\n");
    }
printf("%d items\n", slCount(bbList));
bigBedFileClose(&bbi);
}

int main(int argc, char *argv[])
{
if ((argc == 4) && sameString(argv[1], "intervals"))
    {
    intervals(argv[2], argv[3]);
    return 0;
    }
if (argc != 1)
    usage();
sameStrandProtein();
oppositeStrandProtein();
targetGap();
splitCodon();
mrnaBothStrands();
emptyBlock();
noChain();
sourceRuns();
queryRanges();
rangeWhere();
mapToReference();
oneChainHash();
printf("passed\n");
return 0;
}
