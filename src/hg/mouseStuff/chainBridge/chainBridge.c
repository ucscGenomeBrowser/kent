/* chainBridge - Attempt to extend alignments through double-sided gaps of similar size. */
#include "common.h"
#include "axt.h"
#include "bandExt.h"
#include "chainBlock.h"
#include "chainConnect.h"
#include "dnaseq.h"
#include "gapCalc.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "twoBit.h"

/* Command line parameters & defaults */
unsigned maxGapSize = 6000;
double diffTolerance = 0.3;
struct gapCalc *gapCalc = NULL;	/* Gap scoring scheme to use. */
struct axtScoreScheme *scoreScheme = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainBridge - Attempt to extend alignments through double-sided gaps of similar size\n"
  "usage:\n"
  "   chainBridge in.chain target.2bit query.2bit out.chain\n"
  "options:\n"
  "   -maxGap=N  Maximum size of double-sided gap to try to bridge (default: %d)\n"
  "              Note: there is no size limit for exact sequence matches\n"
  "   -scoreScheme=fileName Read the scoring matrix from a blastz-format file\n"
  "   -linearGap=<medium|loose|filename> Specify type of linearGap to use.\n"
  "              loose is chicken/human linear gap costs.\n"
  "              medium is mouse/human linear gap costs.\n"
  "              Or specify a piecewise linearGap tab delimited file.\n"
  "              (default: loose)\n"
  , maxGapSize
  );
}

/* Command line validation table. */
static struct optionSpec options[] =
{
    { "maxGap", OPTION_INT },
    { "scoreScheme", OPTION_STRING },
    { "linearGap", OPTION_STRING },
    { NULL, 0 },
};

struct chromStrandSeq
    /* Cache whole chromosome sequences on the strand specified.
     * Lots of mem, but quicker than thousands of twoBitReadSeqFrag's and simpler than
     * reversing tweaked coords and sequence; we can just use chain q coords as-is. */
    {
    struct hash *seqHash;
    struct twoBitFile *tbf;
    };

static struct chromStrandSeq *chromStrandSeqNew(char *twoBitFileName)
/* Create a cache for tbf sequences. */
{
struct chromStrandSeq *css;
AllocVar(css);
css->seqHash = hashNew(0);
css->tbf = twoBitOpen(twoBitFileName);
return css;
}

static struct dnaSeq *chromStrandSeqGetDnaSeq(struct chromStrandSeq *css, char *chrom, char strand)
/* Return the lowercased sequence of chrom on specified strand, '+' or '-'. Do not modify/free! */
{
if (strand != '-' && strand != '+')
    errAbort("chromStrandSeqGetSeq: strand must be '+' or '-' not '%c'", strand);
int nameLen = strlen(chrom) + 1;
char chromStrand[nameLen+1];
safef(chromStrand, sizeof(chromStrand), "%s%c", chrom, strand);
struct dnaSeq *seq = hashFindVal(css->seqHash, chromStrand);
if (seq == NULL)
    {
    // See if we already have sequence for the other strand, if so just copy and rc
    char chromOtherStrand[nameLen+1];
    safef(chromOtherStrand, sizeof(chromOtherStrand), "%s%c", chrom, (strand == '-' ? '+' : '-'));
    struct dnaSeq *otherStrandSeq = hashFindVal(css->seqHash, chromOtherStrand);
    if (otherStrandSeq)
        {
        seq = cloneDnaSeq(otherStrandSeq);
        reverseComplement(seq->dna, seq->size);
        }
    else
        {
        seq = twoBitReadSeqFragLower(css->tbf, chrom, 0, 0);
        if (strand == '-')
            reverseComplement(seq->dna, seq->size);
        }
    hashAdd(css->seqHash, chromStrand, seq);
    }
return seq;
}

static char *chromStrandSeqGetSeq(struct chromStrandSeq *css, char *chrom, char strand)
/* Return the lowercased sequence of chrom on specified strand, '+' or '-'. Do not modify/free! */
{
struct dnaSeq *seq = chromStrandSeqGetDnaSeq(css, chrom, strand);
return seq->dna;
}

