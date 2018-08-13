/* gpFx --- routines to calculate the effect of variation on a genePred */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "genePred.h"
#include "gpFx.h"

static char *collapseDashes(char *str)
// Trim extra hyphen characters at end of str.
{
int len = strlen(str);
if (len > 1)
    {
    char *p = str + len - 1;
    while (*p == '-' && p > str)
	*p-- = '\0';
    }
return str;
}

struct txCoords
/* Start and end coords, projected into exon/intron numbers, cDNA offsets and CDS offsets. */
    {
    boolean startInExon; // True if start coord is in an exon, false if in intron or up/downstream
    int startExonIx;     // Exon number if startInExon, intron number if >= 0, up/down if < 0
    int startInCdna;     // Start offset in cDNA if >= 0, n/a if < 0
    int startInCds;      // Start offset in CDS if >= 0, n/a if < 0
    boolean endInExon;   // True if end coord is in an exon, false if in intron or up/downstream
    int endExonIx;       // Exon number if endInExon, intron number if >= 0, up/down if < 0
    int endInCdna;       // End offset in cDNA if > 0, n/a if <= 0
    int endInCds;        // End offset in CDS if > 0, n/a if <= 0
    // Info needed for strand-swapping:
    int cdnaSize;        // Length of transcribed sequence
    int cdsSize;         // Length of translated coding sequence
    int exonCount;       // Number of exons
    char strand;         // '+' or '-'
    };

static void txCoordsInit(struct txCoords *txc, int exonCount, char strand,
			 int cdnaSize, int cdsSize)
/* Set txc's values to defaults that don't place the start and end anywhere in a transcript. */
{
txc->startInExon = FALSE;
txc->startExonIx = -1;            // i.e. upstream unless found in transcript
txc->startInCdna = -1;            // n/a unless found in exon
txc->startInCds = -1;             // n/a unless found in CDS
txc->endInExon = FALSE;
txc->endExonIx = exonCount-1;     // intron[lastExonIx], i.e. downstream unless found in transcript
txc->endInCdna = -1;              // n/a unless found in exon
txc->endInCds = -1;               // n/a unless found in CDS
txc->cdnaSize = cdnaSize;
txc->cdsSize = cdsSize;
txc->exonCount = exonCount;
txc->strand = strand;
}

static struct txCoords txCoordsReverse(struct txCoords *txcIn)
/* Return a struct txCoords with same info as txcIn, but strand-swapped. */
{
if (txcIn->exonCount <= 0 || txcIn->cdnaSize <= 0)
    errAbort("txCoordsReverse: called with non-positive exonCount (%d) or cdnaSize (%d) ",
	     txcIn->exonCount, txcIn->cdnaSize);
struct txCoords txcSwapped;
char swappedStrand = (txcIn->strand == '+') ? '-' : '+';
txCoordsInit(&txcSwapped, txcIn->exonCount, swappedStrand, txcIn->cdnaSize, txcIn->cdsSize);
if (txcIn->startInExon)
    {
    txcSwapped.endInExon = TRUE;
    txcSwapped.endExonIx = txcIn->exonCount - 1 - txcIn->startExonIx;
    }
else
    {
    // Intron number, not exon number, so subtract another 1 here:
    txcSwapped.endExonIx = txcIn->exonCount - 2 - txcIn->startExonIx;
    }
if (txcIn->startInCdna >= 0)
    txcSwapped.endInCdna = txcIn->cdnaSize - txcIn->startInCdna;
if (txcIn->startInCds >= 0)
    txcSwapped.endInCds = txcIn->cdsSize - txcIn->startInCds;
if (txcIn->endInExon)
    {
    txcSwapped.startInExon = TRUE;
    txcSwapped.startExonIx = txcIn->exonCount - 1 - txcIn->endExonIx;
    }
else
    {
    // Intron number, not exon number, so subtract another 1 here:
    txcSwapped.startExonIx = txcIn->exonCount - 2 - txcIn->endExonIx;
    }
if (txcIn->endInCdna > 0)
    txcSwapped.startInCdna = txcIn->cdnaSize - txcIn->endInCdna;
if (txcIn->endInCds > 0)
    txcSwapped.startInCds = txcIn->cdsSize - txcIn->endInCds;
return txcSwapped;
}

