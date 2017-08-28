/* variantProjector -- use sequence alignments and indel shifting to transform genomic variants
 * to transcript/CDS and protein variants.  Compute sufficient information to predict functional
 * effects on proteins and to form HGVS g., n., and if applicable c. and p. terms. */

/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "indelShift.h"
#include "variantProjector.h"

static enum vpTxRegion complementRegion(enum vpTxRegion regionIn)
/* Reverse the +-strand-computed region for a transcript on the - strand. */
{
enum vpTxRegion regionOut = regionIn;
if (regionIn == vpUpstream)
    regionOut = vpDownstream;
else if (regionIn == vpDownstream)
    regionOut = vpUpstream;
return regionOut;
}

INLINE void swapUint(uint *pA, uint *pB)
/* Swap the values of a and b */
{
uint tmp = *pA;
*pA = *pB;
*pB = tmp;
}

static void vpTxPosRc(struct vpTxPosition *txPos, uint txSize)
/* Reverse/complement all components of txPos. */
{
txPos->region = complementRegion(txPos->region);
txPos->txOffset = txSize - txPos->txOffset;
// No change to distances -- except for introns where we swap 5' / 3':
if (txPos->region == vpIntron)
    {
    txPos->intron3TxOffset = txSize - txPos->intron3TxOffset;
    swapUint(&txPos->txOffset, &txPos->intron3TxOffset);
    swapUint(&txPos->gDistance, &txPos->intron3Distance);
    }
// No change to aliBlkIx -- no change to alignment, it is still genomic + strand.
}

static void vpTxPosIntronic(struct vpTxPosition *txPos, struct psl *txAli, uint gOffset, int ix)
/* For introns [could there ever be double-sided gaps??] we need tx offsets and distances
 * for both 5' exon (before the intron) and 3' exon (after the intron).  */
{
uint intronStart = txAli->tStarts[ix] + txAli->blockSizes[ix];
if (gOffset < intronStart)
    errAbort("vpTxIntronicPos: gOffset (%u) < start of intron %d (%u)", gOffset, ix, intronStart);
uint intronEnd = txAli->tStarts[ix+1];
if (gOffset > intronEnd)
    errAbort("vpTxIntronicPos: gOffset (%u) > end of intron %d (%u)", gOffset, ix, intronEnd);
uint txLeft = txAli->qStarts[ix] + txAli->blockSizes[ix];
uint txRight = txAli->qStarts[ix+1];
txPos->txOffset = txLeft;
txPos->gDistance = gOffset - intronStart;
txPos->intron3TxOffset = txRight;
txPos->intron3Distance = intronEnd - gOffset;
}

static boolean genomeHasDeletion(struct psl *txAli, int ix)
/* Return TRUE if block ix has a gap to the right that skips 0 bases on target (reference genome)
 * and >0 bases on query (transcript) -- i.e. not an intron, but rather some missing base(s)
 * in the reference genome. */
{
if (ix < 0 || ix >= txAli->blockCount - 1)
    return FALSE;
int blockSize = txAli->blockSizes[ix];
return (txAli->tStarts[ix+1] == txAli->tStarts[ix] + blockSize &&
        txAli->qStarts[ix+1] > txAli->qStarts[ix] + blockSize);

}

