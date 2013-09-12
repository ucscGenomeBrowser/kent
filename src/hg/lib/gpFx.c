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
assert( !(strand == '+' && ii >= pred->exonCount) );
assert( !(strand == '-' && ii < 0) );
// clip off part of UTR in exon that has CDS in it too
int exonOffset = pred->cdsStart - pred->exonStarts[ii];
if (strand == '-')
    exonOffset = pred->exonEnds[ii] - pred->cdsEnd;
offset += exonOffset;
return offset;
}

char *gpFxModifySequence(struct allele *allele, struct genePred *pred, 
			 int exonNum, int exonQStart, struct dnaSeq *transcriptSequence,
			 struct genePred **retNewPred,
			 int *retTranscriptOffset, int *retBasesAdded,
			 int *retCdsOffset, int *retCdsBasesAdded, struct lm *lm)
/* modify a transcript to what it'd be if the alternate allele were present;
 * if transcript length changes, make retNewPred a shallow copy of pred w/tweaked coords. */
{
// clip allele to exon
struct allele *clipAllele = alleleClip(allele, pred->exonStarts[exonNum], pred->exonEnds[exonNum],
				       lm);
struct variant *clipVariant = clipAllele->variant;
boolean predIsRc = (*pred->strand == '-');
// change transcript at variant point
int exonOffset = clipVariant->chromStart - pred->exonStarts[exonNum];
if (predIsRc)
    exonOffset = clipVariant->chromEnd - pred->exonStarts[exonNum];
int transcriptOffset = exonQStart + exonOffset;
int variantWidth = clipVariant->chromEnd - clipVariant->chromStart;
int basesAdded = clipAllele->length - variantWidth;
char *newAlleleSeq = lmCloneString(lm, clipAllele->sequence);
if (predIsRc)
    {
    transcriptOffset = transcriptSequence->size - transcriptOffset;
    reverseComplement(newAlleleSeq, strlen(newAlleleSeq));
    }

// If clipAllele overlaps CDS, figure out cdsOffset and cdsBasesAdded:
int cdsOffset = -1, cdsBasesAdded = 0;
if (clipVariant->chromStart < pred->cdsEnd && clipVariant->chromEnd > pred->cdsStart)
    {
    struct allele *clipAlleleCds = alleleClip(clipAllele, pred->cdsStart, pred->cdsEnd, lm);
    struct variant *clipVariantCds = clipAlleleCds->variant;
    int exonOffsetCds = clipVariantCds->chromStart - pred->exonStarts[exonNum];
    if (predIsRc)
	exonOffsetCds = clipVariantCds->chromEnd - pred->exonStarts[exonNum];
    int cdsOffsetInTx = exonQStart + exonOffsetCds;
    if (predIsRc)
	cdsOffsetInTx = transcriptSequence->size - cdsOffsetInTx;
    int cdsQStart = getCodingOffsetInTx(pred, pred->strand[0]);
    cdsOffset = cdsOffsetInTx - cdsQStart;
    int variantWidthCds = clipVariantCds->chromEnd - clipVariantCds->chromStart;
    cdsBasesAdded = clipAlleleCds->length - variantWidthCds;
    }

if (basesAdded != 0 && retNewPred != NULL)
    {
    // OK, now we have to make a new genePred that matches the changed sequence
    struct genePred *newPred = lmCloneVar(lm, pred);
    size_t size = pred->exonCount * sizeof(pred->exonStarts[0]);
    newPred->exonStarts = lmCloneMem(lm, pred->exonStarts, size);
    newPred->exonEnds = lmCloneMem(lm, pred->exonEnds, size);
    if (newPred->cdsStart > clipVariant->chromEnd)
	newPred->cdsStart += basesAdded;
    else if (newPred->cdsStart > clipVariant->chromStart && cdsBasesAdded < 0)
	newPred->cdsStart += cdsBasesAdded;
    if (newPred->cdsEnd > clipVariant->chromEnd)
	newPred->cdsEnd += basesAdded;
    else if (newPred->cdsEnd > clipVariant->chromStart && cdsBasesAdded < 0)
	newPred->cdsEnd += cdsBasesAdded;
    newPred->exonEnds[exonNum] += basesAdded;
    int ii;
    for (ii = exonNum+1;  ii < pred->exonCount;  ii++)
	{
	newPred->exonStarts[ii] += basesAdded;
	newPred->exonEnds[ii] += basesAdded;
	}
    newPred->txEnd = newPred->exonEnds[newPred->exonCount - 1];
    *retNewPred = newPred;
    }


// make the change in the sequence
char *retSequence = lmCloneString(lm, transcriptSequence->dna);
retSequence = mergeAllele( retSequence, transcriptOffset, variantWidth, newAlleleSeq,
			   clipAllele->length, lm);
if (retTranscriptOffset != NULL)
    *retTranscriptOffset = transcriptOffset;
if (retBasesAdded != NULL)
    *retBasesAdded = basesAdded;
if (retCdsOffset != NULL)
    *retCdsOffset = cdsOffset;
if (retCdsBasesAdded != NULL)
    *retCdsBasesAdded = cdsBasesAdded;
return retSequence;
}