static struct txCoords getTxCoords(struct variant *variant, struct genePred *pred)
/* Figure out where the variant's start and end hit the transcript: intron, UTR, CDS?
 * Result is on pred->strand. */
{
struct txCoords txc;
txCoordsInit(&txc, pred->exonCount, '+', 0, 0); // Compute cdnaSize, cdsSize below.
int exonOffset = 0, cdsOffset = 0;
uint varStart = variant->chromStart, varEnd = variant->chromEnd;
// If the variant begins upstream, handle that before looping on exons:
if (varStart < pred->txStart && varEnd > pred->txStart)
    {
    txc.startInCdna = 0;
    if (varStart < pred->cdsEnd && varEnd > pred->cdsStart)
	txc.startInCds = 0;
    }
int ii;
for (ii = 0;  ii < pred->exonCount;  ii++)
    {
    uint exonStart = pred->exonStarts[ii], exonEnd = pred->exonEnds[ii];
    uint exonCdsStart = max(pred->cdsStart, exonStart);
    uint exonCdsEnd = min(pred->cdsEnd, exonEnd);
    uint exonCdsSize = 0;
    if (exonCdsEnd > exonCdsStart)
	exonCdsSize = exonCdsEnd - exonCdsStart;
    if (varStart >= exonStart && varStart < exonEnd)
	{
	txc.startInExon = TRUE;
	txc.startExonIx = ii;
	txc.startInCdna = exonOffset + varStart - exonStart;
	if (varStart >= pred->cdsStart && varStart < pred->cdsEnd)
	    txc.startInCds = cdsOffset + varStart - exonCdsStart;
	else if (varStart < pred->cdsStart && varEnd > pred->cdsStart)
	    // Variant spans the left UTR/CDS boundary; set cdsStart to 0:
	    txc.startInCds = 0;
	// If this is an insertion at the beginning of an exon, varEnd is at the end
	// of the preceding intron and its endInC* have not been set, so copy them over:
	if (varEnd == varStart)
	    {
	    txc.endInCdna = txc.startInCdna;
	    txc.endInCds = txc.startInCds;
	    }
	}
    if (varEnd > exonStart && varEnd <= exonEnd)
	{
	txc.endInExon = TRUE;
	txc.endExonIx = ii;
	txc.endInCdna = exonOffset + varEnd - exonStart;
	if (varEnd > pred->cdsStart && varEnd <= pred->cdsEnd)
	    txc.endInCds = cdsOffset + varEnd - exonCdsStart;
	else if (varEnd > pred->cdsEnd && varStart < pred->cdsEnd)
	    // Variant spans the right CDS/UTR boundary; set cdsEnd to cdsSize:
	    txc.endInCds = cdsOffset + exonCdsSize;
	// If this is an insertion at the end of an exon, varStart is at the beginning
	// of the following intron and its startInC* have not been set, so copy them over:
	if (varStart == varEnd)
	    {
	    txc.startInCdna = txc.endInCdna;
	    txc.startInCds = txc.endInCds;
	    }
	}
    if (ii < pred->exonCount - 1)
	{
	uint nextExonStart = pred->exonStarts[ii+1];
	// 'exonIx' is actually an intronIx in this case:
	if (varStart >= exonEnd && varStart < nextExonStart)
	    {
	    txc.startExonIx = ii;
	    if (varEnd > nextExonStart)
		{
		// Variant starts in an intron, but it overlaps the next exon;
		// note the start in cDNA (and CDS if applicable):
		txc.startInCdna = exonOffset + exonEnd - exonStart;
		if (varStart < pred->cdsEnd && varEnd > pred->cdsStart)
		    {
		    uint nextExonEnd = pred->exonEnds[ii+1];
		    if (nextExonEnd > pred->cdsStart)
			txc.startInCds = cdsOffset + exonCdsSize;
		    else
			txc.startInCds = 0;
		    }
		}
	    }
	if (varEnd > exonEnd && varEnd <= nextExonStart)
	    {
	    txc.endExonIx = ii;
	    if (varStart < exonEnd)
		{
		// Variant ends in an intron, but it also overlaps the previous exon;
		// note the end in cDNA (and CDS if applicable):
		txc.endInCdna = exonOffset + exonEnd - exonStart;
		if (varEnd > pred->cdsStart && varStart < pred->cdsEnd)
		    {
		    if (exonStart < pred->cdsEnd)
			txc.endInCds = cdsOffset + exonCdsSize;
		    else
			txc.endInCds = cdsOffset;
		    }
		}
	    }
	}
    exonOffset += exonEnd - exonStart;
    cdsOffset += exonCdsSize;
    }
txc.cdnaSize = exonOffset;
txc.cdsSize = cdsOffset;
// Does variant end downstream?
if (varEnd > pred->txEnd)
    {
    txc.endInCdna = txc.cdnaSize;
    if (varStart < pred->cdsEnd && varEnd > pred->cdsStart)
	txc.endInCds = txc.cdsSize;
    }
if (pred->strand[0] == '-')
    txc = txCoordsReverse(&txc);
if ((txc.startInCdna == -1) != (txc.endInCdna == -1) ||
    (txc.startInCds >= 0 && txc.endInCds < 0))
    errAbort("getTxCoords: inconsistent start/ends for variant %s:%d-%d in %s at %s:%d-%d: "
	     "startInCdna=%d, endInCdna=%d;  startInCds=%d, endInCds=%d",
	     variant->chrom, varStart+1, varEnd,
	     pred->name, pred->chrom, pred->txStart, pred->txEnd,
	     txc.startInCdna, txc.endInCdna, txc.startInCds, txc.endInCds);
return txc;
}

struct gpFx *gpFxNew(char *gAllele, char *transcript, enum soTerm soNumber,
		     enum detailType detailType, struct lm *lm)
/* Fill in the common members of gpFx; leave soTerm-specific members for caller to fill in. */
{
struct gpFx *effect;
lmAllocVar(lm, effect);
effect->gAllele = collapseDashes(lmCloneString(lm, gAllele));
if (isAllNt(effect->gAllele, strlen(effect->gAllele)))
    touppers(effect->gAllele);
effect->transcript = lmCloneString(lm, transcript);
effect->soNumber = soNumber;
effect->detailType = detailType;
return effect;
}

static char * mergeAllele(char *transcript, int offset, int variantWidth,
			  char *newAlleleSeq, int alleleLength, struct lm *lm)
/* merge a variant into an allele */
{
char *newTranscript = NULL;
//#*** This will be incorrect for an MNV that spans exon boundary --
//#*** so we should also clip allele to cds portion(s?!) before calling this.
if (variantWidth == alleleLength)
    {
    newTranscript = lmCloneString(lm, transcript);
    memcpy(&newTranscript[offset], newAlleleSeq, alleleLength);
    }
else 
    {
    int insertionSize = alleleLength - variantWidth;
    int newLength = strlen(transcript) + insertionSize;
    newTranscript = lmAlloc(lm, newLength + 1);
    char *restOfTranscript = &transcript[offset + variantWidth];

    // copy over the part before the variant
    memcpy(newTranscript, transcript, offset);

    // copy in the new allele
    memcpy(&newTranscript[offset], newAlleleSeq, alleleLength);

    // copy in the part after the variant
    memcpy(&newTranscript[offset + alleleLength], restOfTranscript, 
	strlen(restOfTranscript) + 1);
    }

return newTranscript;
}

