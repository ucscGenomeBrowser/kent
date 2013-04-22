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

static unsigned countDashes(char *string)
/* count the number of dashes in a string */
{
int count = 0;

while(*string)
    {
    if (*string == '-')
	count++;
    string++;
    }

return count;
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
else  if (variantWidth > alleleLength)
    errAbort("gpFx: allele not padded");
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

    // copy in the new allele
    memcpy(&newTranscript[offset + alleleLength], restOfTranscript, 
	strlen(restOfTranscript) + 1);
    }

return newTranscript;
}

char *gpFxModifySequence(struct allele *allele, struct genePred *pred, 
			 int exonNum, int exonQStart, struct dnaSeq *transcriptSequence,
			 struct genePred **retNewPred, struct lm *lm)
/* modify a transcript to what it'd be if the alternate allele were present;
 * if transcript length changes, make retNewPred a shallow copy of pred w/tweaked coords. */
{
// clip allele to exon
struct allele *clipAllele = alleleClip(allele, pred->exonStarts[exonNum], pred->exonEnds[exonNum],
				       lm);

// change transcript at variant point
int exonOffset = clipAllele->variant->chromStart - pred->exonStarts[exonNum];
int transcriptOffset = exonQStart + exonOffset;

int variantWidth = clipAllele->variant->chromEnd - clipAllele->variant->chromStart;

if (variantWidth != clipAllele->length && retNewPred != NULL)
    {
    // OK, now we have to make a new genePred that matches the changed sequence
    int basesAdded = clipAllele->length - variantWidth;
    struct genePred *newPred = lmCloneVar(lm, pred);
    int alEnd = clipAllele->variant->chromEnd;
    if (newPred->cdsStart > alEnd)
	newPred->cdsStart += basesAdded;
    if (newPred->cdsEnd > alEnd)
	newPred->cdsEnd += basesAdded;
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

char *retSequence = lmCloneString(lm, transcriptSequence->dna);
char *newAlleleSeq = lmCloneString(lm, clipAllele->sequence);
if (*pred->strand == '-')
    {
    transcriptOffset = transcriptSequence->size - (transcriptOffset + strlen(newAlleleSeq));
    reverseComplement(newAlleleSeq, strlen(newAlleleSeq));
    }

// make the change in the sequence
retSequence = mergeAllele( retSequence, transcriptOffset, variantWidth, newAlleleSeq,
			   clipAllele->length, lm);

return retSequence;
}

static int getCodingOffsetInTx(struct genePred *pred, char strand)
/* Skip past UTR (portions of) exons to get offset of CDS relative to transcript start.
 * The strand arg is used instead of pred->strand. */
{
int offset = 0;
int iStart = 0, iIncr = 1;
if (strand == '-')
    {
    // Work our way left from the last exon.
    iStart = pred->exonCount - 1;
    iIncr = -1;
    }
int ii;
// trim off the 5' UTR (or 3' UTR if strand is '-')
for (ii = iStart; ii >= 0 && ii < pred->exonCount; ii += iIncr)
    {
    if ((pred->cdsStart >= pred->exonStarts[ii]) &&
	(pred->cdsStart < pred->exonEnds[ii]))
	break;
    int exonSize = pred->exonEnds[ii] - pred->exonStarts[ii];
    offset += exonSize;
    }
assert( !(strand == '+' && ii >= pred->exonCount) );
assert( !(strand == '-' && ii < 0) );
// clip off part of UTR in exon that has CDS in it too
int exonOffset = pred->cdsStart - pred->exonStarts[ii];
offset += exonOffset;
return offset;
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

static int firstChange(char *string1, char *string2, int *numDifferent)
/* return the position of the first difference between the two sequences */
/* if numDifferent is non-NULL, return number of characters between first 
 * difference, and the last difference */
{
int count = 0;

for(; *string1 == *string2 && (*string1); count++, string1++, string2++)
    ;

int firstChange = count;
int lastChange = firstChange;

if (numDifferent != NULL)
    {
    for (; (*string1) && (*string2); string1++, string2++, count++)
	{
	if (*string1 != *string2)
	    lastChange = count;
	}
    *numDifferent = lastChange - firstChange + 1;
    }

return firstChange;
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
	return (variant->chromEnd < pred->exonStarts[1]);
    else
	return (variant->chromStart > pred->exonEnds[nextToLastExonNum] - 50);
    }
return FALSE;
}

void setSpecificCodingSoTerm(struct gpFx *effect, char *oldAa, char *newAa, int numDashes,
			     int cdsChangeLength, boolean safeFromNMD)
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
    if (numDashes != 0)
	{
	// Got a deletion variant -- check frame:
	//#*** if numDashes == cdsChangeLength, then we don't need numDashes...
	if ((numDashes % 3) == 0)
	    {
	    if ((cdsChangeLength % 3) != 0)
		errAbort("gpFx: assumption about cdsChangeLength vs. numDashes not holding; "
			 "cdsChangeLength=%d, numDashes=%d", cdsChangeLength, numDashes);
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
	    else if ((cdsChangeLength % 3) == 0)
		effect->soNumber = inframe_insertion;
	    else
		effect->soNumber = frameshift_variant;
	    }
	else
	    {
	    // Single aa change
	    if (cc->pepPosition == 0 && cc->aaOld[0] == 'M')
		effect->soNumber = initiator_codon_variant;
	    else if (cc->pepPosition == oldAaSize-1 && oldAa[oldAaSize-1] != 'Z')
		effect->soNumber = incomplete_terminal_codon_variant;
	    else
		effect->soNumber = missense_variant;
	    }
	}
    }
}