static boolean canTrivialExtend(struct chain *chain, struct cBlock *blk,
                                struct chromStrandSeq *tCss, struct chromStrandSeq *qCss,
                                int *retFromStart, int *retFromEnd)
/* Find the number of bases between blk and blk->next that are identical in t and q,
 * starting at the beginning of the gap and, if bases are left over, from the end of the gap.
 * Return TRUE if gap has equal size and completely identical sequence on t and q. */
{
if (blk == NULL || blk->next == NULL)
    {
    *retFromStart = 0;
    *retFromEnd = 0;
    return FALSE;
    }
int fromStart = 0, fromEnd = 0;
int tGapStart = blk->tEnd, tGapEnd = blk->next->tStart;
int qGapStart = blk->qEnd, qGapEnd = blk->next->qStart;
int tGapSize = tGapEnd - tGapStart;
int qGapSize = qGapEnd - qGapStart;
int smaller = (tGapSize < qGapSize) ? tGapSize : qGapSize;
if (smaller == 0)
    {
    *retFromStart = 0;
    *retFromEnd = 0;
    return FALSE;
    }
else if (smaller < 0)
    errAbort("Negative gap length found: chain %d, t %s[%d,%d) q %s[%d,%d)",
             chain->id, chain->tName, tGapStart, tGapEnd, chain->qName, qGapStart, qGapEnd);
char *tChrom = chromStrandSeqGetSeq(tCss, chain->tName, '+');
char *qChrom = chromStrandSeqGetSeq(qCss, chain->qName, chain->qStrand);
char *tSeq = tChrom + tGapStart;
char *qSeq = qChrom + qGapStart;
while (fromStart < smaller && tSeq[fromStart] == qSeq[fromStart])
    fromStart++;
int basesAtEnd = smaller - fromStart;
tSeq = tChrom + tGapEnd - basesAtEnd;
qSeq = qChrom + qGapEnd - basesAtEnd;
while (fromEnd < basesAtEnd && tSeq[basesAtEnd-1-fromEnd] == qSeq[basesAtEnd-1-fromEnd])
    fromEnd++;
*retFromStart = fromStart;
*retFromEnd = fromEnd;
return (fromStart == tGapSize && tGapSize == qGapSize);
}

static boolean tryTrivialExtend(struct chain *chain, struct cBlock *blk,
                                struct chromStrandSeq *tCss, struct chromStrandSeq *qCss,
                                struct cBlock **retNextBlk)
/* Sometimes double-sided gaps of equal length have identical sequence on t and q --
 * in that trivial case, just merge blk and blk->next and see if the next gap is trivial too.
 * If a double-sided gap is not trivial but has identical sequence at the start and/or end,
 * adjust blk and blk->next coords to shrink the gap to just the mismatching sequence.
 * Return TRUE if any changes were made. */
{
boolean changed = FALSE;
struct cBlock *nextBlk = blk->next;
int fromStart = 0, fromEnd = 0;
while (canTrivialExtend(chain, blk, tCss, qCss, &fromStart, &fromEnd))
    {
    // The most trivial case: entire gap matches perfectly on t and q.
    // Merge nextBlk into blk, repeat to see if we can keep merging.
    blk->tEnd = nextBlk->tEnd;
    blk->qEnd = nextBlk->qEnd;
    blk->next = nextBlk->next;
    freeMem(nextBlk);
    nextBlk = blk->next;
    changed = TRUE;
    }
// Not completely trivial, but we can shave some bases off the beginning and/or
// end of the gap.
if (fromStart > 0)
    {
    blk->tEnd += fromStart;
    blk->qEnd += fromStart;
    changed = TRUE;
    }
if (fromEnd > 0)
    {
    nextBlk->tStart -= fromEnd;
    nextBlk->qStart -= fromEnd;
    changed = TRUE;
    }
*retNextBlk = nextBlk;
return changed;
}

