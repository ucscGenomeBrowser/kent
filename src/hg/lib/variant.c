/* variant.c -- routines to convert other variant formats to a generic
 *              variant structure */

#include "common.h"
#include "variant.h"

struct allele  *alleleClip(struct allele *allele, int sx, int ex)
/* clip allele to be inside region defined by sx..ex.  Returns 
 * pointer to new allele which should be freed by alleleFree, or variantFree
 */
{
struct variant *oldVariant = allele->variant;
int start = oldVariant->chromStart;
int end = oldVariant->chromEnd;
int oldVariantWidth = end - start;
int delFront = 0;
int delRear = 0;

if (start < sx)
    {
    if (oldVariantWidth != allele->length)	 /* FIXME */
	errAbort("cannot clip alleles that are a different length than variant region");
    delFront = sx - start;
    start = sx;
    }

if (end > ex)
    {
    if (oldVariantWidth != allele->length)	 /* FIXME */
	errAbort("cannot clip alleles that are a different length than variant region");
    delRear = end - ex;
    end = ex;
    }

struct variant *newVariant;
AllocVar(newVariant);
newVariant->chrom = cloneString(oldVariant->chrom);
newVariant->chromStart = start;
newVariant->chromEnd = end;
newVariant->numAlleles = 1;
struct allele *newAllele;
AllocVar(newAllele);
newVariant->alleles = newAllele;
newAllele->variant = newVariant;
newAllele->length = allele->length - delRear - delFront;
assert(newAllele->length >= 0);
newAllele->sequence = cloneString(&allele->sequence[delFront]);
newAllele->sequence[newAllele->length] = 0;   // cut off delRear part

return newAllele;
}

static char *addDashes(char *input, int count)
/* add dashes at the end of a sequence to pad it out so it's length is count */
{
char *ret = needMem(count + 1);
int inLen = strlen(input);

safecpy(ret, count + 1, input);
count -= inLen;

char *ptr = &ret[inLen];
while(count--)
    *ptr++ = '-';

return ret;
}

struct variant *variantNew(char *chrom, unsigned start, unsigned end, unsigned numAlleles,
			   char *slashSepAlleles, struct lm *lm)
/* Create a variant from basic information that is easy to extract from most other variant
 * formats: coords, allele count, and string of slash-separated alleles. */
{
struct variant *variant;

// this is probably the wrong way to do this.  Alleles in
// variant should be their size in query bases
int alleleLength = end - start;

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
	errAbort("number of alleles in pgSnp doesn't match number in name");
    
    char *thisAlleleString = nextAlleleString;

    // advance pointer to next variant string
    // probably there's some kent routine to do this behind the curtain
    nextAlleleString = strchr(thisAlleleString, '/');
    if (nextAlleleString)	 // null out '/' and move to next char
	{
	*nextAlleleString = 0;
	nextAlleleString++;
	}

    // this check probably not right, could be different per allele
    int alleleStringLength = strlen(thisAlleleString);
    if (sameString(thisAlleleString, "-") && alleleLength == 0)
	{
	alleleStringLength = 0;
	thisAlleleString[0] = '\0';
	}
    else if (alleleStringLength != alleleLength)
	{
	if ( alleleStringLength < alleleLength)
	    {
	    thisAlleleString = addDashes(thisAlleleString, alleleLength);
	    alleleStringLength = alleleLength;
	    }
	}

    // we have a new allele!
    struct allele *allele;
    AllocVar(allele);
    slAddHead(&variant->alleles, allele);
    allele->variant = variant;
    allele->length = alleleStringLength; 
    toLowerN(thisAlleleString, alleleStringLength);
    allele->sequence = lmCloneString(lm, thisAlleleString);
    }

slReverse(&variant->alleles);

return variant;
}

struct variant *variantFromPgSnp(struct pgSnp *pgSnp, struct lm *lm)
/* convert pgSnp record to variant record */
{
return variantNew(pgSnp->chrom, pgSnp->chromStart, pgSnp->chromEnd, pgSnp->alleleCount,
		  pgSnp->name, lm);
}