struct gpFx *gpFxChangedCodingExon(struct allele *allele, struct genePred *pred, int exonNum,
				   char *transcriptSequence, char *newSequence,
				   struct genePred *newPred, struct lm *lm)
/* calculate effect of allele change on coding transcript */
{
struct gpFx *effectsList = NULL;
if (*pred->strand == '-')
    exonNum = pred->exonCount - exonNum - 1;

// first find effects of allele in UTR, if any
int dnaChangeLength;
int cDnaPosition = firstChange(newSequence, transcriptSequence, &dnaChangeLength);
effectsList = gpFxCheckUtr(allele, pred, exonNum, cDnaPosition, lm);

// calculate original and variant coding DNA and AA's
char *oldCodingSequence, *newCodingSequence, *oldaa, *newaa;
getSequences(pred, transcriptSequence, &oldCodingSequence, &oldaa, lm);
getSequences(newPred, newSequence, &newCodingSequence, &newaa, lm);

// if coding sequence hasn't changed, we're done (allele == reference allele)
if (sameString(oldCodingSequence, newCodingSequence))
    return effectsList;

// allocate the effect structure - fill in soNumber and details below
struct gpFx *effect = gpFxNew(allele->sequence, pred->name, coding_sequence_variant, codingChange,
			      lm);
struct codingChange *cc = &effect->details.codingChange;
cc->cDnaPosition = cDnaPosition;
int cdsChangeLength;
cc->cdsPosition = firstChange( newCodingSequence, oldCodingSequence, &cdsChangeLength);
cc->exonNumber = exonNum;

// show specific coding changes by making before-and-after subsequences:
int codonPosStart = (cc->cdsPosition / 3) ;
int codonPosEnd = ((cdsChangeLength - 1 + cc->cdsPosition) / 3) ;
int numCodons = (codonPosEnd - codonPosStart + 1);
cc->codonOld = lmCloneStringZ(lm, oldCodingSequence + codonPosStart*3, numCodons*3);
cc->codonNew = lmCloneStringZ(lm, newCodingSequence + codonPosStart*3, numCodons*3);

cc->pepPosition = codonPosStart;
cc->aaOld = lmCloneStringZ(lm, oldaa + cc->pepPosition, numCodons);
cc->aaNew = lmCloneStringZ(lm, newaa + cc->pepPosition, numCodons);

boolean safeFromNMD = isSafeFromNMD(exonNum, allele->variant, pred);
setSpecificCodingSoTerm(effect, oldaa, newaa, countDashes(newCodingSequence), cdsChangeLength,
			safeFromNMD);

slAddHead(&effectsList, effect);
return effectsList;
}