static char *getCodingSequence(struct genePred *pred, char *transcriptSequence, struct lm *lm)
/* extract the CDS from a transcript */
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

static struct dnaSeq *lmNewSimpleSeq(struct lm *lm, char *seq)
/* Like newDnaSeq but no name and using localmem. */
{
struct dnaSeq *simpleSeq;
lmAllocVar(lm, simpleSeq);
simpleSeq->dna = seq;
simpleSeq->size = strlen(seq);
return simpleSeq;
}

static char *lmSimpleTranslate(struct lm *lm, struct dnaSeq *dnaSeq)
/* Just translate dnaSeq into pep, use 'Z' and truncate for stop codon, and use localmem. */
{
int aaLen = (dnaSeq->size / 3) + 1;
char *pep = lmAlloc(lm, aaLen + 1);
int i;
for (i = 0;  i < dnaSeq->size;  i += 3)
    {
    char aa = lookupCodon(dnaSeq->dna + i);
    pep[i/3] = aa;
    if (aa == '\0')
	{
	pep[i/3] = 'Z';
	break;
	}
    }
if (i < dnaSeq->size && pep[i/3] != 'Z')
    // incomplete final codon
    pep[i/3] = 'X';
return pep;
}

static void getSequences(struct genePred *pred, char *transcriptSequence,
			 char **retCodingSequence,  char **retAaSeq, struct lm *lm)
/* get coding sequences for a transcript and a variant transcript */
{
char *codingSequence = getCodingSequence(pred, transcriptSequence, lm);
struct dnaSeq *codingDna = lmNewSimpleSeq(lm, codingSequence);
char *aaSeq = lmSimpleTranslate(lm, codingDna);
char *stop = strchr(aaSeq, 'Z');
if (stop != NULL)
    {
    // If aaSeq was truncated by a stop codon, truncate codingSequence as well:
    int codingLength = 3*strlen(aaSeq);
    codingSequence[codingLength] = '\0';
    }
*retCodingSequence = codingSequence;
*retAaSeq = aaSeq;
}

static void setNCExonVals(struct gpFx *gpFx, int exonNum, int cDnaPosition)
/* This gpFx is for a variant in exon of non-coding gene or UTR exon of coding gene;
 * set details.nonCodingExon values. */
{
gpFx->details.nonCodingExon.exonNumber = exonNum;
gpFx->details.nonCodingExon.cDnaPosition = cDnaPosition;
}

struct gpFx *gpFxCheckUtr( struct allele *allele, struct genePred *pred,
			   int exonNum, int cDnaPosition, struct lm *lm)
/* check for effects in UTR of coding gene -- caller ensures it's in exon, pred is coding
 * and exonNum has been strand-adjusted */
{
struct gpFx *gpFx = NULL;
enum soTerm term = 0;
struct variant *variant = allele->variant;
if (variant->chromStart < pred->cdsStart && variant->chromEnd > pred->txStart)
    // we're in left UTR
    term = (*pred->strand == '-') ? _3_prime_UTR_variant : _5_prime_UTR_variant;
else if (variant->chromStart < pred->txEnd && variant->chromEnd > pred->cdsEnd)
    // we're in right UTR
    term = (*pred->strand == '-') ? _5_prime_UTR_variant : _3_prime_UTR_variant;
if (term != 0)
    {
    gpFx = gpFxNew(allele->sequence, pred->name, term, nonCodingExon, lm);
    setNCExonVals(gpFx, exonNum, cDnaPosition);
    }
return gpFx;
}

struct gpFx *gpFxChangedNoncodingExon(struct allele *allele, struct genePred *pred,
				      int exonNum, int cDnaPosition, struct lm *lm)
/* generate an effect for a variant in a non-coding transcript */
{
struct gpFx *gpFx = gpFxNew(allele->sequence, pred->name, non_coding_exon_variant, nonCodingExon,
			    lm);
if (*pred->strand == '-')
    exonNum = pred->exonCount - exonNum - 1;
setNCExonVals(gpFx, exonNum, cDnaPosition);
return gpFx;
}

boolean isSafeFromNMD(int exonNum, struct variant *variant, struct genePred *pred)
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