void vpPosGenoToTx(uint gOffset, struct psl *txAli, struct vpTxPosition *txPos, boolean isTxEnd)
/* Use txAli to project gOffset onto transcript-relative coords in txPos.
 * Set isTxEnd to TRUE if we are projecting to the end coordinate in transcript space:
 * higher genomic coord if transcript on '+' strand, lower genomic coord if tx on '-' strand. */
{
ZeroVar(txPos);
txPos->aliBlkIx = -1;
txPos->gOffset = gOffset;
boolean isRc = (pslQStrand(txAli) == '-');
// Coordinate transforms of start and end coordinates can be done the same way, but
// when determining which region of the transcript the variant falls in, we need to
// treat the open end differently (looking backward) from the closed start (looking forward).
int endCmp = (isTxEnd != isRc) ? 1 : 0;
int gOffsetCmp = gOffset - endCmp;
if (gOffsetCmp < txAli->tStart)
    {
    txPos->region = vpUpstream;
    // Can't use qStart here -- when isRc, qStarts are reversed but qStart and qEnd are not
    txPos->txOffset = txAli->qStarts[0];
    txPos->gDistance = txAli->tStart - gOffset;
    }
else if (gOffsetCmp < txAli->tEnd)
    {
    int ix;
    for (ix = 0;  ix < txAli->blockCount;  ix++)
        {
        int tBlkStart = txAli->tStarts[ix];
        int tBlkEnd = tBlkStart + txAli->blockSizes[ix];
        if (!endCmp && ix > 0 && gOffset == tBlkStart && genomeHasDeletion(txAli, ix-1))
            {
            // Include adjacent skipped transcript bases to the left
            txPos->region = vpExon;
            txPos->txOffset = txAli->qStarts[ix-1] + txAli->blockSizes[ix-1];
            txPos->aliBlkIx = ix-1;
            break;
            }
        else if (endCmp && gOffset == tBlkEnd && genomeHasDeletion(txAli, ix))
            {
            // Include adjacent skipped transcript bases to the right
            txPos->region = vpExon;
            txPos->txOffset = txAli->qStarts[ix+1];
            txPos->aliBlkIx = ix+1;
            break;
            }
        else if (gOffsetCmp >= tBlkStart && gOffsetCmp < tBlkEnd)
            {
            // Normal exonic base
            txPos->region = vpExon;
            txPos->txOffset = txAli->qStarts[ix] + gOffset - tBlkStart;
            txPos->aliBlkIx = ix;
            break;
            }
        else if (ix < txAli->blockCount-1 &&
                 gOffsetCmp >= tBlkEnd && gOffsetCmp < txAli->tStarts[ix+1])
            {
            // Intronic (or skipped genomic) base
            txPos->region = vpIntron;
            vpTxPosIntronic(txPos, txAli, gOffset, ix);
            txPos->aliBlkIx = ix;
            break;
            }
        }
    }
else
    {
    txPos->region = vpDownstream;
    // Can't use qEnd here -- when isRc, qStarts are reversed but qStart and qEnd are not
    int lastIx = txAli->blockCount - 1;
    txPos->txOffset = txAli->qStarts[lastIx] + txAli->blockSizes[lastIx];
    txPos->gDistance = gOffset - txAli->tEnd;
    }
if (isRc)
    vpTxPosRc(txPos, txAli->qSize);
}

static char *getTxInRange(struct dnaSeq *txSeq, struct vpTxPosition *startPos,
                          struct vpTxPosition *endPos)
/* If [startPos, endPos) overlaps the actual transcript (i.e. is not intronic or up/down,
 * return the transcript sequence in that range (possibly empty).  If there is no overlap
 * then return NULL. */
 {
char *txRef = NULL;
if (endPos->txOffset > startPos->txOffset)
    {
    int txRefLen = endPos->txOffset - startPos->txOffset;
    txRef = cloneStringZ(txSeq->dna + startPos->txOffset, txRefLen);
    }
else if (startPos->region == vpExon || endPos->region == vpExon)
    // Exonic insertion into tx
    txRef = cloneString("");
if (txRef)
    touppers(txRef);
return txRef;
}

static void vpTxGetRef(struct vpTx *vpTx, struct dnaSeq *txSeq)
/* Set vpTx->txRef to transcript variant reference sequence from txSeq using vpTx start & end,
 * or NULL if variant has no overlap with transcript exons. */
{
freez(&vpTx->txRef);
vpTx->txRef = getTxInRange(txSeq, &vpTx->start, &vpTx->end);
}

// Google search on "minimal intron size" turned up a study of shortest known introns, ~48-50 in
// most species surveyed at the time.
#define MIN_INTRON 45