struct gpFx *gpFxInExon(struct allele *allele, struct genePred *pred, int exonNum,
			int exonQStart, struct dnaSeq *transcriptSequence,
			struct lm *lm)
/* generate an effect from a different allele in an exon */
{
struct genePred *newPred = pred;
char *newSequence = gpFxModifySequence(allele, pred, exonNum, exonQStart, transcriptSequence,
				       &newPred, lm);

if (sameString(newSequence, transcriptSequence->dna))
    return NULL;  // no change in transcript

struct gpFx *effectsList;
if (pred->cdsStart != pred->cdsEnd)
    effectsList = gpFxChangedCodingExon(allele, pred, exonNum, transcriptSequence->dna,
					newSequence, newPred, lm);
else
    {
    int cDnaPosition = firstChange(newSequence, transcriptSequence->dna, NULL);
    effectsList = gpFxChangedNoncodingExon(allele, pred, exonNum, cDnaPosition, lm);
    }

return effectsList;
}

static struct gpFx *gpFxCheckExons(struct variant *variantList, struct genePred *pred,
				   struct dnaSeq *transcriptSequence, struct lm *lm)
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
		struct gpFx *effect = gpFxNew("", pred->name,
					      complex_transcript_variant, none, lm);
		effectsList = slCat(effectsList, effect);
		}
	    struct allele *allele = variant->alleles;
	    for(; allele ; allele = allele->next)
		{
		effectsList = slCat(effectsList, gpFxInExon(allele, pred, ii, exonQStart,
							    transcriptSequence, lm));
		}
	    }
	}
    exonQStart += (pred->exonEnds[ii] - pred->exonStarts[ii]);
    }

return effectsList;
}

static struct gpFx *gpFxCheckIntrons(struct variant *variant, struct genePred *pred, struct lm *lm)
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
	struct gpFx *effects = gpFxNew("", pred->name, soNumber, intron, lm);
	effects->details.intron.intronNumber = minusStrand ? (pred->exonCount - ii - 1) : ii;
	slAddHead(&effectsList, effects);
	}
    else if ((variant->chromEnd > intronStart-3 && variant->chromStart < intronStart) ||
	     (variant->chromEnd > intronEnd && variant->chromStart < intronEnd+3))
	{
	// if variant is in exon *but* within 3 bases of splice site,
	// it also qualifies as splice_region_variant:
	struct gpFx *effects = gpFxNew("", pred->name, splice_region_variant, intron, lm);
	effects->details.intron.intronNumber = minusStrand ? (pred->exonCount - ii - 1) : ii;
	slAddHead(&effectsList, effects);
	}
    }
return effectsList;
}


static struct gpFx *gpFxCheckBackground(struct variant *variant, struct genePred *pred,
					struct lm *lm)
// check to see if the variant is up or downstream or in intron of the gene 
{
struct gpFx *effectsList = NULL;

for(; variant ; variant = variant->next)
    {
    // is this variant in an intron
    effectsList = slCat(effectsList, gpFxCheckIntrons(variant, pred, lm));

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
	struct gpFx *effects = gpFxNew("", pred->name, soNumber, none, lm);
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

// check to see if SNP is up or downstream in intron 
effectsList = slCat(effectsList, gpFxCheckBackground(variant, pred, lm));

// check to see if SNP is in the transcript
effectsList = slCat(effectsList, gpFxCheckExons(variant, pred, transcriptSequence, lm));

//#*** sort by position

return effectsList;
}
