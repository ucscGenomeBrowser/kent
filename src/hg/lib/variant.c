/* variant.c -- routines to convert other variant formats to a generic
 *              variant structure */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "annoRow.h"
#include "variant.h"

struct allele  *alleleClip(struct allele *allele, int sx, int ex, struct lm *lm)
/* Return new allele pointing to new variant, both clipped to region defined by [sx,ex). */
{
struct variant *oldVariant = allele->variant;
int start = oldVariant->chromStart;
int end = oldVariant->chromEnd;
int delFront = 0;
int delRear = 0;

if (start < sx)
    {
    delFront = min(sx - start, allele->length);
    start = sx;
    }

if (end > ex)
    {
    delRear = min(end - ex, allele->length - delFront);
    end = ex;
    }

struct variant *newVariant;
lmAllocVar(lm, newVariant);
newVariant->chrom = lmCloneString(lm, oldVariant->chrom);
newVariant->chromStart = start;
newVariant->chromEnd = end;
newVariant->numAlleles = 1;

struct allele *newAllele;
lmAllocVar(lm, newAllele);
newVariant->alleles = newAllele;
newAllele->variant = newVariant;
newAllele->length = allele->length - delRear - delFront;
assert(newAllele->length >= 0);
newAllele->sequence = lmCloneString(lm, &allele->sequence[delFront]);
newAllele->sequence[newAllele->length] = 0;   // cut off delRear part

return newAllele;
}

static boolean isDash(char *string)
/* Return TRUE if the only char in string is '-'
 * (possibly repeated like the darn pgVenter alleles). */
{
char *p;
for (p = string;  p != NULL && *p != '\0';  p++)
    if (*p != '-')
	return FALSE;
return TRUE;
}

struct variant *variantNew(char *chrom, unsigned start, unsigned end, unsigned numAlleles,
			   char *slashSepAlleles, char *refAllele, struct lm *lm)
/* Create a variant from basic information that is easy to extract from most other variant
 * formats: coords, allele count, string of slash-separated alleles and reference allele. */
{
struct variant *variant;

// We have a new variant!
lmAllocVar(lm, variant);
variant->chrom = lmCloneString(lm, chrom);
variant->chromStart = start;
variant->chromEnd = end;
variant->numAlleles = numAlleles;

// get the alleles.
char *nextAlleleString = lmCloneString(lm, slashSepAlleles);
int alleleNumber = 0;
for( ; alleleNumber < numAlleles; alleleNumber++)
    {
    if (nextAlleleString == NULL)
	errAbort("number of alleles in /-separated string doesn't match numAlleles");
    
    char *thisAlleleString = nextAlleleString;

    // advance pointer to next variant string
    // probably there's some kent routine to do this behind the curtain
    nextAlleleString = strchr(thisAlleleString, '/');
    if (nextAlleleString)	 // null out '/' and move to next char
	{
	*nextAlleleString = 0;
	nextAlleleString++;
	}

    boolean isRefAllele = (sameWord(thisAlleleString, refAllele) ||
			   (isEmpty(refAllele) && sameString(thisAlleleString, "-")));
    int alleleStringLength = strlen(thisAlleleString);
    if (isDash(thisAlleleString))
	{
	alleleStringLength = 0;
	thisAlleleString[0] = '\0';
	}

    // we have a new allele!
    struct allele *allele;
    AllocVar(allele);
    slAddHead(&variant->alleles, allele);
    allele->variant = variant;
    allele->length = alleleStringLength; 
    allele->sequence = lmCloneString(lm, thisAlleleString);
    allele->isReference = isRefAllele;
    }

slReverse(&variant->alleles);

return variant;
}

struct variant *variantFromPgSnpAnnoRow(struct annoRow *row, char *refAllele, struct lm *lm)
/* Translate pgSnp annoRow into variant (allocated by lm). */
{
struct pgSnp pgSnp;
pgSnpStaticLoad(row->data, &pgSnp);
return variantNew(pgSnp.chrom, pgSnp.chromStart, pgSnp.chromEnd, pgSnp.alleleCount,
		  pgSnp.name, refAllele, lm);
}

struct variant *variantFromVcfAnnoRow(struct annoRow *row, char *refAllele, struct lm *lm,
				      struct dyString *dyScratch)
/* Translate vcf array of words into variant (allocated by lm, overwriting dyScratch
 * as temporary scratch string). */
{
char **words = row->data;
char *alStr = vcfGetSlashSepAllelesFromWords(words, dyScratch);
// The reference allele is the first allele in alStr -- and it may be trimmed on both ends with
// respect to the raw VCF ref allele in words[3], so copy vcfRefAllele back out of alStr.
// That ensures that variantNew will get the reference allele that matches the slash-separated
// allele string.
int refLen = strlen(alStr);
char *p = strchr(alStr, '/');
if (p)
    refLen = p - alStr;
char vcfRefAllele[refLen + 1];
safencpy(vcfRefAllele, sizeof(vcfRefAllele), alStr, refLen);
unsigned alCount = countChars(alStr, '/') + 1;
return variantNew(row->chrom, row->start, row->end, alCount, alStr, vcfRefAllele, lm);
}