void gpFxSetNoncodingInfo(struct gpFx *gpFx, int exonIx, int exonCount, int cdnaPos,
                          char *txRef, char *txAlt, struct lm *lm)
/* This gpFx is for a variant in exon of non-coding gene or UTR exon of coding gene;
 * set details.nonCodingExon values. */
{
gpFx->details.nonCodingExon.exonNumber = exonIx;
gpFx->details.nonCodingExon.exonCount = exonCount;
gpFx->details.nonCodingExon.cDnaPosition = cdnaPos;
gpFx->details.nonCodingExon.txRef = lmCloneString(lm, txRef);
gpFx->details.nonCodingExon.txAlt = lmCloneString(lm, txAlt);
}

static struct gpFx *gpFxCheckUtr( struct allele *allele, struct genePred *pred,
				  struct txCoords *txc, int exonIx, boolean predIsNmd,
				  char *txRef, char *txAlt, struct lm *lm)
/* check for effects in UTR of coding gene -- caller ensures it's in exon, pred is coding
 * and exonIx has been strand-adjusted */
{
struct gpFx *gpFx = NULL;
enum soTerm term = 0;
struct variant *variant = allele->variant;
if ((variant->chromStart < pred->cdsStart && variant->chromEnd > pred->txStart) ||
    (variant->chromStart == pred->cdsStart && variant->chromEnd == pred->cdsStart)) // insertion
    // we're in left UTR
    term = (*pred->strand == '-') ? _3_prime_UTR_variant : _5_prime_UTR_variant;
else if ((variant->chromStart < pred->txEnd && variant->chromEnd > pred->cdsEnd) ||
	 (variant->chromStart == pred->cdsEnd && variant->chromEnd == pred->cdsEnd)) //insertion
    // we're in right UTR
    term = (*pred->strand == '-') ? _5_prime_UTR_variant : _3_prime_UTR_variant;
if (term != 0)
    {
    if (predIsNmd)
	// This transcript is already subject to nonsense-mediated decay, so the effect
	// is probably not a big deal:
	term = NMD_transcript_variant;
    gpFx = gpFxNew(allele->sequence, pred->name, term, nonCodingExon, lm);
    gpFxSetNoncodingInfo(gpFx, exonIx, pred->exonCount, txc->startInCdna, txRef, txAlt, lm);
    }
return gpFx;
}

static struct gpFx *gpFxChangedNoncodingExon(struct allele *allele, struct genePred *pred,
					     struct txCoords *txc, int exonIx,
                                             char *txRef, char *txAlt, struct lm *lm)
/* generate an effect for a variant in a non-coding transcript */
{
struct gpFx *gpFx = gpFxNew(allele->sequence, pred->name, non_coding_transcript_exon_variant,
                            nonCodingExon, lm);
gpFxSetNoncodingInfo(gpFx, exonIx, pred->exonCount, txc->startInCdna, txRef, txAlt, lm);
return gpFx;
}

static int getCodingOffsetInTx(struct genePred *pred, char strand)
/* Skip past UTR (portions of) exons to get offset of CDS relative to transcript start.
 * The strand arg is used instead of pred->strand. */
{
int offset = 0;
int iStart = 0, iIncr = 1;
boolean isRc = (strand == '-');
if (isRc)
    {
    // Work our way left from the last exon.
    iStart = pred->exonCount - 1;
    iIncr = -1;
    }
int ii;
// trim off the 5' UTR (or 3' UTR if strand is '-')
for (ii = iStart; ii >= 0 && ii < pred->exonCount; ii += iIncr)
    {
    if ((!isRc &&
	 (pred->cdsStart >= pred->exonStarts[ii]) && (pred->cdsStart < pred->exonEnds[ii])) ||
	(isRc && (pred->cdsEnd > pred->exonStarts[ii]) && (pred->cdsEnd <= pred->exonEnds[ii])))
	break;
    int exonSize = pred->exonEnds[ii] - pred->exonStarts[ii];
    offset += exonSize;
    }
if (strand == '+' && ii >= pred->exonCount)
    errAbort("getCodingOffsetInTx: exon number overflow (strand=+, exonCount=%d, num=%d)",
	     pred->exonCount, ii);
if (strand == '-' && ii < 0)
    errAbort("getCodingOffsetInTx: exon number underflow (strand=-, exonCount=%d, num=%d)",
	     pred->exonCount, ii);
// clip off part of UTR in exon that has CDS in it too
int exonOffset = pred->cdsStart - pred->exonStarts[ii];
if (strand == '-')
    exonOffset = pred->exonEnds[ii] - pred->cdsEnd;
offset += exonOffset;
return offset;
}

static char *getCodingSequenceSimple(struct genePred *pred, char *transcriptSequence, struct lm *lm)
/* Extract the CDS from a transcript, assuming frame is 0 for all coding exons.
 * Temporarily force transcriptSequence to + strand so we can work in + strand coords,
 * then restore transcriptSequence and put result on correct strand. */
{
if (*pred->strand == '-')
    reverseComplement(transcriptSequence, strlen(transcriptSequence));

// Get leftmost CDS boundary to trim off 5' (or 3' if on minus strand):
int cdsOffset = getCodingOffsetInTx(pred, '+');
char *newString = lmCloneString(lm, transcriptSequence + cdsOffset);

// trim off 3' (or 5' if on minus strand)
newString[genePredCdsSize(pred)] = 0;

// correct for strand
if (*pred->strand == '-')
    {
    reverseComplement(transcriptSequence, strlen(transcriptSequence));
    reverseComplement(newString, strlen(newString));
    }

return newString;
}

