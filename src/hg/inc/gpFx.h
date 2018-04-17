/* gpFx --- routines to calculate the effect of variation on a genePred; variantProjector.c can
 * also produce struct gpFx results using PSL+CDS+sequence, more accurate than genePred. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef GPFX_H
#define GPFX_H

#include "variant.h"
#include "soTerm.h"

// a single gpFx variant effect call
struct gpFx
    {
    struct gpFx *next;
    char *gAllele;		// Genomic allele sequence used to determine functional effect
    char *transcript;		// ID of feature affected by this call
    uint soNumber;		// Sequence Ontology Number of effect
    enum detailType		// This tells which value to use for 'union details' below
	{
	unknown,		// Catch uninitialized (except for needMem) use
	codingChange,		// (non)synonymous variant, deletions in CDS
	nonCodingExon,		// variant in non-coding gene or UTR of coding gene
	intron,			// intron_variant
	none			// variant for which soNumber is enough (e.g. up/downstream)
	} detailType;
    union details
	{
	struct codingChange     // (non)synonymous variant, deletions in CDS
	    {
	    uint exonNumber;	// 0-based exon number (from genePred, beware false "introns")
            uint exonCount;     // Total number of exons (sometimes less than aligned blocks)
	    uint cDnaPosition;	// offset of variant in transcript cDNA
	    char *txRef;	// Transcript reference allele (usually == genomic ref % strand)
	    char *txAlt;	// Transcript alternate allele (usually == gAllele % strand)
	    uint cdsPosition;	// offset of variant from transcript's cds start
	    uint pepPosition;	// offset of variant in translated product
	    char *aaOld;	// peptides, before change by variant (starting at pepPos)
	    char *aaNew;	// peptides, changed by variant
	    char *codonOld;	// codons, before change by variant (starting at cdsPos)
	    char *codonNew;	// codons, changed by variant
	    } codingChange;
	struct nonCodingExon	// variant in non-coding gene or UTR of coding gene
	    {
	    uint exonNumber;	// 0-based exon number (from genePred, beware false "introns")
            uint exonCount;     // Total number of exons (sometimes less than aligned blocks)
	    uint cDnaPosition;	// offset of variant in transcript cDNA
	    char *txRef;	// Transcript reference allele (usually == genomic ref % strand)
	    char *txAlt;	// Transcript alternate allele (usually == gAllele % strand)
	    } nonCodingExon;
	struct intron 		// intron_variant
	    {
	    uint intronNumber;	// 0-based intron number (from genePred, beware false "introns")
            uint intronCount;   // Total number of introns (sometimes less than aligned blocks - 1)
	    } intron;
	} details;
    };

struct gpFx *gpFxPredEffect(struct variant *variant, struct genePred *pred,
			    struct dnaSeq *transcriptSequence, struct lm *lm);
// return the predicted effect(s) of a variation list on a genePred

// number of bases up or downstream that we flag
#define GPRANGE 5000

boolean hasAltAllele(struct allele *alleles);
/* Return TRUE if alleles include at least one non-reference allele. */

char *firstAltAllele(struct allele *alleles);
/* Ensembl always reports an alternate allele, even if that allele is not being used
 * to calculate any consequence.  When allele doesn't really matter, just use the
 * first alternate allele that is given. */

struct gpFx *gpFxNew(char *gAllele, char *transcript, enum soTerm soNumber,
		     enum detailType detailType, struct lm *lm);
/* Fill in the common members of gpFx; leave soTerm-specific members for caller to fill in. */

struct gpFx *gpFxNoVariation(struct variant *variant, struct lm *lm);
/* Return a gpFx with SO term no_sequence_alteration, for VCF rows that aren't really variants. */

void gpFxSetNoncodingInfo(struct gpFx *gpFx, int exonIx, int exonCount, int cdnaPos,
                          char *txRef, char *txAlt, struct lm *lm);
/* This gpFx is for a variant in exon of non-coding gene or UTR exon of coding gene;
 * set details.nonCodingExon values. */

#endif /* GPFX_H */
