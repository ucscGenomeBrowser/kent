
#include "common.h"
#include "genePred.h"
#include "gpFx.h"

char *gpFxModifySequence(struct allele *allele, struct genePred *pred, 
    int exonNum, struct psl *transcriptPsl, struct dnaSeq *transcriptSequence)
/* modify a transcript to what it'd be if the alternate allele were present */
{
//if ((pred->optFields & genePredExonFramesFld) == 0)
 //   genePredAddExonFrames(pred);

// change transcript at variant point
int exonOffset = allele->variant->chromStart - transcriptPsl->tStarts[exonNum];
int transcriptOffset = transcriptPsl->qStarts[exonNum] + exonOffset;

if (allele->length != allele->variant->chromEnd - allele->variant->chromStart)
    errAbort("only support alleles the same length as the reference");

char *retSequence = cloneString(transcriptSequence->dna);
if (*pred->strand == '-')
    transcriptOffset = transcriptSequence->size - (transcriptOffset + 1);

// make the change in the sequence
memcpy(&retSequence[transcriptOffset], allele->sequence, 
    allele->length);

return retSequence;
}

char *getCodingSequence(struct genePred *pred, char *transcriptSequence)
/* extract the CDS from a transcript */
{
int ii;

if (*pred->strand == '-')
    reverseComplement(transcriptSequence, strlen(transcriptSequence));

// trim off the 5'
char *ptr = transcriptSequence;
for(ii=0; ii < pred->exonCount; ii++)
    {
    int exonSize = pred->exonEnds[ii] - pred->exonStarts[ii];
    if (pred->cdsStart > pred->exonStarts[ii])
	break;

    ptr += exonSize;
    }
int exonOffset = pred->cdsStart - pred->exonStarts[ii];
ptr += exonOffset;

char *newString = cloneString(ptr);

// trim off 3'
newString[genePredCdsSize(pred)] = 0;

if (*pred->strand == '-')
    {
    reverseComplement(transcriptSequence, strlen(transcriptSequence));
    reverseComplement(newString, strlen(newString));
    }

return newString;
}

static int firstChange(char *string1, char *string2)
/* return the position of the first difference between the two sequences */
{
int count = 0;

while (*string1++ == *string2++)
    count++;

return count;
}

struct gpFx *gpFxInCodingExon(struct allele *allele, struct genePred *pred, 
    int exonNum, struct psl *transcriptPsl, struct dnaSeq *transcriptSequence)
/* generate an effect from a different allele in a coding exon */
{
struct gpFx *effectsList = NULL;
char *newSequence = gpFxModifySequence(allele, pred, exonNum,
	transcriptPsl, transcriptSequence);

if (sameString(newSequence, transcriptSequence->dna))
    return effectsList;  // no change in transcript

// check to see if coding sequence is changed
// calculate original coding AA's
char *oldCodingSequence = getCodingSequence(pred, transcriptSequence->dna);
struct dnaSeq *oldCodingDna = newDnaSeq(oldCodingSequence, 
	strlen(oldCodingSequence), pred->name);
aaSeq *oldaa = translateSeq(oldCodingDna, 0, FALSE);

// calculate variant coding AA's
char *newCodingSequence = getCodingSequence(pred, newSequence);
struct dnaSeq *newCodingDna = newDnaSeq(newCodingSequence, 
    strlen(newCodingSequence), pred->name);
aaSeq *newaa = translateSeq(newCodingDna, 0, FALSE);

//transcript ID, exon number(s), cDNA position, CDS position, peptide position, alternate amino acids, alternate codons" 9

if (sameString(newaa->dna, oldaa->dna))
    {
    // synonymous change
    }
else
    {
    // non-synonymous change
    struct gpFx *effects;
    AllocVar(effects);
    effects->so.soNumber = non_synonymous_variant;
    effects->so.sub.codingChange.transcript = cloneString(pred->name);
    if (*pred->strand == '-')
	effects->so.sub.codingChange.exonNumber = exonNum;
	effects->so.sub.codingChange.exonNumber = pred->exonCount - exonNum;
    effects->so.sub.codingChange.cDnaPosition = firstChange( newSequence, 
	transcriptSequence->dna);
    effects->so.sub.codingChange.cdsPosition = firstChange( newCodingSequence,
	oldCodingSequence);
    effects->so.sub.codingChange.pepPosition = firstChange( newaa->dna,
	oldaa->dna);
	    /*
	    char *aaChanges;
	    char *codonChanges;
	    */
    slAddHead(&effectsList, effects);
    }

return effectsList;
}


struct gpFx *gpFxInExon(struct allele *allele, struct genePred *pred, 
    int exonNum, struct psl *transcriptPsl, struct dnaSeq *transcriptSequence)
{
struct gpFx *effectsList = NULL;

if (positiveRangeIntersection(pred->cdsStart, pred->cdsEnd,
	allele->variant->chromStart, allele->variant->chromEnd))
    {
    // we're in CDS
    effectsList = slCat(effectsList, gpFxInCodingExon(allele, pred, exonNum,
	transcriptPsl, transcriptSequence));
    }

if (positiveRangeIntersection(pred->txStart, pred->cdsStart,
	allele->variant->chromStart, allele->variant->chromEnd))
    {
    // we're in 5' UTR ( or UTR intron )
    }

if (positiveRangeIntersection(pred->txStart, pred->cdsStart,
	allele->variant->chromStart, allele->variant->chromEnd))
    {
    // we're in 3' UTR
    }

return effectsList;
}

struct psl *genePredToPsl(struct genePred *pred)
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
	    if ((size = positiveRangeIntersection(pred->exonStarts[ii], 
		pred->exonEnds[ii], 
		variant->chromStart, variant->chromEnd)) > 0)
		{

		if (size != variant->chromEnd - variant->chromStart)
		    errAbort("variant crosses exon boundary");

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
    if (positiveRangeIntersection(pred->exonEnds[ii], 
	    pred->exonStarts[ii+1],
	    variant->chromStart, variant->chromEnd))
	{
	struct gpFx *effects;
	AllocVar(effects);
	effects->so.soNumber = intron_variant;
	effects->so.sub.intron.transcript = cloneString(pred->name);
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
struct gpFx *effectsList = NULL, *effects;

for(; variant ; variant = variant->next)
    {
    // is this variant in an intron
    effectsList = slCat(effectsList, gpFxCheckIntrons(variant, pred));

    if (positiveRangeIntersection(pred->txStart - GPRANGE, pred->txStart,
	    variant->chromStart, variant->chromEnd))
	{
	AllocVar(effects);
	if (*pred->strand == '+')
	    ;//effects->gpFxType = gpFxUpstream;
	else
	    ;//ffects->gpFxType = gpFxDownstream;
	effectsList = slCat(effectsList, effects);
	}

    if (positiveRangeIntersection(pred->txEnd, pred->txEnd + GPRANGE,
	    variant->chromStart, variant->chromEnd))
	{
	AllocVar(effects);
	if (*pred->strand == '+')
	    ;//ffects->gpFxType = gpFxDownstream;
	else
	    ;//ffects->gpFxType = gpFxUpstream;
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

if (effectsList != NULL)
    return effectsList;

// default is no effect
struct gpFx *noEffect;

AllocVar(noEffect);
noEffect->next = NULL;
;//oEffect->gpFxType = gpFxNone;

return noEffect;
}
