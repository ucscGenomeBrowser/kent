/* gpFx --- routines to calculate the effect of variation on a genePred */

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
			txc.startInCds = cdsOffset;
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

struct gpFx *gpFxNew(char *allele, char *transcript, enum soTerm soNumber,
		     enum detailType detailType, struct lm *lm)
/* Fill in the common members of gpFx; leave soTerm-specific members for caller to fill in. */
{
struct gpFx *effect;
lmAllocVar(lm, effect);
effect->allele = collapseDashes(strUpper(lmCloneString(lm, allele)));
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

static void setNCExonVals(struct gpFx *gpFx, int exonIx, int cdnaPos)
/* This gpFx is for a variant in exon of non-coding gene or UTR exon of coding gene;
 * set details.nonCodingExon values. */
{
gpFx->details.nonCodingExon.exonNumber = exonIx;
gpFx->details.nonCodingExon.cDnaPosition = cdnaPos;
}

static struct gpFx *gpFxCheckUtr( struct allele *allele, struct genePred *pred,
				  struct txCoords *txc, int exonIx, struct lm *lm)
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
    gpFx = gpFxNew(allele->sequence, pred->name, term, nonCodingExon, lm);
    setNCExonVals(gpFx, exonIx, txc->startInCdna);
    }
return gpFx;
}

static struct gpFx *gpFxChangedNoncodingExon(struct allele *allele, struct genePred *pred,
					     struct txCoords *txc, int exonIx, struct lm *lm)
/* generate an effect for a variant in a non-coding transcript */
{
struct gpFx *gpFx = gpFxNew(allele->sequence, pred->name, non_coding_exon_variant, nonCodingExon,
			    lm);
setNCExonVals(gpFx, exonIx, txc->startInCdna);
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

static char *getCodingSequence(struct genePred *pred, char *transcriptSequence, struct lm *lm)
/* Extract the CDS from a transcript. Temporarily force transcriptSequence to + strand
 * so we can work in + strand coords, then restore transcriptSequence and put result
 * on correct strand. */
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

static char *gpFxModifyCodingSequence(char *oldCodingSeq, struct genePred *pred,
				      struct txCoords *txc, struct allele *allele,
				      int *retCdsBasesAdded, struct lm *lm)
/* Return a new coding sequence that is oldCodingSeq with allele applied. */
{
boolean isRc = (pred->strand[0] == '-');
char *newAlleleSeq = allele->sequence;
int newAlLen = strlen(newAlleleSeq);
if (isRc)
    {
    newAlleleSeq = lmCloneString(lm, allele->sequence);
    reverseComplement(newAlleleSeq, newAlLen);
    }
int variantSizeOnCds = txc->endInCds - txc->startInCds;
if (variantSizeOnCds < 0)
    errAbort("gpFx: endInCds (%d) < startInCds (%d)", txc->endInCds, txc->startInCds);
char *newCodingSeq = mergeAllele(oldCodingSeq, txc->startInCds, variantSizeOnCds,
				 newAlleleSeq, allele->length, lm);
// If newCodingSequence has an early stop, truncate there:
truncateAtStopCodon(newCodingSeq);
int variantSizeOnRef = allele->variant->chromEnd - allele->variant->chromStart;
if (retCdsBasesAdded)
    *retCdsBasesAdded = allele->length - variantSizeOnRef;
return newCodingSeq;
}

static boolean isSafeFromNMD(int exonNum, struct variant *variant, struct genePred *pred)
/* Return TRUE if variant in strand-corrected exonNum is in the region
 * of pred that would make it safe from Nonsense-Mediated Decay (NMD).
 * NMD is triggered by the presence of a stop codon that appears
 * before ~50bp before the end of the last exon.  In other words, if
 * there's a stop codon with a detectable downstream splice junction,
 * translation is prevented -- see
 * http://en.wikipedia.org/wiki/MRNA_surveillance#Mechanism_in_mammals */
{
boolean lastExonNum = pred->exonCount - 1;
if (exonNum == lastExonNum)
    return TRUE;
int nextToLastExonNum = pred->exonCount - 2;
if (exonNum == nextToLastExonNum)
    {
    // Is it within 50bp of 3' end of exon?
    if (pred->strand[0] == '-')
	return (variant->chromEnd < pred->exonStarts[1] + 50);
    else
	return (variant->chromStart > pred->exonEnds[nextToLastExonNum] - 50);
    }
return FALSE;
}

static void setSpecificCodingSoTerm(struct gpFx *effect, char *oldAa, char *newAa,
				    int cdsBasesAdded, boolean safeFromNMD)
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
	    // possibly following a frameshift caused by an insertion.
	    if (cc->aaNew[0] != 'Z')
		{
		if (newAa[newAaSize-1] != 'Z')
		    errAbort("gpFx: new protein is smaller but last base in new sequence "
			     "is '%c' not 'Z'", newAa[newAaSize-1]);
		effect->soNumber = frameshift_variant;
		}
	    else if (safeFromNMD)
		effect->soNumber = stop_gained;
	    else
		effect->soNumber = NMD_transcript_variant;
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
				   struct txCoords *txc, int exonIx,
				   struct dnaSeq *transcriptSequence, struct lm *lm)