static boolean canExtend(struct cBlock *blk)
/* Return TRUE if blk is followed by a gap of similar size on t and q and not too large. */
{
if (blk == NULL || blk->next == NULL)
    return FALSE;
int tGapSize = blk->next->tStart - blk->tEnd;
int qGapSize = blk->next->qStart - blk->qEnd;
if (tGapSize == 0 || qGapSize == 0)
    return FALSE;
int smaller, larger;
if (tGapSize < qGapSize)
    {
    smaller = tGapSize;
    larger = qGapSize;
    }
else
    {
    smaller = qGapSize;
    larger = tGapSize;
    }
if (smaller > maxGapSize)
    {
    verbose(2, "gap size %d is too big\n", smaller);
    return FALSE;
    }
double sizeRatio = (double)larger / (double)smaller;
verbose(2, "tGapSize=%d, qGapSize=%d, sizeRatio=%f\n", tGapSize, qGapSize, sizeRatio);
if (sizeRatio < 1.0 + diffTolerance)
    return TRUE;
return FALSE;
}

static boolean maybeMergeBlocks(struct cBlock *blk0, struct cBlock *blk1)
/* Merge blk1 into blk0 if possible.  I.e. if blk1's t and q start at the end of blk0
 * then update blk0's t and q end to blk1's t and q end, and return TRUE.
 * blk0 and blk1 must be on same t and q chroms and blk1 must start after blk0 on t and q. */
{
if (blk0 && blk1)
    {
    int tOverlap = blk0->tEnd - blk1->tStart;
    int qOverlap = blk0->qEnd - blk1->qStart;
    if (tOverlap == qOverlap && tOverlap >= 0 && qOverlap >= 0)
        {
        blk0->tEnd = blk1->tEnd;
        blk0->qEnd = blk1->qEnd;
        return TRUE;
        }
    }
return FALSE;
}

static void trimAndAddBlock(struct cBlock **pBlockList, struct cBlock *blk)
/* Trim overlap between blk and head of *pBlockList, then add blk to head if anything is left. */
{
struct cBlock *curBlk = *pBlockList;
if (curBlk != NULL)
    {
    int overlap = curBlk->tEnd - blk->tStart;
    if (overlap > 0)
        {
        blk->tStart = curBlk->tEnd;
        blk->qStart += overlap;
        }
    overlap = curBlk->qEnd - blk->qStart;
    if (overlap > 0)
        {
        blk->tStart += overlap;
        blk->qStart = curBlk->qEnd;
        }
    }
if (blk->tEnd > blk->tStart && blk->qEnd > blk->qStart)
    slAddHead(pBlockList, blk);
}

static void chainBridgeOne(struct chain *chain,
                           struct chromStrandSeq *tCss, struct chromStrandSeq *qCss, FILE *outF)
