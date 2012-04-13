
#ifndef VARIANT_H
#define VARIANT_H

#include "pgSnp.h"

struct allele
    {
    struct allele *next;
    int length;
    char *sequence;
    };

struct variant
    {
    struct variant *next;  /* Next in singly linked list. */
    char *chrom;	/* Chromosome */
    unsigned chromStart;	/* Start position in chrom */
    unsigned chromEnd;	/* End position in chrom */
    struct allele *alleles;	/* alleles */
    };

struct variant *variantFromPgSnp(struct pgSnp *pgSnp);
/* convert pgSnp record to variant record */

#endif /* VARIANT_H*/