/* calculate effect of allele change on coding transcript */
{
// calculate original and variant coding DNA and AA's
char *oldCodingSequence = getCodingSequence(pred, transcriptSequence->dna, lm);
int oldCdsLen = strlen(oldCodingSequence);
char *oldaa = lmSimpleTranslate(lm, oldCodingSequence, oldCdsLen);
int cdsBasesAdded = 0;
char *newCodingSequence = gpFxModifyCodingSequence(oldCodingSequence, pred, txc, allele,
						   &cdsBasesAdded, lm);
int newCdsLen = strlen(newCodingSequence);
char *newaa = lmSimpleTranslate(lm, newCodingSequence, newCdsLen);


// allocate the effect structure - fill in soNumber and details below
struct gpFx *effect = gpFxNew(allele->sequence, pred->name, coding_sequence_variant, codingChange,
			      lm);
struct codingChange *cc = &effect->details.codingChange;
cc->cDnaPosition = txc->startInCdna;
cc->cdsPosition = txc->startInCds;
cc->exonNumber = exonIx;
int pepPos = txc->startInCds / 3;
// At this point we don't use genePredExt's exonFrames field -- we just assume that
// the CDS starts in frame.  That's not always the case (e.g. ensGene has some CDSs
// that begin out of frame), so watch out for early truncation of oldCodingSequence
// due to stop codon in the wrong frame:
if (pepPos >= strlen(oldaa))
    return NULL;
cc->pepPosition = pepPos;
if (cdsBasesAdded % 3 == 0)
    {
    // Common case: substitution, same number of old/new codons/peps:
    int numOldCodons = (1 + allele->length / 3), numNewCodons = (1 + allele->length / 3);
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

boolean safeFromNMD = isSafeFromNMD(exonIx, allele->variant, pred);
setSpecificCodingSoTerm(effect, oldaa, newaa, cdsBasesAdded, safeFromNMD);

return effect;
}


static boolean hasAltAllele(struct allele *alleles)
/* Make sure there's something to work on here... */
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
			       struct genePred *pred, struct dnaSeq *transcriptSeq, struct lm *lm)
/* Given a variant that overlaps an exon of pred, figure out what each allele does. */
{
struct gpFx *effectsList = NULL;
struct allele *allele = variant->alleles;
for ( ; allele ; allele = allele->next)
    {
    if (!allele->isReference)
	{
	if (pred->cdsStart != pred->cdsEnd)
	    {
	    // first find effects of allele in UTR, if any
	    effectsList = slCat(effectsList,
				gpFxCheckUtr(allele, pred, txc, exonIx, lm));
	    if (txc->startInCds >= 0)
		effectsList = slCat(effectsList,
				    gpFxChangedCds(allele, pred, txc, exonIx, transcriptSeq, lm));
	    }
	else
	    effectsList = slCat(effectsList,
				gpFxChangedNoncodingExon(allele, pred, txc, exonIx, lm));

	// Was entire exon deleted?
	int exonNumPos = exonIx;
	if (pred->strand[0] == '-')
	    exonNumPos = pred->exonCount - 1 - exonIx;
	uint exonStart = pred->exonStarts[exonNumPos], exonEnd = pred->exonEnds[exonNumPos];
	if (variant->chromStart <= exonStart && variant->chromEnd >= exonEnd)
	    {
	    struct gpFx *effect = gpFxNew(allele->sequence, pred->name, exon_loss,
					  nonCodingExon, lm);
	    setNCExonVals(effect, exonIx, txc->startInCdna);
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
		struct gpFx *effect = gpFxNew(allele->sequence, pred->name, splice_region_variant,
					      nonCodingExon, lm);
		setNCExonVals(effect, exonIx, txc->startInCdna);
		slAddTail(&effectsList, effect);
		}
	    }
	}
    }
