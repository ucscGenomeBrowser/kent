/* variant.c -- routines to convert other variant formats to a generic
 *              variant structure */

#include "common.h"
#include "variant.h"

struct variant *variantFromPgSnp(struct pgSnp *pgSnp)
/* convert pgSnp record to variant record */
{
struct variant *variant;

// this is probably the wrong way to do this.  Alleles in
// variant should be their size in query bases
int alleleLength = pgSnp->chromEnd - pgSnp->chromStart;

// We have a new variant!
AllocVar(variant);
variant->chrom = cloneString(pgSnp->chrom);
variant->chromStart = pgSnp->chromStart;
variant->chromEnd = pgSnp->chromEnd;
variant->numAlleles = pgSnp->alleleCount;

// get the alleles.
char *nextAlleleString = pgSnp->name;
int alleleNumber = 0;
for( ; alleleNumber < pgSnp->alleleCount; alleleNumber++)
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
    if (alleleStringLength != alleleLength)
	errAbort("length of allele number %d is %d, should be %d", 
	    alleleNumber, alleleStringLength, alleleLength);

    // we have a new allele!
    struct allele *allele;
    AllocVar(allele);
    slAddHead(&variant->alleles, allele);
    allele->variant = variant;
    allele->length = alleleStringLength;
    allele->sequence = cloneString(thisAlleleString);
    }

slReverse(&variant->alleles);

return variant;
}

