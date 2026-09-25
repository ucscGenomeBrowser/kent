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
 * refs #38349, #38249 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "chain.h"
#include "psl.h"
#include "liftOver.h"
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
  "Writes one block of output per case to stdout.  Takes no arguments and reads nothing.\n");
}

static struct chain *chainFromBlocks(char qStrand, int blockCount, int *refStarts,
    int *srcStarts, int *sizes)
/* One chain in the shape the lift functions read it: loaded with the reference on the
 * target side, then swapped so the assembly the data came from is the target.  The block
 * coordinates are the chain's own, so with qStrand '-' the source side is counted from the
 * far end, the same as in a chain file.  Blocks must be given in ascending target order. */
{
struct chain *chain;
struct cBlock *blockList = NULL, *b;
int i;

AllocVar(chain);
chain->tName = cloneString(refChrom);
chain->tSize = seqSize;
chain->qName = cloneString(srcChrom);
chain->qSize = seqSize;
chain->qStrand = qStrand;
chain->id = 1;
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

int main(int argc, char *argv[])
{
if (argc != 1)
    usage();
sameStrandProtein();
oppositeStrandProtein();
targetGap();
splitCodon();
mrnaBothStrands();
emptyBlock();
noChain();
printf("passed\n");
return 0;
}