return effectsList;
}

static struct gpFx *gpFxInIntron(struct variant *variant, struct txCoords *txc, int intronIx,
				 struct genePred *pred, char *altAllele, struct lm *lm)
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
    else if ((variant->chromEnd > intronStart+3 && variant->chromStart < intronStart+8) ||
	     (variant->chromEnd > intronEnd-8 && variant->chromStart < intronEnd+3))
	// Within 3 to 8 bases of intron start or end:
	soNumber = splice_region_variant;
    struct gpFx *effects = gpFxNew(altAllele, pred->name, soNumber, intron, lm);
    effects->details.intron.intronNumber = intronIx;
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
				gpFxInExon(variant, &txc, ix, pred, transcriptSeq, lm));
	    }
	else
	    {
	    // Intronic (and/or splice) variant:
	    effectsList = slCat(effectsList,
				gpFxInIntron(variant, &txc, ix, pred, defaultAltAllele, lm));
	    }
	}
    else
	{
	// Let the user beware -- this variant is just complex (it overlaps at least one
	// exon/intron boundary).  It could be an insertion, an MNV (multi-nt var) or
	// a deletion.
	struct gpFx *effect = gpFxNew(defaultAltAllele, pred->name, complex_transcript_variant,
				      none, lm);
	effectsList = slCat(effectsList, effect);
	// But we can at least say which introns and/or exons are affected.
	// Transform exon and intron numbers into ordered integers, -1 (upstream) through
	// 2*lastExonIx+1 (downstream), with even numbers being exonNum*2 and odd numbers
	// being intronNum*2 + 1:
	int vieStart = (2 * txc.startExonIx) + (txc.startInExon ? 0 : 1);
	int vieEnd = (2 * txc.endExonIx) + (txc.endInExon ? 0 : 1);
	if (vieEnd < vieStart)
	    {
	    // Insertion at exon boundary (or bug)
	    if (vieEnd != vieStart-1 || varStart != varEnd || txc.startInExon == txc.endInExon)
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
					gpFxInIntron(variant, &txc, txc.endExonIx, pred,
						     defaultAltAllele, lm));
		effectsList = slCat(effectsList,
				    gpFxInExon(variant, &txc, txc.startExonIx, pred,
					       transcriptSeq, lm));
		}
	    else
		{
		// Exonic end precedes intronic start.
		effectsList = slCat(effectsList,
				    gpFxInExon(variant, &txc, txc.endExonIx, pred,
					       transcriptSeq, lm));
		// Watch out for downstream as "intron[lastExonIx]"
		if (txc.startExonIx < txc.exonCount - 1)
		    effectsList = slCat(effectsList,
					gpFxInIntron(variant, &txc, txc.startExonIx, pred,
						     defaultAltAllele, lm));
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
					gpFxInExon(variant, &txc, ix, pred, transcriptSeq, lm));
		else
		    effectsList = slCat(effectsList,
					gpFxInIntron(variant, &txc, ix, pred,
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

struct gpFx *gpFxPredEffect(struct variant *variant, struct genePred *pred,
			    struct dnaSeq *transcriptSequence, struct lm *lm)
// return the predicted effect(s) of a variation list on a genePred
{
struct gpFx *effectsList = NULL;

// make sure we can deal with the variants that are coming in
checkVariantList(variant);

for (; variant != NULL;  variant = variant->next)
    {
    // If only the reference allele has been observed, skip it:
    //#*** Some might like to keep variants e.g. in VCF output... 
    //#*** aha, Ensembl has requested a term for 'no change' from SONG.
    //#*** Add that to soTerm when it exists...
    if (! hasAltAllele(variant->alleles))
	return NULL;

    // check to see if SNP is up or downstream
    effectsList = slCat(effectsList, gpFxCheckUpDownstream(variant, pred, lm));

    // check to see if SNP is in the transcript
    effectsList = slCat(effectsList, gpFxCheckTranscript(variant, pred, transcriptSequence, lm));
    }

return effectsList;
}