INLINE int calcMissingBases(struct genePred *pred, int exonIx, int cdsOffset)
/* If pred's exonFrame differs from the frame that we expect based on number
 * of CDS bases collected so far, return the number of 'N's we need to add
 * in order to restore the reading frame. */
{
int missingBases = 0;
int exonFrame = pred->exonFrames[exonIx];
if (exonFrame >= 0)
    {
    int startingFrame = cdsOffset % 3;
    missingBases = exonFrame - startingFrame;
    if (missingBases < 0)
	missingBases += 3;
    }
return missingBases;
}

static char *getCodingSequence(struct genePred *pred, char *transcriptSequence,
			       boolean *retAddedBases, struct lm *lm)
/* Extract the CDS sequence from a transcript.  If pred has exonFrames, add 'N' where
 * needed (for example, if the coding region begins out-of-frame, add one or two 'N's
 * at the beginning of the cds sequence) and set retAddedBases if we do add 'N'.
 * If pred doesn't have exonFrames, use the simple method above. */
{
if (retAddedBases)
    *retAddedBases = FALSE;
if (pred->optFields & genePredExonFramesFld)
    {
    boolean isRc = (pred->strand[0] == '-');
    int i, iStart = 0, iIncr = 1;
    if (isRc)
	{
	iStart = pred->exonCount-1;
	iIncr = -1;
	}
    char *cdsSeq = lmAlloc(lm, genePredCdsSize(pred) + 3 * pred->exonCount);
    int txOffset = getCodingOffsetInTx(pred, pred->strand[0]), cdsOffset = 0;
    for (i = iStart;  i >= 0 && i < pred->exonCount;  i += iIncr)
	{
	int start, end;
	if (genePredCdsExon(pred, i, &start, &end))
	    {
	    int exonCdsSize = end - start;
	    int missingBases = calcMissingBases(pred, i, cdsOffset);
	    if (missingBases > 0)
		{
		if (retAddedBases)
		    *retAddedBases = TRUE;
		while (missingBases > 0)
		    {
		    cdsSeq[cdsOffset++] = 'N';
		    missingBases--;
		    }
		}
	    memcpy(&cdsSeq[cdsOffset], &transcriptSequence[txOffset], exonCdsSize);
	    cdsOffset += exonCdsSize;
	    txOffset += exonCdsSize;
	    }
	}
    return cdsSeq;
    }
else
    return getCodingSequenceSimple(pred, transcriptSequence, lm);
}

static int getCorrectedCdsOffset(struct genePred *pred, int cdsOffsetIn)
/* Increment cdsOffset for each 'N' that getCodingSequence added prior to it. */
{
int totalMissingBases = 0;
int cdsOffsetSoFar = 0;
if (pred->optFields & genePredExonFramesFld)
    {
    boolean isRc = (pred->strand[0] == '-');
    int i, iStart = 0, iIncr = 1;
    if (isRc)
	{
	iStart = pred->exonCount-1;
	iIncr = -1;
	}
    for (i = iStart;  i >= 0 && i < pred->exonCount;  i += iIncr)
	{
	int start, end;
	if (genePredCdsExon(pred, i, &start, &end))
	    {
	    // Don't count missing bases after cdsOffsetIn:
	    if (cdsOffsetSoFar > cdsOffsetIn)
		break;
	    int exonCdsSize = end - start;
	    totalMissingBases += calcMissingBases(pred, i, cdsOffsetSoFar + totalMissingBases);
	    cdsOffsetSoFar += exonCdsSize;
	    }
	}
    }
return cdsOffsetIn + totalMissingBases;
}

static char *lmSimpleTranslate(struct lm *lm, char *dna, int size)
/* Just translate dna into pep, use 'Z' and truncate for stop codon, and use localmem. */
{
int aaLen = (size / 3) + 1;
char *pep = lmAlloc(lm, aaLen + 1);
int i;
for (i = 0;  i < size;  i += 3)
    {
    char aa = lookupCodon(dna + i);
    pep[i/3] = aa;
    if (aa == '\0')
	{
	pep[i/3] = 'Z';
	break;
	}
    }
if (i < size && pep[i/3] != 'Z')
    // incomplete final codon
    pep[i/3] = 'X';
return pep;
}

static void truncateAtStopCodon(char *codingSeq)
/* If codingSeq contains a stop codon, truncate any sequence past that. */
{
if (codingSeq == NULL)
    errAbort("truncateAtStopCodon: null input");
char *p = codingSeq;
while (p[0] != '\0' && p[1] != '\0' && p[2] != '\0')
    {
    if (isStopCodon(p))
	{
	p[3] = '\0';
	break;
	}
    p += 3;
    }
}

static char *gpFxModifyCodingSequence(char *oldCodingSeq, int startInCds, int endInCds, char *alt,
				      int *retCdsBasesAdded, struct lm *lm)
/* Return a new coding sequence that is oldCodingSeq with allele applied. */
{
char *newAlleleSeq = alt;
int newAlLen = strlen(newAlleleSeq);
if (! isAllNt(newAlleleSeq, newAlLen))
    {
    // symbolic -- may be deletion or insertion, but we can't tell. :(
    newAlleleSeq = "";
    newAlLen = 0;
    }
int variantSizeOnCds = endInCds - startInCds;
if (variantSizeOnCds < 0)
    errAbort("gpFx: endInCds (%d) < startInCds (%d)", endInCds, startInCds);
char *newCodingSeq = mergeAllele(oldCodingSeq, startInCds, variantSizeOnCds,
				 newAlleleSeq, newAlLen, lm);
// If newCodingSequence has an early stop, truncate there:
truncateAtStopCodon(newCodingSeq);
if (retCdsBasesAdded)
    *retCdsBasesAdded = newAlLen - variantSizeOnCds;
return newCodingSeq;
}