static boolean intronTooShort(struct psl *txAli, int blkIx)
/* Return TRUE if the gap following blkIx is too short to be a plausible intron. */
{
if (blkIx >= txAli->blockCount - 1 || blkIx < 0)
    errAbort("intronTooShort: blkIx %d is out of range [0, %d]", blkIx, txAli->blockCount - 1);
int intronLen = txAli->tStarts[blkIx+1] - (txAli->tStarts[blkIx] + txAli->blockSizes[blkIx]);
return (intronLen < MIN_INTRON);
}

static void spliceGenomicInRange(struct seqWindow *gSeqWin, uint gStart, uint gEnd,
                                 struct psl *txAli, boolean checkIntrons, char *buf, size_t bufSize)
/* Splice genomic exons in range into buf and reverse-complement if necessary.
 * If checkIntrons is true then include genomic sequence from "introns" that are too short to be
 * actual introns. */
{
int splicedLen = 0;
buf[0] = 0;
char *p = buf;
int ix;
for (ix = 0;  ix < txAli->blockCount;  ix++)
    {
    int tBlkStart = txAli->tStarts[ix];
    if (gEnd <= tBlkStart)
        break;
    int tBlkEnd = tBlkStart + txAli->blockSizes[ix];
    if (gStart > tBlkEnd)
        continue;
    int startInBlk = max(tBlkStart, gStart);
    int endInBlk = min(tBlkEnd, gEnd);
    int len = endInBlk - startInBlk;
    if (len > 0)
        {
        seqWindowCopy(gSeqWin, startInBlk, len, p, bufSize);
        p += len;
        bufSize -= len;
        splicedLen += len;
        }
    if (checkIntrons && ix < txAli->blockCount - 1 && intronTooShort(txAli, ix))
        {
        // It's an indel, not an intron -- add genomic sequence
        int startInIndel = max(tBlkEnd, gStart);
        int endInIndel = min(gEnd, txAli->tStarts[ix+1]);
        len = endInIndel - startInIndel;
        if (len > 0)
            {
            seqWindowCopy(gSeqWin, startInIndel, len, p, bufSize);
            p += len;
            bufSize -= len;
            splicedLen += len;
            }
        }
    }
boolean isRc = (pslQStrand(txAli) == '-');
if (isRc && splicedLen)
    reverseComplement(buf, splicedLen);
}

static boolean genomeTxMismatch(char *txRef, struct seqWindow *gSeqWin,
                                uint gStart, uint gEnd, struct psl *txAli)
/* If the variant overlaps aligned blocks then compare spliced strand-corrected genomic reference
 * sequence with transcript reference sequence.  vpTx start, end and txRef must be in place. */
{
boolean mismatch = FALSE;
if (txRef != NULL)
    {
    int bufLen = gEnd - gStart + 1;
    char splicedGSeq[bufLen];
    splicedGSeq[0] = '\0';
    spliceGenomicInRange(gSeqWin, gStart, gEnd, txAli, FALSE, splicedGSeq, sizeof(splicedGSeq));
    mismatch = differentString(splicedGSeq, txRef);
    }
return mismatch;
}

boolean vpTxPosIsInsertion(struct vpTxPosition *startPos, struct vpTxPosition *endPos)
/* Return TRUE if startPos and endPos describe a zero-length region. */
{
// Sometimes an insertion happens at the boundary between regions, in which case startPos->region
// and endPos->region will be different even though they land on the same point.
// At the boundary, endPos->region is looking left/5' and startPos->region is looking right/3'.
return (startPos->txOffset == endPos->txOffset &&
        ((startPos->gDistance == endPos->gDistance &&
          startPos->intron3TxOffset == endPos->intron3TxOffset &&
          startPos->intron3Distance == endPos->intron3Distance) ||
         // exon -> intron boundary: startPos is intronic (but gDistance is 0), endPos is exonic
         (startPos->region == vpExon && endPos->region == vpIntron &&
          startPos->gDistance == 0 && endPos->gDistance == 0) ||
         // intron -> exon boundary: startPos is exonic, endPos is intronic (but intron3Distance==0)
         (startPos->region == vpIntron && endPos->region == vpExon &&
          startPos->txOffset > 0 && startPos->gDistance == 0 && endPos->intron3Distance == 0)));
}