void setSpecificCodingSoTerm(struct gpFx *effect, char *oldAa, char *newAa, int cdsBasesAdded,
			     boolean safeFromNMD)
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

struct gpFx *gpFxChangedCodingExon(struct allele *allele, struct genePred *pred, int exonNum,
				   struct dnaSeq *transcriptSequence, char *newSequence,
				   struct genePred *newPred, int cDnaPosition, int basesAdded,
				   int cdsPosition, int cdsBasesAdded, struct lm *lm)
/* calculate effect of allele change on coding transcript */
{
struct gpFx *effectsList = NULL;
if (*pred->strand == '-')
    exonNum = pred->exonCount - exonNum - 1;

// first find effects of allele in UTR, if any
effectsList = gpFxCheckUtr(allele, pred, exonNum, cDnaPosition, lm);

// calculate original and variant coding DNA and AA's
char *oldCodingSequence, *newCodingSequence, *oldaa, *newaa;
getSequences(pred, transcriptSequence->dna, &oldCodingSequence, &oldaa, lm);
getSequences(newPred, newSequence, &newCodingSequence, &newaa, lm);

// if coding sequence hasn't changed (just UTR), we're done
if (sameString(oldCodingSequence, newCodingSequence))
    return effectsList;

// allocate the effect structure - fill in soNumber and details below
struct gpFx *effect = gpFxNew(allele->sequence, pred->name, coding_sequence_variant, codingChange,
			      lm);
struct codingChange *cc = &effect->details.codingChange;
cc->cDnaPosition = cDnaPosition;
cc->cdsPosition = cdsPosition;
cc->exonNumber = exonNum;
int pepPos = cdsPosition / 3;
cc->pepPosition = pepPos;
if (cdsBasesAdded % 3 == 0)
    {
    // Common case: substitution, same number of old/new codons/peps:
    int numOldCodons = (1 + allele->length / 3), numNewCodons = (1 + allele->length / 3);
    if (cdsBasesAdded > 0)
	{
	// insertion: more new codons than old
	numOldCodons = (cdsPosition % 3) == 0 ? 0 : 1;
	numNewCodons = numOldCodons + (cdsBasesAdded / 3);
	}
    else if (cdsBasesAdded < 0)
	{
	// deletion: more old codons than new
	numNewCodons = (cdsPosition % 3) == 0 ? 0 : 1;
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

// show specific coding changes by making before-and-after subsequences:

boolean safeFromNMD = isSafeFromNMD(exonNum, allele->variant, pred);
setSpecificCodingSoTerm(effect, oldaa, newaa, cdsBasesAdded, safeFromNMD);

slAddHead(&effectsList, effect);
return effectsList;
}


struct gpFx *gpFxInExon(struct allele *allele, struct genePred *pred, char *refAllele, int exonNum,
			int exonQStart, struct dnaSeq *transcriptSequence, struct lm *lm)
/* generate an effect from a different allele in an exon */
{
struct genePred *newPred = pred;
int cDnaPosition = 0, basesAdded = 0, cdsPosition = 0, cdsBasesAdded = 0;
char *newSequence = gpFxModifySequence(allele, pred, exonNum, exonQStart,
				       transcriptSequence, &newPred, &cDnaPosition, &basesAdded,
				       &cdsPosition, &cdsBasesAdded, lm);

if (sameString(newSequence, transcriptSequence->dna))
    {
    struct variant *variant = allele->variant;
    errAbort("gpFxInExon: %s:%d-%d, allele='%s', refAllele='%s', no change, shouldn't be here",
	     variant->chrom, variant->chromStart+1, variant->chromEnd, allele->sequence,
	     refAllele);
    return NULL;  // no change in transcript -- but allele should always be non-reference
    }

struct gpFx *effectsList;
if (pred->cdsStart != pred->cdsEnd)
    effectsList = gpFxChangedCodingExon(allele, pred, exonNum, transcriptSequence,
					newSequence, newPred, cDnaPosition, basesAdded,
					cdsPosition, cdsBasesAdded, lm);
else
    {
    effectsList = gpFxChangedNoncodingExon(allele, pred, exonNum, cDnaPosition, lm);
    }

return effectsList;
}

static boolean hasAltAllele(struct allele *alleles, char *refAllele)
/* Make sure there's something to work on here... */
{
while (alleles != NULL && sameString(alleles->sequence, refAllele))
    alleles = alleles->next;
return (alleles != NULL);
}

static char *firstAltAllele(struct allele *alleles, char *refAllele)
/* Ensembl always reports an alternate allele, even if that allele is not being used
 * to calculate any consequence.  When allele doesn't really matter, just use the
 * first alternate allele that is given. */
{
while (alleles != NULL && sameString(alleles->sequence, refAllele))
    alleles = alleles->next;
if (alleles == NULL)
    errAbort("firstAltAllele: no alt allele in list");
return alleles->sequence;
}

static struct gpFx *gpFxCheckExons(struct variant *variantList, struct genePred *pred,
				   char *refAllele, struct dnaSeq *transcriptSequence,
				   struct lm *lm)
// check to see if the variant is in an exon
{
struct gpFx *effectsList = NULL;
int exonQStart = 0;
int ii;
for (ii=0; ii < pred->exonCount; ii++)
    {
    struct variant *variant = variantList;
    for(; variant ; variant = variant->next)
	{
	// check if in an exon
	int end = variant->chromEnd;
	if (variant->chromStart == end)
	    end++;
	int size;
	if ((size = positiveRangeIntersection(pred->exonStarts[ii], pred->exonEnds[ii], 
					      variant->chromStart, end)) > 0)
	    {
	    if (size != end - variant->chromStart)
		{
		// variant is not completely in exon
		char *altAllele = firstAltAllele(variant->alleles, refAllele);
		struct gpFx *effect = gpFxNew(altAllele, pred->name, complex_transcript_variant,
					      none, lm);
		effectsList = slCat(effectsList, effect);
		}
	    struct allele *allele = variant->alleles;
	    for(; allele ; allele = allele->next)
		{
		if (differentString(allele->sequence, refAllele))
		    effectsList = slCat(effectsList,
					gpFxInExon(allele, pred, refAllele, ii, exonQStart,
						   transcriptSequence, lm));
		}
	    }
	}
    exonQStart += (pred->exonEnds[ii] - pred->exonStarts[ii]);
    }

return effectsList;
}

static struct gpFx *gpFxCheckIntrons(struct variant *variant, struct genePred *pred,
				     char *altAllele, struct lm *lm)
// check to see if a single variant overlaps an intron (and possibly splice region)
//#*** TODO: watch out for "introns" that are actually indels between tx seq and ref genome!
{
struct gpFx *effectsList = NULL;
boolean minusStrand = (pred->strand[0] == '-');
int ii;
for (ii=0; ii < pred->exonCount - 1; ii++)
    {
    int intronStart = pred->exonEnds[ii];
    int intronEnd = pred->exonStarts[ii+1];
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
	effects->details.intron.intronNumber = minusStrand ? (pred->exonCount - ii - 2) : ii;
	slAddHead(&effectsList, effects);
	}
    else if ((variant->chromEnd > intronStart-3 && variant->chromStart < intronStart) ||
	     (variant->chromEnd > intronEnd && variant->chromStart < intronEnd+3))
	{
	// if variant is in exon *but* within 3 bases of splice site,
	// it also qualifies as splice_region_variant:
	struct gpFx *effects = gpFxNew(altAllele, pred->name,
				       splice_region_variant, intron, lm);
	effects->details.intron.intronNumber = minusStrand ? (pred->exonCount - ii - 1) : ii;
	slAddHead(&effectsList, effects);
	}
    }
return effectsList;
}


static struct gpFx *gpFxCheckBackground(struct variant *variant, struct genePred *pred,
					char *refAllele, struct lm *lm)
// check to see if the variant is up or downstream or in intron of the gene 
{
struct gpFx *effectsList = NULL;
char *altAllele = firstAltAllele(variant->alleles, refAllele);

for(; variant ; variant = variant->next)
    {
    // is this variant in an intron
    effectsList = slCat(effectsList, gpFxCheckIntrons(variant, pred, altAllele, lm));

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
	struct gpFx *effects = gpFxNew(altAllele, pred->name, soNumber, none, lm);
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

struct gpFx *gpFxPredEffect(struct variant *variant, struct genePred *pred, char *refAllele,
			    struct dnaSeq *transcriptSequence, struct lm *lm)
// return the predicted effect(s) of a variation list on a genePred
{
struct gpFx *effectsList = NULL;

// make sure we can deal with the variants that are coming in
checkVariantList(variant);

// If only the reference allele has been observed, skip it:
if (! hasAltAllele(variant->alleles, refAllele))
    return NULL;

// check to see if SNP is up or downstream or in intron
effectsList = slCat(effectsList, gpFxCheckBackground(variant, pred, refAllele, lm));

// check to see if SNP is in the transcript
effectsList = slCat(effectsList, gpFxCheckExons(variant, pred, refAllele, transcriptSequence, lm));

//#*** sort by transcript name?  transcript start?

return effectsList;
}