static void setSpecificCodingSoTerm(struct gpFx *effect, char *oldAa, char *newAa,
				    int cdsBasesAdded)
/* Assuming that deletions are marked with dashes in newCodingSequence, 
 * and that effect fields aside from soNumber are already populated, use the
 * pep seqs and number of dashes to determine the appropriate SO term.
 * Probably the majority of cases will be synonymous_variant or missense_variant,
 * but we have to check for several other special cases esp. indels. */
{
struct codingChange *cc = &effect->details.codingChange;
int oldAaSize = strlen(oldAa), newAaSize = strlen(newAa);
if (sameString(newAa, oldAa))
    {
    if (cc->pepPosition == oldAaSize-1 && cc->aaOld[0] == 'Z')
	effect->soNumber = stop_retained_variant;
    else
	effect->soNumber = synonymous_variant;
    }
else
    {
    if (cdsBasesAdded < 0)
	{
	// Got a deletion variant -- check frame (and whether we lost a stop codon):
	if ((cdsBasesAdded % 3) == 0)
	    {
	    if (strchr(cc->aaOld, 'Z') && !strchr(cc->aaNew, 'Z'))
		effect->soNumber = stop_lost;
	    else
		effect->soNumber = inframe_deletion;
	    }
	else
	    effect->soNumber = frameshift_variant;
	}
    else
	{
	// Not a deletion; could be single-base (including early stop) or insertion
	if (newAaSize < oldAaSize)
	    {
	    // Not a deletion but protein got smaller; must have been an early stop codon,
	    // possibly inserted or following a frameshift caused by an insertion.
	    int frame = cc->cdsPosition % 3;
	    int alleleLength = strlen(cc->txAlt);
	    if (! isAllNt(cc->txAlt, alleleLength))
		// symbolic -- may be deletion or insertion, but we can't tell. :(
		alleleLength = 0;
	    int i, affectedCodons = (frame + alleleLength + 2) / 3;
	    boolean stopGain = FALSE;
	    for (i = 0;  i < affectedCodons;  i++)
		if (cc->aaNew[i] == 'Z')
		    {
		    effect->soNumber = stop_gained;
		    stopGain = TRUE;
		    break;
		    }
	    if (! stopGain)
		{
		if (newAa[newAaSize-1] != 'Z')
		    errAbort("gpFx: new protein is smaller but last base in new sequence "
			     "is '%c' not 'Z'.\n"
			     "oldAa (%daa): %s\nnewAa (%daa): %s\n"
			     , newAa[newAaSize-1], oldAaSize, oldAa, newAaSize, newAa);
		effect->soNumber = frameshift_variant;
		}
	    }
	else if (newAaSize > oldAaSize)
	    {
	    // protein got bigger; insertion or lost stop codon
	    if (cc->aaOld[0] == 'Z')
		effect->soNumber = stop_lost;
	    else if ((cdsBasesAdded % 3) == 0)
		effect->soNumber = inframe_insertion;
	    else
		effect->soNumber = frameshift_variant;
	    }
	else
	    {
	    // Single aa change
	    if (cc->pepPosition == 0 && cc->aaOld[0] == 'M')
		effect->soNumber = initiator_codon_variant;
	    else if (cc->pepPosition == oldAaSize-1)
		{
		if (oldAa[oldAaSize-1] == 'Z')
		    effect->soNumber = stop_lost;
		else
		    effect->soNumber = incomplete_terminal_codon_variant;
		}
	    else
		effect->soNumber = missense_variant;
	    }
	}
    }
}

static struct gpFx *gpFxChangedCds(struct allele *allele, struct genePred *pred,
				   struct txCoords *txc, int exonIx, boolean predIsNmd,
				   struct dnaSeq *transcriptSequence, char *txRef, char *txAlt,
                                   struct lm *lm)