static int limitToExon(struct vpTx *vpTx, uint gEnd, struct psl *txAli)
/* If variant ends in an exon, return the max number of bases by which we can shift the variant
 * along the genome in the direction of transcription without running past the end of the exon
 * into a splice site.
 * See HGVS "exception 3' rule": http://varnomen.hgvs.org/bg-material/numbering/
 * If not applicable, return INDEL_SHIFT_NO_MAX. */
{
int maxShift = INDEL_SHIFT_NO_MAX;
if (vpTx->end.region == vpExon)
    {
    int blkIx = vpTx->end.aliBlkIx;
    if (txAli->strand[0] == '-')
        {
        while (blkIx > 0 && intronTooShort(txAli, blkIx-1))
            blkIx--;
        int tBlkStart = txAli->tStarts[blkIx];
        maxShift = gEnd - tBlkStart;
        }
    else
        {
        while (blkIx < txAli->blockCount - 1 && intronTooShort(txAli, blkIx))
            blkIx++;
        int tBlkEnd = txAli->tStarts[blkIx] + txAli->blockSizes[blkIx];
        maxShift = tBlkEnd - gEnd;
        }
    }
return maxShift;
}

static boolean hasAnomalousGaps(struct psl *txAli, uint gStart, uint gEnd)
/* Return TRUE if txAli has an indel between genomic start & end that is too short to be intron. */
{
int ix;
for (ix = 0;  ix < txAli->blockCount - 1;  ix++)
    {
    uint intronStart = txAli->tStarts[ix] + txAli->blockSizes[ix];
    uint intronEnd = txAli->tStarts[ix+1];
    if (gStart <= intronEnd && gEnd >= intronStart && intronTooShort(txAli, ix))
        return TRUE;
    else if (gEnd < intronStart)
        break;
    }
return FALSE;
}

static void processIndels(struct vpTx *vpTx, struct seqWindow *gSeqWin,
                          uint gStart, uint gEnd, char *gAltCpy,
                          struct psl *txAli, struct dnaSeq *txSeq)