/* Iterate through gaps; merge blocks separated by double-sided gaps with identical sequence
 * on t and q.  When we find a double-sided gap that is approximately the same size on t and q,
 * try to bridge the gap with a banded S-W alignment. */
{
if (chain == NULL || chain->blockList == NULL || chain->blockList->next == NULL)
    return;
// Provide a little context for bandExt input sequences:
int overlap = 5;
// Alignment parameters for bandExt:
boolean global = FALSE;
int dir = 1;  // +1 for forward, -1 for reverse
int maxInsert = maxGapSize/10;
int symAlloc = 2 * maxGapSize;
int symCount = 0;
boolean changed = FALSE;
struct cBlock *newBlockList = NULL;
struct cBlock *blk = chain->blockList, *nextBlk = blk->next;
for ( ; blk != NULL; blk = nextBlk)
    {
    nextBlk = blk->next;
    changed |= tryTrivialExtend(chain, blk, tCss, qCss, &nextBlk);
    if (canExtend(blk))
        {
        char *tChrom = chromStrandSeqGetSeq(tCss, chain->tName, '+');
        char *qChrom = chromStrandSeqGetSeq(qCss, chain->qName, chain->qStrand);
        int tGapStart = blk->tEnd, tGapEnd = blk->next->tStart;
        int qGapStart = blk->qEnd, qGapEnd = blk->next->qStart;
        int tAliStart = tGapStart - overlap, tAliEnd = tGapEnd + overlap;
        int qAliStart = qGapStart - overlap, qAliEnd = qGapEnd + overlap;
        if (tAliStart < 0 || tAliEnd > chain->tSize || qAliStart < 0 || qAliEnd > chain->qSize)
            errAbort("chainBridgeOne: program error, need more sophisticated overlap arithmetic");
        char tSym[symAlloc+1], qSym[symAlloc+1];
        if (bandExt(global, scoreScheme, maxInsert, tChrom + tAliStart, tAliEnd - tAliStart,
                    qChrom + qAliStart, qAliEnd - qAliStart,
                    dir, symAlloc, &symCount, tSym, qSym, NULL, NULL))
            {
            struct cBlock *extBlocks = cBlocksFromAliSym(symCount, qSym, tSym,
                                                         qAliStart, tAliStart);
            if (maybeMergeBlocks(blk, extBlocks))
                {
                // The first extended block merged with blk; if there was only one ext block,
                // can we also merge in nextBlk?
                freeMem(slPopHead(&extBlocks));
                if (!extBlocks && maybeMergeBlocks(blk, nextBlk))
                    {
                    // A single extBlock completely bridged the gap between blk and nextBlk.
                    // Splice out nextBlk and repeat with blk.
                    blk->next = nextBlk->next;
                    freeMem(nextBlk);
                    nextBlk = blk;
                    continue;
                    }
                }
            // Done with blk.
            trimAndAddBlock(&newBlockList, blk);
            if (extBlocks)
                {
                // Add additional extBlocks before the final extBlock
                while (extBlocks->next)
                    trimAndAddBlock(&newBlockList, slPopHead(&extBlocks));
                if (maybeMergeBlocks(extBlocks, nextBlk))
                    {
                    // The final extended block merged with nextBlk; splice out nextBlk
                    // and repeat with final extended block.
                    extBlocks->next = nextBlk->next;
                    freeMem(nextBlk);
                    nextBlk = extBlocks;
                    }
                else
                    {
                    // The final extended block didn't merge with nextBlk.  Done with final
                    // extended block, on to nextBlk.
                    trimAndAddBlock(&newBlockList, extBlocks);
                    }
                }
            changed = TRUE;
            continue;
            }
        }
    trimAndAddBlock(&newBlockList, blk);
    }
slReverse(&newBlockList);
chain->blockList = newBlockList;
if (changed)
    {
    // Update chain score.
    struct dnaSeq *tSeq = chromStrandSeqGetDnaSeq(tCss, chain->tName, '+');
    struct dnaSeq *qSeq = chromStrandSeqGetDnaSeq(qCss, chain->qName, chain->qStrand);
    chain->score = chainCalcScore(chain, scoreScheme, gapCalc, qSeq, tSeq);
    }
}

void chainBridge(char *inFile, char *tTwoBitFile, char *qTwoBitFile, char *outFile)
/* chainBridge - Attempt to extend alignments through double-sided gaps of similar size. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
struct chromStrandSeq *tCss = chromStrandSeqNew(tTwoBitFile);
struct chromStrandSeq *qCss = chromStrandSeqNew(qTwoBitFile);
FILE *outF = mustOpen(outFile, "w");
axtScoreSchemeDnaWrite(scoreScheme, outF, "chainBridge");
struct chain *chain, *chainList = NULL;
while ((chain = chainRead(lf)) != NULL)
    {
    chainBridgeOne(chain, tCss, qCss, outF);
    slAddHead(&chainList, chain);
    }
lineFileClose(&lf);
// Since scores may have changed, re-sort to maintain score order (required by chainNet).
slReverse(&chainList);
slSort(&chainList, chainCmpScore);
for (chain = chainList;  chain != NULL;  chain = chain->next)
    chainWrite(chain, outF);
carefulClose(&outF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
maxGapSize = optionInt("maxGap", maxGapSize);
char *gapFileName = optionVal("linearGap", NULL);
if (isNotEmpty(gapFileName))
    gapCalc = gapCalcFromFile(gapFileName);
else
    gapCalc = gapCalcDefault();
char *scoreSchemeName = optionVal("scoreScheme", NULL);
if (isNotEmpty(scoreSchemeName))
    scoreScheme = axtScoreSchemeRead(scoreSchemeName);
else
    scoreScheme = axtScoreSchemeDefault();
chainBridge(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