/* calculate effect of allele change on coding transcript */
{
// calculate original and variant coding DNA and AA's
boolean addedBasesForFrame = FALSE;
char *oldCodingSequence = getCodingSequence(pred, transcriptSequence->dna, &addedBasesForFrame, lm);
int startInCds = txc->startInCds, endInCds = txc->endInCds;
if (addedBasesForFrame)
    {
    // The annotated CDS exons were not all in frame, so getCodingSequence added 'N's
    // and now we can't simply use txc->startInCds.
    startInCds = getCorrectedCdsOffset(pred, txc->startInCds);
    endInCds = getCorrectedCdsOffset(pred, txc->endInCds);
    }
int oldCdsLen = strlen(oldCodingSequence);
char *oldaa = lmSimpleTranslate(lm, oldCodingSequence, oldCdsLen);
int cdsBasesAdded = 0;
char *newCodingSequence = gpFxModifyCodingSequence(oldCodingSequence, startInCds, endInCds,
						   txAlt, &cdsBasesAdded, lm);
int newCdsLen = strlen(newCodingSequence);
char *newaa = lmSimpleTranslate(lm, newCodingSequence, newCdsLen);


// allocate the effect structure - fill in soNumber and details below
struct gpFx *effect = gpFxNew(allele->sequence, pred->name, coding_sequence_variant, codingChange,
			      lm);
struct codingChange *cc = &effect->details.codingChange;
cc->cDnaPosition = txc->startInCdna;
cc->txRef = lmCloneString(lm, txRef);
cc->txAlt = lmCloneString(lm, txAlt);
cc->cdsPosition = startInCds;
cc->exonNumber = exonIx;
cc->exonCount = pred->exonCount;
int pepPos = startInCds / 3;
// At this point we don't use genePredExt's exonFrames field -- we just assume that
// the CDS starts in frame.  That's not always the case (e.g. ensGene has some CDSs
// that begin out of frame), so watch out for early truncation of oldCodingSequence
// due to stop codon in the wrong frame:
if (pepPos >= strlen(oldaa))
    return effect;
cc->pepPosition = pepPos;
if (cdsBasesAdded % 3 == 0)
    {
    // Common case: substitution, same number of old/new codons/peps:
    int refPepEnd = (endInCds + 2) / 3;
    int numOldCodons = refPepEnd - pepPos, numNewCodons = numOldCodons;
    if (cdsBasesAdded > 0)
	{
	// insertion: more new codons than old
	numOldCodons = (cc->cdsPosition % 3) == 0 ? 0 : 1;
	numNewCodons = numOldCodons + (cdsBasesAdded / 3);
	}
    else if (cdsBasesAdded < 0)
	{
	// deletion: more old codons than new
	numNewCodons = (cc->cdsPosition % 3) == 0 ? 0 : 1;
	numOldCodons = numNewCodons + (-cdsBasesAdded / 3);
	}
    cc->codonOld = lmCloneStringZ(lm, oldCodingSequence + pepPos*3, numOldCodons*3);
    cc->codonNew = lmCloneStringZ(lm, newCodingSequence + pepPos*3, numNewCodons*3);
    cc->aaOld = lmCloneStringZ(lm, oldaa + pepPos, numOldCodons);
    cc->aaNew = lmCloneStringZ(lm, newaa + pepPos, numNewCodons);
    }
else
    {
    // frameshift -- who knows how many codons we can reliably predict...
    cc->codonOld = lmCloneString(lm, oldCodingSequence + pepPos*3);
    cc->codonNew = lmCloneString(lm, newCodingSequence + pepPos*3);
    cc->aaOld = lmCloneString(lm, oldaa + pepPos);
    cc->aaNew = lmCloneString(lm, newaa + pepPos);
    }
if (cdsBasesAdded != 0)
    {
    // indel; trim identical bases at the beginning/end, except for stop codon at end.
    int pepLen = strlen(cc->aaOld);
    uint pepEnd = pepPos + pepLen;
    boolean endsWithStop = (cc->aaOld[pepLen-1] == 'Z');
    if (endsWithStop)
        cc->aaOld[pepLen-1] = '!';
    int placeholder = 0;
    trimRefAlt(cc->aaOld, cc->aaNew, &cc->pepPosition, &pepEnd, &placeholder, &placeholder);
    if (endsWithStop)
        cc->aaOld[strlen(cc->aaOld)-1] = 'Z';
    }

if (predIsNmd)
    // This transcript is already subject to nonsense-mediated decay, so the effect
    // is probably not a big deal:
    effect->soNumber = NMD_transcript_variant;
else
    setSpecificCodingSoTerm(effect, oldaa, newaa, cdsBasesAdded);
return effect;
}


boolean hasAltAllele(struct allele *alleles)
/* Return TRUE if alleles include at least one non-reference allele. */
{
while (alleles != NULL && alleles->isReference)
    alleles = alleles->next;
return (alleles != NULL);
}

char *firstAltAllele(struct allele *alleles)
/* Ensembl always reports an alternate allele, even if that allele is not being used
 * to calculate any consequence.  When allele doesn't really matter, just use the
 * first alternate allele that is given. */
{
while (alleles != NULL && alleles->isReference)
    alleles = alleles->next;
if (alleles == NULL)
    errAbort("firstAltAllele: no alt allele in list");
return alleles->sequence;
}

static struct gpFx *gpFxInExon(struct variant *variant, struct txCoords *txc, int exonIx,
			       struct genePred *pred, boolean predIsNmd,
			       struct dnaSeq *transcriptSeq, struct lm *lm)
/* Given a variant that overlaps an exon of pred, figure out what each allele does. */
{
struct gpFx *effectsList = NULL;
struct allele *allele = variant->alleles;
for ( ; allele ; allele = allele->next)
    {
    if (!allele->isReference)
	{
        // Trim redundant bases from transcript ref and alt and adjust cdna and cds coords.
        struct txCoords txcTrim = *txc;
        int refLen = txcTrim.endInCdna - txcTrim.startInCdna;
        char txRef[refLen + 1];
        safencpy(txRef, sizeof(txRef), transcriptSeq->dna + txcTrim.startInCdna, refLen);
        touppers(txRef);
        int altLen = strlen(allele->sequence);
        char txAlt[altLen + 1];
        safecpy(txAlt, sizeof(txAlt), allele->sequence);
        if (pred->strand[0] == '-')
            reverseComplement(txAlt, altLen);
        trimRefAlt(txRef, txAlt, (uint *)&txcTrim.startInCdna, (uint *)&txcTrim.endInCdna,
                   &refLen, &altLen);
        if (txcTrim.startInCds >= 0)
            txcTrim.startInCds += (txcTrim.startInCdna - txc->startInCdna);
        if (txcTrim.endInCds >= 0)
            txcTrim.endInCds += (txcTrim.endInCdna - txc->endInCdna);
	if (pred->cdsStart != pred->cdsEnd)
	    {
	    // first find effects of allele in UTR, if any
	    effectsList = slCat(effectsList,
				gpFxCheckUtr(allele, pred, &txcTrim, exonIx, predIsNmd,
                                             txRef, txAlt, lm));
	    if (txcTrim.startInCds >= 0)
		effectsList = slCat(effectsList,
				    gpFxChangedCds(allele, pred, &txcTrim, exonIx, predIsNmd,
						   transcriptSeq, txRef, txAlt, lm));
	    }
	else
	    effectsList = slCat(effectsList,
				gpFxChangedNoncodingExon(allele, pred, &txcTrim, exonIx,
                                                          txRef, txAlt, lm));
	if (!predIsNmd)
	    {
	    // Was entire exon deleted?
	    int exonNumPos = exonIx;
	    if (pred->strand[0] == '-')
		exonNumPos = pred->exonCount - 1 - exonIx;
	    uint exonStart = pred->exonStarts[exonNumPos], exonEnd = pred->exonEnds[exonNumPos];
	    if (variant->chromStart <= exonStart && variant->chromEnd >= exonEnd)
		{
		struct gpFx *effect = gpFxNew(allele->sequence, pred->name, exon_loss_variant,
					      nonCodingExon, lm);
		gpFxSetNoncodingInfo(effect, exonIx, pred->exonCount, txcTrim.startInCdna,
                                     txRef, txAlt, lm);
		slAddTail(&effectsList, effect);
		}
	    else
		{
		// If variant is in exon *but* within 3 bases of splice site,
		// it also qualifies as splice_region_variant:
		if ((variant->chromEnd > exonEnd-3 && variant->chromStart < exonEnd &&
		     exonIx < pred->exonCount - 1) ||
		    (variant->chromEnd > exonStart && variant->chromStart < exonStart+3 &&
		     exonIx > 0))
		    {
		    struct gpFx *effect = gpFxNew(allele->sequence, pred->name,
						  splice_region_variant, nonCodingExon, lm);
		    gpFxSetNoncodingInfo(effect, exonIx, pred->exonCount, txcTrim.startInCdna,
                                         txRef, txAlt, lm);
		    slAddTail(&effectsList, effect);
		    }
		}
	    }
	}
    }
return effectsList;
}