/* If variant is an insertion or deletion, detect whether its placement is ambiguous, i.e. could
 * be shifted left or right with the same result.  Also detect whether txAli happens to have
 * a non-intronic indel in the same ambiguous region -- in that case, the genomic variant might
 * actually mean no change to the transcript so compare carefully.  If txAli is on '-' strand,
 * gTxStart can be greater than gTxEnd (i.e. they are swapped relative to the genome). */
{
int refLen = gEnd - gStart, altLen = strlen(gAltCpy);
if (! vpTx->genomeMismatch && indelShiftIsApplicable(refLen, altLen))
    {
    boolean isRc = (pslQStrand(txAli) == '-');
    // Genomic coords for transcript start and transcript end -- swapped if isRc!
    uint gTxStart = gStart, gTxEnd = gEnd;
    if (isRc)
        swapUint(&gTxStart, &gTxEnd);
    // Attempt to shift variants along the genome as far in the direction of transcription
    // as possible -- but not past an exon's 3' boundary into a splice site / intron.
    // shifting: http://andrewjesaitis.com/2017/03/the-state-of-variant-annotation-in-2017/
    // HGVS "exception 3' rule": http://varnomen.hgvs.org/bg-material/numbering/
    // Also find out how far we could shift 5' so we can detect genome/tx non-intron indels.
    int maxShift = limitToExon(vpTx, gTxEnd, txAli);
    uint gTxStart5 = gTxStart, gTxEnd5 = gTxEnd;
    char gAltCpy5[altLen+1];
    safecpy(gAltCpy5, sizeof(gAltCpy5), gAltCpy);
    uint ambigStart, ambigEnd;
    if (isRc)
        {
        indelShift(gSeqWin, &gTxEnd5, &gTxStart5, gAltCpy5, INDEL_SHIFT_NO_MAX, isdRight);
        vpTx->basesShifted = indelShift(gSeqWin, &gTxEnd, &gTxStart, gAltCpy, maxShift, isdLeft);
        ambigStart = gTxEnd;
        ambigEnd = gTxStart5;
        }
    else
        {
        indelShift(gSeqWin, &gTxStart5, &gTxEnd5, gAltCpy5, INDEL_SHIFT_NO_MAX, isdLeft);
        vpTx->basesShifted = indelShift(gSeqWin, &gTxStart, &gTxEnd, gAltCpy, maxShift, isdRight);
        ambigStart = gTxStart5;
        ambigEnd = gTxEnd;
        }
    if (vpTx->basesShifted)
        {
        // Variant was shifted on genome; re-project genomic coordinates to tx.
        vpPosGenoToTx(gTxStart, txAli, &vpTx->start, FALSE);
        vpPosGenoToTx(gTxEnd, txAli, &vpTx->end, TRUE);
        vpTxGetRef(vpTx, txSeq);
        vpTx->gRef = needMem(refLen+1);
        seqWindowCopy(gSeqWin, min(gTxStart, gTxEnd), refLen, vpTx->gRef, refLen+1);
        if (isRc)
            reverseComplement(vpTx->gRef, refLen);
        vpTx->txAlt = cloneString(gAltCpy);
        }
    if (hasAnomalousGaps(txAli, ambigStart, ambigEnd))
        {
        // The transcript and genome have a non-intron indel in this indel region,
        // so this variant could actually mean no change to the transcript.  We need to
        // carefully compare the transcript sequence to the genomic alt sequence over the
        // whole region.
        struct vpTxPosition ambigStartPos, ambigEndPos;
        vpPosGenoToTx(isRc ? ambigEnd : ambigStart, txAli, &ambigStartPos, FALSE);
        vpPosGenoToTx(isRc ? ambigStart : ambigEnd, txAli, &ambigEndPos, TRUE);
        char *ambigTxRef = getTxInRange(txSeq, &ambigStartPos, &ambigEndPos);
        // First get the reference according to the genome -- retaining genomic sequence from
        // the non-intron indel(s).
        char splicedGSeq[ambigEnd-ambigStart+1];
        spliceGenomicInRange(gSeqWin, ambigStart, ambigEnd, txAli, TRUE,
                             splicedGSeq, sizeof(splicedGSeq));
        if (differentString(ambigTxRef, splicedGSeq))
            {
            // Now modify splicedGSeq using gAltCpy.
            char gTxAlt[altLen+1];
            safecpy(gTxAlt, sizeof(gTxAlt), gAltCpy);
            if (isRc)
                reverseComplement(gTxAlt, altLen);
            int gLen = strlen(splicedGSeq);
            char gModified[gLen+altLen+1];
            int modOffset = gLen - refLen;
            safecpy(gModified, sizeof(gModified), splicedGSeq);
            safecpy(gModified+modOffset, sizeof(gModified)-modOffset, gTxAlt);
            // Use txAlt from modified genomic sequence instead of just using gAltCpy.
            int alleleOffset = vpTx->start.txOffset - ambigStartPos.txOffset;
            if (alleleOffset > strlen(gModified))
                errAbort("alleleOffset is %d but gModified length is %d",
                         alleleOffset, (int)strlen(gModified));
            vpTx->txAlt = cloneString(gModified+alleleOffset);
            }
        }
    }
}

struct vpTx *vpGenomicToTranscript(struct seqWindow *gSeqWin, struct bed3 *gBed3, char *gAlt,
                                   struct psl *txAli, struct dnaSeq *txSeq)
