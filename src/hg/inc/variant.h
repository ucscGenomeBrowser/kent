/* variant.h -- a generic variant.  Meant to capture information that's in VCF or pgSNP. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef VARIANT_H
#define VARIANT_H

#include "annoRow.h"
#include "localmem.h"
#include "pgSnp.h"

struct allele   // a single allele in a variant. 
    {
    struct allele *next;
    struct variant *variant;
    char *sequence;
    int length;
    boolean isReference;
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
			   char *slashSepAlleles, char *refAllele, struct lm *lm);
/* Create a variant from basic information that is easy to extract from most other variant
 * formats: coords, allele count, string of slash-separated alleles and reference allele. */

struct variant *variantFromPgSnpAnnoRow(struct annoRow *row, char *refAllele, boolean hasBin,
                                        struct lm *lm);
/* Translate pgSnp annoRow into variant (allocated by lm). */

struct variant *variantFromVcfAnnoRow(struct annoRow *row, char *refAllele, struct lm *lm,
				      struct dyString *dyScratch);
/* Translate vcf array of words into variant (allocated by lm, overwriting dyScratch
 * as temporary scratch string). */

struct allele  *alleleClip(struct allele *allele, int sx, int ex, struct lm *lm);
/* Return new allele pointing to new variant, both clipped to region defined by sx..ex. */

#endif /* VARIANT_H*/