static struct gpFx *gpFxInIntron(struct variant *variant, int intronIx,
				 struct genePred *pred, boolean predIsNmd, char *altAllele,
				 struct lm *lm)
// Annotate a variant that overlaps an intron (and possibly splice region)
//#*** TODO: watch out for "introns" that are actually indels between tx seq and ref genome!
{
struct gpFx *effectsList = NULL;
boolean minusStrand = (pred->strand[0] == '-');
// If on - strand, flip intron number back to + strand for getting intron coords:
int intronPos = minusStrand ? (pred->exonCount - intronIx - 2) : intronIx;
int intronStart = pred->exonEnds[intronPos];
int intronEnd = pred->exonStarts[intronPos+1];
if (variant->chromEnd > intronStart && variant->chromStart < intronEnd)
    {
    enum soTerm soNumber = intron_variant;
    if (variant->chromEnd > intronStart && variant->chromStart < intronStart+2)
	// Within 2 bases of intron start(/end for '-'):
	soNumber = minusStrand ? splice_acceptor_variant : splice_donor_variant;
    if (variant->chromEnd > intronEnd-2 && variant->chromStart < intronEnd)
	// Within 2 bases of intron end(/start for '-'):
	soNumber = minusStrand ? splice_donor_variant : splice_acceptor_variant;
    else if ((variant->chromEnd >= intronStart+2 && variant->chromStart < intronStart+8) ||
	     (variant->chromEnd > intronEnd-8 && variant->chromStart <= intronEnd-2))
	// Within 3 to 8 bases of intron start or end:
	soNumber = splice_region_variant;
    if (predIsNmd)
	// This transcript is already subject to nonsense-mediated decay, so the effect
	// is probably not a big deal:
	soNumber = NMD_transcript_variant;
    struct gpFx *effects = gpFxNew(altAllele, pred->name, soNumber, intron, lm);
    effects->details.intron.intronNumber = intronIx;
    effects->details.intron.intronCount = pred->exonCount - 1;
    slAddTail(&effectsList, effects);
    }
return effectsList;
}

static struct gpFx *gpFxCheckTranscript(struct variant *variant, struct genePred *pred,
					struct dnaSeq *transcriptSeq, struct lm *lm)
