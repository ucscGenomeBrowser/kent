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

struct gpFx *gpFxNew(char *allele, char *transcript, enum soTerm soNumber)
/* Fill in the common members of gpFx; leave soTerm-specific members for caller to fill in. */
{
struct gpFx *effect;
AllocVar(effect);
effect->allele = collapseDashes(strUpper(cloneString(allele)));
effect->so.transcript = cloneString(transcript);
effect->so.soNumber = soNumber;
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
    char *newAlleleSeq, int alleleLength)
/* merge a variant into an allele */
{
char *newTranscript = transcript;

if (variantWidth == alleleLength)
    {
    // for the moment, we're sticking the dashes into the transcripts
    if (1) //noDashes(newAlleleSeq))
	{
	memcpy(&transcript[offset], newAlleleSeq, alleleLength);
	}
#ifdef DOESNTKNOWABOUTTHEOFFSETARGUMENT
    // if we decide not to put the dashes into the resulting transcripts
    // this will have to be re-written to grok the offset argument
    else
	{
	char *transcriptSource = transcript;
	int ii;
	for(ii=0; ii < variantWidth; ii++)
	    {
	    if (*newAlleleSeq == '-')
		{
		transcriptSource++;
		}
	    else
		{
		*transcript++ = *newAlleleSeq++;
		transcriptSource++;
		}
	    }
	while(*transcriptSource)
	    *transcript++ = *transcriptSource++;
	*transcript = 0;
	}
#endif
    }
else  if (variantWidth > alleleLength)
    errAbort("allele not padded");
else 
    {
    int insertionSize = alleleLength - variantWidth;
    int newLength = strlen(transcript) + insertionSize;
    newTranscript = needMem(newLength + 1);
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
			 int exonNum, struct psl *transcriptPsl, struct dnaSeq *transcriptSequence,
			 struct genePred **retNewPred)
/* modify a transcript to what it'd be if the alternate allele were present;
 * if transcript length changes, make retNewPred a shallow copy of pred w/tweaked coords. */
{
// clip allele to exon
struct allele *clipAllele = alleleClip(allele, transcriptPsl->tStarts[exonNum], 
	transcriptPsl->tStarts[exonNum] + transcriptPsl->blockSizes[exonNum]);

// change transcript at variant point
int exonOffset = clipAllele->variant->chromStart - transcriptPsl->tStarts[exonNum];
int transcriptOffset = transcriptPsl->qStarts[exonNum] + exonOffset;

int variantWidth = clipAllele->variant->chromEnd - clipAllele->variant->chromStart;

if (variantWidth != clipAllele->length && retNewPred != NULL)
    {
    // OK, now we have to make a new genePred that matches the changed sequence
    int basesAdded = clipAllele->length - variantWidth;
    struct genePred *newPred = CloneVar(pred);
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

char *retSequence = cloneString(transcriptSequence->dna);
char *newAlleleSeq = cloneString(clipAllele->sequence);
if (*pred->strand == '-')
    {
    transcriptOffset = transcriptSequence->size - (transcriptOffset + strlen(newAlleleSeq));
    reverseComplement(newAlleleSeq, strlen(newAlleleSeq));
    }

// make the change in the sequence
retSequence = mergeAllele( retSequence, transcriptOffset, variantWidth, newAlleleSeq,
			   clipAllele->length);

// clean up
freeMem(newAlleleSeq);

return retSequence;
}

static char *getCodingSequence(struct genePred *pred, char *transcriptSequence)
/* extract the CDS from a transcript */
{
int ii;

if (*pred->strand == '-')
    reverseComplement(transcriptSequence, strlen(transcriptSequence));

// trim off the 5' UTR ( or 3' if on the minus strand)
char *ptr = transcriptSequence;
for(ii=0; ii < pred->exonCount; ii++)
    {
    int exonSize = pred->exonEnds[ii] - pred->exonStarts[ii];
    if ((pred->cdsStart >= pred->exonStarts[ii]) &&
	(pred->cdsStart < pred->exonEnds[ii]))
	break;

    ptr += exonSize;
    }
assert (ii != pred->exonCount);

// clip off part of UTR in exon that has CDS in it too
int exonOffset = pred->cdsStart - pred->exonStarts[ii];
ptr += exonOffset;

char *newString = cloneString(ptr);

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

for(; *string1 == *string2; count++, string1++, string2++)
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

static void getSequences(struct genePred *pred, char *transcriptSequence,
    char **codingSequence,  aaSeq **aaSeq)
/* get coding sequences for a transcript and a variant transcript */
{
*codingSequence = getCodingSequence(pred, transcriptSequence);
struct dnaSeq *codingDna = newDnaSeq(*codingSequence, strlen(*codingSequence), NULL);
*aaSeq = translateSeq(codingDna, 0, FALSE); // TRUE truncates at stop, but doesn't include the Z
char *stop = strchr((*aaSeq)->dna, 'Z');
if (stop != NULL)
    {
    *(stop+1) = '\0';
    // If aaSeq was truncated by a stop codon, truncate codingSequence as well:
    int codingLength = 3*strlen((*aaSeq)->dna);
    (*codingSequence)[codingLength] = '\0';
    }
freez(codingDna);
}

struct gpFx *gpFxCheckUtr( struct allele *allele, struct genePred *pred, 
    int exonNum, struct psl *transcriptPsl, struct dnaSeq *transcriptSequence,
    char *newSequence)
/* check for effects in UTR of coding gene */
{
//#*** positiveRangeInt doesn't work for 0-length insertion variants
if (positiveRangeIntersection(pred->txStart, pred->cdsStart,
	allele->variant->chromStart, allele->variant->chromEnd))
    {
    // we're in 5' UTR ( or UTR intron )
    //errAbort("don't support variants in 5' UTR");
    }

if (positiveRangeIntersection(pred->txStart, pred->cdsStart,
	allele->variant->chromStart, allele->variant->chromEnd))
    {
    // we're in 3' UTR
    //errAbort("don't support variants in 3' UTR");
    }

return NULL;
}

struct gpFx *gpFxChangedNoncodingTranscript( struct allele *allele, struct genePred *pred, 
    int exonNum, struct psl *transcriptPsl, struct dnaSeq *transcriptSequence,
    char *newSequence, struct genePred *newPred)
/* generate an effect for a variant in a non-coding transcript */
{
return NULL;
//    errAbort("found a change in non-coding gene. we don't support non-coding genes at the moment");
}

struct gpFx *gpFxChangedCodingTranscript( struct allele *allele, struct genePred *pred, 
    int exonNum, struct psl *transcriptPsl, struct dnaSeq *transcriptSequence,
    char *newSequence, struct genePred *newPred)
/* calculate effect of allele change on coding transcript */
{
struct gpFx *effectsList = NULL;

// first find effects of allele in UTR
effectsList = gpFxCheckUtr(allele, pred, exonNum, transcriptPsl, 
    transcriptSequence, newSequence);

// check to see if coding sequence is changed
// calculate original and variant coding AA's
char *oldCodingSequence, *newCodingSequence;
aaSeq *oldaa, *newaa;

getSequences(pred, transcriptSequence->dna, &oldCodingSequence, &oldaa);
getSequences(newPred, newSequence, &newCodingSequence, &newaa);

// if coding region hasn't changed, we're done
if (sameString(oldCodingSequence, newCodingSequence))
    return effectsList;

// start allocating the effect structure - fill in soNumber below
struct gpFx *effects = gpFxNew(allele->sequence, pred->name, 0);
slAddHead(&effectsList, effects);

struct codingChange *cc = &effects->so.sub.codingChange;
int dnaChangeLength;
cc->cDnaPosition = firstChange( newSequence, transcriptSequence->dna, &dnaChangeLength);
int cdsChangeLength;
cc->cdsPosition = firstChange( newCodingSequence, oldCodingSequence, &cdsChangeLength);
if (*pred->strand == '-')
    cc->exonNumber = pred->exonCount - exonNum;
else
    cc->exonNumber = exonNum;

// show specific coding changes by making before-and-after subsequences:
int codonPosStart = (cc->cdsPosition / 3) ;
int codonPosEnd = ((cdsChangeLength - 1 + cc->cdsPosition) / 3) ;
int numCodons = (codonPosEnd - codonPosStart + 1);
cc->codonOld = cloneStringZ(oldCodingSequence + codonPosStart*3, numCodons*3);
cc->codonNew = cloneStringZ(newCodingSequence + codonPosStart*3, numCodons*3);

cc->pepPosition = codonPosStart;
cc->aaOld = cloneStringZ(oldaa->dna + cc->pepPosition, numCodons);
cc->aaNew = cloneStringZ(newaa->dna + cc->pepPosition, numCodons);

int numDashes;
if (sameString(newaa->dna, oldaa->dna))
    {
    // synonymous change
    effects->so.soNumber = synonymous_variant;
    }
else
    {
    // this is assuming that deletions are marked with dashes
    // in newCodingSequence
    if ((numDashes = countDashes(newCodingSequence)) != 0)
	{
	if ((numDashes % 3) == 0)
	    effects->so.soNumber = inframe_deletion;
	else
	    effects->so.soNumber = frameshift_variant;
	}
    else if (newaa->size < oldaa->size)
	{
	effects->so.soNumber = stop_gained;
	}
    else if (newaa->size != oldaa->size)
	{
	effects->so.soNumber = inframe_insertion;
	}
    else
	{
	char *stopPos = strchr(newaa->dna, 'Z');
	// non-synonymous change: nonsense (early stop codon) or missense change?
	if (stopPos != NULL && stopPos < newaa->dna + newaa->size - 1)
	    effects->so.soNumber = stop_gained;
	else
	    effects->so.soNumber = missense_variant;
	}
    }

return effectsList;
}


struct gpFx *gpFxInExon(struct allele *allele, struct genePred *pred, 
    int exonNum, struct psl *transcriptPsl, struct dnaSeq *transcriptSequence)
/* generate an effect from a different allele in an exon */
{
struct genePred *newPred = pred;
char *newSequence = gpFxModifySequence(allele, pred, exonNum,
				       transcriptPsl, transcriptSequence, &newPred);

if (sameString(newSequence, transcriptSequence->dna))
    return NULL;  // no change in transcript

struct gpFx *effectsList;
if (pred->cdsStart != pred->cdsEnd)
    effectsList = gpFxChangedCodingTranscript( allele, pred, exonNum, transcriptPsl, 
					       transcriptSequence, newSequence, newPred);
else
    effectsList = gpFxChangedNoncodingTranscript( allele, pred, exonNum, transcriptPsl, 
						  transcriptSequence, newSequence, newPred);

if (newPred != pred)
    freeMem(newPred);
return effectsList;
}

static struct psl *genePredToPsl(struct genePred *pred)
/* generate a PSL alignment to a transcript from a genePred */
{
int qSize = genePredBases(pred);
#define BOGUS_CHROM_SIZE  0
struct psl *psl = pslNew(pred->name, qSize, 0, qSize,
                         pred->chrom, BOGUS_CHROM_SIZE, 
			 pred->txStart, pred->txEnd,
			 pred->strand, pred->exonCount, 0);
psl->match = psl->qSize;

int i, qNext = 0;
for (i = 0; i < pred->exonCount; i++)
    {
    int exonSize =  pred->exonEnds[i] - pred->exonStarts[i];

    psl->qStarts[i] = qNext;
    //psl->tStarts[i] = pred->exonStarts[i] + pred->txStart;
    psl->tStarts[i] = pred->exonStarts[i];
    psl->blockSizes[i] = exonSize;

/* notnow
    if (i > 0)
        {
        psl->tNumInsert += 1;
        psl->tBaseInsert += psl->tStarts[i] - pslTEnd(psl, i-1);
        }
*/

    psl->blockCount++;
    qNext += psl->blockSizes[i];
    }

return psl;
}

static struct gpFx *gpFxCheckExons(struct variant *variant, 
    struct genePred *pred, struct dnaSeq *transcriptSequence)
// check to see if the variant is in an exon
{
int ii;
struct gpFx *effectsList = NULL;
struct psl *transcriptPsl = genePredToPsl(pred);

// should copy transcriptSequence if we have more than one variant!!
for(; variant ; variant = variant->next)
    {
    struct allele *allele = variant->alleles;
    for(; allele ; allele = allele->next)
	{
	for(ii=0; ii < pred->exonCount; ii++)
	    {
	    // check if in an exon
	    int size;
	    int end = variant->chromEnd;
	    if (variant->chromStart == end)
		end++;
	    if ((size = positiveRangeIntersection(pred->exonStarts[ii], 
		pred->exonEnds[ii], 
		variant->chromStart, end)) > 0)
		{
		if (size != end - variant->chromStart)
		    {
		    struct gpFx *effect = gpFxNew(allele->sequence, pred->name,
						  complex_transcript_variant);
		    effectsList = slCat(effectsList, effect);
		    }
		effectsList = slCat(effectsList, gpFxInExon(allele, pred, ii,
		    transcriptPsl, transcriptSequence));
		}
	    }
	}
    }

return effectsList;
}

static struct gpFx *gpFxCheckIntrons(struct variant *variant, 
    struct genePred *pred)
// check to see if a single variant is in an exon or an intron
{
int ii;
struct gpFx *effectsList = NULL;

for(ii=0; ii < pred->exonCount - 1; ii++)
    {
    // check if in intron
    if (variant->chromEnd > pred->exonEnds[ii] &&
	variant->chromStart < pred->exonStarts[ii+1])
	{
	struct gpFx *effects = gpFxNew("", pred->name, intron_variant);
	effects->so.sub.intron.intronNumber = ii;
	slAddHead(&effectsList, effects);
	}
    }

return effectsList;
}


static struct gpFx *gpFxCheckBackground(struct variant *variant, 
    struct genePred *pred)
// check to see if the variant is up or downstream or in intron of the gene 
{
struct gpFx *effectsList = NULL;

for(; variant ; variant = variant->next)
    {
    // is this variant in an intron
    effectsList = slCat(effectsList, gpFxCheckIntrons(variant, pred));

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
	struct gpFx *effects = gpFxNew("", pred->name, soNumber);
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
    struct dnaSeq *transcriptSequence)
// return the predicted effect(s) of a variation list on a genePred
{
struct gpFx *effectsList = NULL;

// make sure we can deal with the variants that are coming in
checkVariantList(variant);

// check to see if SNP is up or downstream in intron 
effectsList = slCat(effectsList, gpFxCheckBackground(variant, pred));

// check to see if SNP is in the transcript
effectsList = slCat(effectsList, gpFxCheckExons(variant, pred, 
    transcriptSequence));

return effectsList;
}
