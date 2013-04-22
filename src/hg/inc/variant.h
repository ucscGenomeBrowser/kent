/* variant.h -- a generic variant.  Meant to capture information that's in VCF or pgSNP. */

#ifndef VARIANT_H
#define VARIANT_H

#include "localmem.h"
#include "pgSnp.h"

struct allele   // a single allele in a variant. 
    {
    struct allele *next;
    struct variant *variant;
    int length;
    char *sequence;
    };

struct variant   // a single variant
    {
    struct variant *next;  /* Next in singly linked list. */
    char *chrom;	/* Chromosome */
    unsigned chromStart;	/* Start position in chrom */
    unsigned chromEnd;	/* End position in chrom */
    unsigned numAlleles;   /* the number of alleles */
    struct allele *alleles;	/* alleles */
    };

struct variant *variantNew(char *chrom, unsigned start, unsigned end, unsigned numAlleles,
			   char *slashSepAlleles, struct lm *lm);
/* Create a variant from basic information that is easy to extract from most other variant
 * formats: coords, allele count, and string of slash-separated alleles. */

struct variant *variantFromPgSnp(struct pgSnp *pgSnp, struct lm *lm);
/* convert pgSnp record to variant record */

struct allele  *alleleClip(struct allele *allele, int sx, int ex, struct lm *lm);
/* Return new allele pointing to new variant, both clipped to region defined by sx..ex. */

#endif /* VARIANT_H*/