/* Project a genomic variant onto a transcript, trimming identical bases at the beginning and/or
 * end of ref and alt alleles and shifting ambiguous indel placements in the direction of
 * transcription except across an exon-intron boundary.
 * Both ref and alt must be [ACGTN]-only (no symbolic alleles like "." or "-" or "<DEL>").
 * This may change gSeqWin's range. */
{
int altLen = strlen(gAlt);
if (!isAllNt(gAlt, altLen
             +1)) //#*** FIXME isAllNt ignores last base in string!!! always TRUE for len=1
    errAbort("vpGenomicToTranscript: alternate allele must be sequence of IUPAC DNA characters "
             "but is '%s'", gAlt);
gSeqWin->fetch(gSeqWin, gBed3->chrom, gBed3->chromStart, gBed3->chromEnd);
struct vpTx *vpTx;
AllocVar(vpTx);
vpTx->txName = cloneString(txSeq->name);
boolean isRc = (pslQStrand(txAli) == '-');
// Genomic coords and sequence for variant -- trim identical bases from ref and alt if any:
uint gStart = gBed3->chromStart, gEnd = gBed3->chromEnd;
int refLen = gEnd - gStart;
char gRef[refLen+1];
seqWindowCopy(gSeqWin, gStart, refLen, gRef, sizeof(gRef));
char gAltCpy[altLen+1];
safecpy(gAltCpy, sizeof(gAltCpy), gAlt);
if (differentString(gRef, gAltCpy))
    // Trim identical bases from start and end -- unless this is an assertion that there is
    // no change, in which case it's good to keep the range on which that assertion was made.
    trimRefAlt(gRef, gAltCpy, &gStart, &gEnd, &refLen, &altLen);
// Even if we may later shift the variant position in the direction of transcription, first do
// an initial projection to find exon boundaries and detect mismatch between genome and tx.
vpPosGenoToTx(isRc ? gEnd : gStart, txAli, &vpTx->start, FALSE);
vpPosGenoToTx(isRc ? gStart : gEnd, txAli, &vpTx->end, TRUE);
// Compare genomic ref allele vs. txRef
vpTxGetRef(vpTx, txSeq);
vpTx->genomeMismatch = genomeTxMismatch(vpTx->txRef, gSeqWin, gStart, gEnd, txAli);
processIndels(vpTx, gSeqWin, gStart, gEnd, gAltCpy, txAli, txSeq);
// processIndels may or may not set vpTx->txAlt and vpTx->gRef
if (vpTx->txAlt == NULL)
    {
    vpTx->txAlt = cloneString(gAltCpy);
    if (isRc)
        reverseComplement(vpTx->txAlt, altLen);
    }
if (vpTx->gRef == NULL)
    {
    vpTx->gRef = cloneString(gRef);
    if (isRc)
        reverseComplement(vpTx->gRef, refLen);
    }
return vpTx;
}

void vpTxFree(struct vpTx **pVpTx)
/* Free up a vpTx. */
{
if (pVpTx)
    {
    struct vpTx *vpTx = *pVpTx;
    freeMem(vpTx->txName);
    freeMem(vpTx->txRef);
    freeMem(vpTx->txAlt);
    freeMem(vpTx->gRef);
    freez(pVpTx);
    }
}

char *translateString(char *codons)
/* Translate a string of codon DNA into a string of peptide bases.  stop codon is 'X'. */
{
struct dnaSeq *codonSeq = newDnaSeq(cloneString(codons), strlen(codons), NULL);
aaSeq *alt = translateSeq(codonSeq, 0, FALSE);
aaSeqZToX(alt);
dnaSeqFree(&codonSeq);
return dnaSeqCannibalize(&alt);
}

// How close can an intronic variant be to a splice junction without being considered a splice hit?
#define SPLICE_REGION_FUDGE 6

struct vpPep *vpTranscriptToProtein(struct vpTx *vpTx, struct genbankCds *cds,
                                    struct dnaSeq *txSeq, struct dnaSeq *protSeq)
/* Project a coding transcript variant onto a protein sequence, shifting position to the first
 * differing amino acid position.  Return NULL if no cds or incomplete cds. */