/* Check to see if variant overlaps an exon and/or intron of pred. */
{
struct gpFx *effectsList = NULL;
uint varStart = variant->chromStart, varEnd = variant->chromEnd;
if (varStart < pred->txEnd && varEnd > pred->txStart)
    {
    boolean predIsNmd = genePredNmdTarget(pred);
    char *defaultAltAllele = firstAltAllele(variant->alleles);
    struct txCoords txc = getTxCoords(variant, pred);
    // Simplest case first: variant starts and ends in a single exon or single intron
    if (txc.startInExon == txc.endInExon && txc.startExonIx == txc.endExonIx)
	{
	int ix = txc.startExonIx;
	if (txc.startInExon)
	    {
	    // Exonic variant; figure out what kind:
	    effectsList = slCat(effectsList,
				gpFxInExon(variant, &txc, ix, pred, predIsNmd, transcriptSeq, lm));
	    }
	else
	    {
	    // Intronic (and/or splice) variant:
	    effectsList = slCat(effectsList,
				gpFxInIntron(variant, ix, pred, predIsNmd, defaultAltAllele, lm));
	    }
	}
    else
	{
	if (!predIsNmd)
	    {
	    // Let the user beware -- this variant is just complex (it overlaps at least one
	    // exon/intron boundary).  It could be an insertion, an MNV (multi-nt var) or
	    // a deletion.
	    struct gpFx *effect = gpFxNew(defaultAltAllele, pred->name, complex_transcript_variant,
					  none, lm);
	    effectsList = slCat(effectsList, effect);
	    }
	// But we can at least say which introns and/or exons are affected.
	// Transform exon and intron numbers into ordered integers, -1 (upstream) through
	// 2*lastExonIx+1 (downstream), with even numbers being exonNum*2 and odd numbers
	// being intronNum*2 + 1:
	int vieStart = (2 * txc.startExonIx) + (txc.startInExon ? 0 : 1);
	int vieEnd = (2 * txc.endExonIx) + (txc.endInExon ? 0 : 1);
	if (vieEnd < vieStart)
	    {
	    // vieEnd == vieStart-1 ==> insertion at exon/intron boundary
            // vieEnd == vieStart-2 ==> insertion at exon-exon boundary (i.e. ref has deletion!)
	    if ((vieEnd != vieStart-1 && vieEnd != vieStart-2) ||
                varStart != varEnd)
		errAbort("gpFxCheckTranscript: expecting insertion in pred=%s "
			 "but varStart=%d, varEnd=%d, vieStart=%d, vieEnd=%d, "
			 "starts in %son, ends in %son",
			 pred->name, varStart, varEnd, vieStart, vieEnd,
			 (txc.startInExon ? "ex" : "intr"), (txc.endInExon ? "ex" : "intr"));
	    // Since it's an insertion, remember that end is before start.
	    if (txc.startInExon)
		{
		// Intronic end precedes exonic start. Watch out for upstream as "intron[-1]":
		if (txc.endExonIx >= 0)
		    effectsList = slCat(effectsList,
					gpFxInIntron(variant, txc.endExonIx, pred, predIsNmd,
						     defaultAltAllele, lm));
		effectsList = slCat(effectsList,
				    gpFxInExon(variant, &txc, txc.startExonIx, pred, predIsNmd,
					       transcriptSeq, lm));
		}
	    else
		{
		// Exonic end precedes intronic start.
		effectsList = slCat(effectsList,
				    gpFxInExon(variant, &txc, txc.endExonIx, pred, predIsNmd,
					       transcriptSeq, lm));
		// Watch out for downstream as "intron[lastExonIx]"
		if (txc.startExonIx < txc.exonCount - 1)
		    effectsList = slCat(effectsList,
					gpFxInIntron(variant, txc.startExonIx, pred,
						     predIsNmd, defaultAltAllele, lm));
		}
	    } // end if variant is insertion
	else
	    {
	    // MNV or deletion - consider each overlapping intron and/or exon
	    int ie;
	    // Watch out for upstream (vieStart < 0) and downstream (vieEnd > last exon).
	    for (ie = max(vieStart, 0);  ie <= min(vieEnd, 2*(pred->exonCount-1));  ie++)
		{
		boolean isExon = (ie%2 == 0);
		int ix = ie / 2;
		if (isExon)
		    effectsList = slCat(effectsList,
					gpFxInExon(variant, &txc, ix, pred, predIsNmd,
						   transcriptSeq, lm));
		else
		    effectsList = slCat(effectsList,
					gpFxInIntron(variant, ix, pred, predIsNmd,
						     defaultAltAllele, lm));
		} // end for each (partial) exon/intron overlapping variant
	    } // end if variant is MNV or deletion
	} // end if variant is complex
    } // end if variant overlaps pred
return effectsList;
}

static struct gpFx *gpFxCheckUpDownstream(struct variant *variant, struct genePred *pred,
					  struct lm *lm)
// check to see if the variant is up or downstream
{
struct gpFx *effectsList = NULL;
char *defaultAltAllele = firstAltAllele(variant->alleles);

for(; variant ; variant = variant->next)
    {
    // Is this variant to the left or right of transcript?
    enum soTerm soNumber = 0;
    if (variant->chromStart < pred->txStart &&
	variant->chromEnd > pred->txStart - GPRANGE)
	{
	if (*pred->strand == '+')
	    soNumber = upstream_gene_variant;
	else
	    soNumber = downstream_gene_variant;
	}
    else if (variant->chromEnd > pred->txEnd &&
	     variant->chromStart < pred->txEnd + GPRANGE)
	{
	if (*pred->strand == '+')
	    soNumber = downstream_gene_variant;
	else
	    soNumber = upstream_gene_variant;
	}
    if (soNumber != 0)
	{
	struct gpFx *effects = gpFxNew(defaultAltAllele, pred->name, soNumber, none, lm);
	effectsList = slCat(effectsList, effects);
	}
    }

return effectsList;
}

static void checkVariantList(struct variant *variant)
// check to see that we either have one variant (possibly with multiple
// alleles) or that if we have a list of variants, they only have
// one allele a piece.
{
if (variant->next == NULL)	 // just one variant
    return;

for(; variant; variant = variant->next)
    if (variant->numAlleles != 1)
	errAbort("gpFxPredEffect needs either 1 variant, or only 1 allele in all variants");
}

struct gpFx *gpFxNoVariation(struct variant *variant, struct lm *lm)
/* Return a gpFx with SO term no_sequence_alteration, for VCF rows that aren't really variants. */
{
char *seq = NULL;
struct allele *allele;
for (allele = variant->alleles;  allele != NULL;  allele = allele->next)
    if (allele->isReference)
        {
        seq = allele->sequence;
        // Don't break out of the loop -- pick the last one we see because the first is likely
        // the "real" reference allele, while the other(s) is something like "<X>" or "<*>".
        }
return gpFxNew(seq, "", no_sequence_alteration, none, lm);
}

struct gpFx *gpFxPredEffect(struct variant *variant, struct genePred *pred,
			    struct dnaSeq *transcriptSequence, struct lm *lm)
// return the predicted effect(s) of a variation list on a genePred
{
struct gpFx *effectsList = NULL;

// make sure we can deal with the variants that are coming in
checkVariantList(variant);

for (; variant != NULL;  variant = variant->next)
    {
    if (! hasAltAllele(variant->alleles))
	effectsList = slCat(effectsList, gpFxNoVariation(variant, lm));
    else
        {
        // check to see if SNP is up or downstream
        effectsList = slCat(effectsList, gpFxCheckUpDownstream(variant, pred, lm));

        // check to see if SNP is in the transcript
        effectsList = slCat(effectsList,
                            gpFxCheckTranscript(variant, pred, transcriptSequence, lm));
        }
    }

return effectsList;
}