//#*** This will produce incorrect results for the rare cds with join(...) unless we make a more
//#*** complicated cds data structure to represent those (basically list of cds's) and use it here.
{
if (cds == NULL || cds->start == -1 || cds->end == -1 || !cds->startComplete)
    return NULL;
if (txSeq == NULL)
    errAbort("vpTranscriptToProtein: txSeq must not be NULL");
struct vpPep *vpPep = NULL;
AllocVar(vpPep);
vpPep->name = cloneString(protSeq->name);
uint txStart = vpTx->start.txOffset;
uint txEnd = vpTx->end.txOffset;
// If the variant starts and ends within exon(s) and overlaps CDS then predict protein change.
if (txStart < cds->end && txEnd > cds->start &&
    vpTx->start.region == vpExon && vpTx->end.region == vpExon)
    {
    if (txStart < cds->start)
        vpPep->spansUtrCds = TRUE;
    uint startInCds = max(txStart, cds->start) - cds->start;
    uint endInCds = min(txEnd, cds->end) - cds->start;
    vpPep->start = startInCds / 3;
    vpPep->end = (endInCds + 2) / 3;
    uint codonStartInCds = vpPep->start * 3;
    uint codonEndInCds = vpPep->end * 3;
    uint codonLenInCds = codonEndInCds - codonStartInCds;
    aaSeq *txTrans = translateSeqN(txSeq, cds->start + codonStartInCds,
                                   codonLenInCds, FALSE);
    aaSeqZToX(txTrans);
    // We need pSeq to end with "X" because vpPep->start can be the stop codon / terminal
    char *pSeq = protSeq->dna;
    int pLen = protSeq->size;
    char pSeqWithX[pLen+2];
    if (pSeq[pLen-1] != 'X')
        {
        safencpy(pSeqWithX, sizeof(pSeqWithX), pSeq, pLen);
        safencpy(pSeqWithX+pLen, sizeof(pSeqWithX)-pLen, "X", 1);
        pSeq = pSeqWithX;
        }
    vpPep->txMismatch = !sameStringN(txTrans->dna, pSeq+vpPep->start,
                                     vpPep->end - vpPep->start);
    int startPadding = (startInCds - codonStartInCds);
    int endPadding = codonEndInCds - endInCds;
    int txAltLen = strlen(vpTx->txAlt);
    char altCodons[txSeq->size+txAltLen+1];
    altCodons[0] = '\0';
    if (startPadding > 0)
        // Copy the unchanged first base or two of ref codons
        safencpy(altCodons, sizeof(altCodons),
                 txSeq->dna + cds->start + codonStartInCds, startPadding);
    int txRefLen = txEnd - txStart;
    uint utr5Bases = (cds->start > txStart) ? cds->start - txStart : 0;
    if (txAltLen > utr5Bases)
        // Copy in the alternate allele
        safencpy(altCodons+startPadding, sizeof(altCodons)-startPadding,
                 vpTx->txAlt + utr5Bases, txAltLen - utr5Bases);
    if (!vpPep->spansUtrCds && (txRefLen - txAltLen) % 3 != 0)
        {
        vpPep->frameshift = TRUE;
        // Extend ref to the end of the protein.
        vpPep->ref = cloneString(pSeq+vpPep->start);
        // Copy in all remaining tx sequence to see how soon we would hit a stop codon
        safecpy(altCodons+startPadding+txAltLen, sizeof(altCodons)-startPadding-txAltLen,
                txSeq->dna + txEnd);
        }
    else
        {
        vpPep->ref = cloneStringZ(pSeq+vpPep->start, vpPep->end - vpPep->start);
        if (endPadding > 0)
            // Copy the unchanged last base or two of ref codons
            safencpy(altCodons+startPadding+txAltLen,
                     sizeof(altCodons)-startPadding-txAltLen,
                     txSeq->dna + cds->start + endInCds, endPadding);
        }
    char *alt = translateString(altCodons);
    if (endsWith(vpPep->ref, "X") && !endsWith(alt, "X"))
        {
        // Stop loss -- recompute alt
        freeMem(alt);
        safecpy(altCodons+startPadding+txAltLen, sizeof(altCodons)-startPadding-txAltLen,
                txSeq->dna + txEnd);
        alt = translateString(altCodons);
        }
    vpPep->alt = alt;
    if (!vpPep->spansUtrCds)
        {
        int refLen = strlen(vpPep->ref), altLen = strlen(vpPep->alt);
        if (differentString(vpPep->ref, vpPep->alt))
            {
            // If alt has a stop codon, temporarily disguise it so it can't get trimmed
            char *altStop = strchr(vpPep->alt, 'X');
            if (altStop)
                *altStop = 'Z';
            trimRefAlt(vpPep->ref, vpPep->alt, &vpPep->start, &vpPep->end, &refLen, &altLen);
            if (altStop)
                *strchr(vpPep->alt, 'Z') = 'X';
            }
        if (indelShiftIsApplicable(refLen, altLen))
            {
            struct seqWindow *pSeqWin = memSeqWindowNew(vpPep->name, pSeq);
            vpPep->rightShiftedBases = indelShift(pSeqWin, &vpPep->start, &vpPep->end,
                                                  vpPep->alt, INDEL_SHIFT_NO_MAX, isdRight);
            memSeqWindowFree(&pSeqWin);
            }
        }
    dnaSeqFree((struct dnaSeq **)&txTrans);
    }
else if (vpTx->end.region == vpTx->start.region &&
         (vpTx->start.region == vpExon ||       // all UTR
          vpTx->start.region == vpUpstream ||   // all upstream
          vpTx->start.region == vpDownstream || // all downstream
          (vpTx->start.region == vpIntron &&
           vpTx->start.gDistance >= SPLICE_REGION_FUDGE &&
           vpTx->start.intron3Distance >= SPLICE_REGION_FUDGE &&
           vpTx->end.gDistance >= SPLICE_REGION_FUDGE &&
           vpTx->end.intron3Distance >= SPLICE_REGION_FUDGE)))  // all non-splice intronic
    {
    // Mutalyzer predicts no change when variant is in tx but not cds or non-splice intron,
    // monkey see, monkey do.  Then again, it predicts p.(=) for splice hits too, doh!
    vpPep->likelyNoChange = TRUE;
    }
else
    {
    vpPep->cantPredict = TRUE;
    }
return vpPep;
}

void vpPepFree(struct vpPep **pVp)
/* Free up a vpPep. */
{
if (pVp && *pVp)
    {
    struct vpPep *vp = *pVp;
    freeMem(vp->name);
    freeMem(vp->ref);
    freeMem(vp->alt);
    freez(pVp);
    }
}

static char *regionStrings[] =
    { "vpUnknown",
      "vpUpstream",
      "vpDownstream",
      "vpExon",
      "vpIntron",
    };

char *vpTxRegionToString(enum vpTxRegion region)
/* Return a static string for region.  Do not free result! */
{
if (region > ArraySize(regionStrings))
    errAbort("vpTxRegionToString: invalid region %d", region);
return regionStrings[region];
}

void vpTxPosSlideInSameRegion(struct vpTxPosition *txPos, int bases)
/* Move txPos's region-appropriate offsets and distances by bases (can be negative).
 * *Caller must ensure that this will not slide us into another region!* */
{
if (txPos->region == vpIntron)
    {
    txPos->gDistance += bases;
    txPos->intron3Distance -= bases;
    }
else if (txPos->region == vpExon)
    txPos->txOffset += bases;
else if (txPos->region == vpUpstream)
    txPos->gDistance -= bases;
else if (txPos->region == vpDownstream)
    txPos->gDistance += bases;
else
    errAbort("vpTxPosSlideInSameRegion: unrecognized region %d", txPos->region);
}

boolean vpTxPosRangeIsSingleBase(struct vpTxPosition *startPos, struct vpTxPosition *endPos)
/* Return true if [startPos, endPos) is a single-base region. */
{
if (startPos->region == vpUpstream && endPos->region == vpUpstream &&
    startPos->gDistance == endPos->gDistance + 1)
    return TRUE;
if (startPos->region == vpExon && endPos->region == vpExon &&
    startPos->txOffset + 1 == endPos->txOffset)
    return TRUE;
if (startPos->region == vpIntron && endPos->region == vpIntron &&
    startPos->txOffset == endPos->txOffset && startPos->gDistance + 1 == endPos->gDistance)
    return TRUE;
if (startPos->region == vpDownstream && endPos->region == vpDownstream &&
    startPos->gDistance + 1 == endPos->gDistance)
    return TRUE;
return FALSE;
}
