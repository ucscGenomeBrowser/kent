/* gpFx --- routines to calculate the effect of variation on a genePred */

#ifndef GPFX_H
#define GPFX_H

#include "variant.h"
#include "soTerm.h"

// a single gpFx variant effect call
struct gpFx
    {
    struct gpFx *next;
    char *allele;		// Allele sequence used to determine functional effect
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
	    uint cDnaPosition;	// offset of variant in transcript cDNA
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
	    uint cDnaPosition;	// offset of variant in transcript cDNA
	    } nonCodingExon;
	struct intron 		// intron_variant
	    {
	    uint intronNumber;	// 0-based intron number (from genePred, beware false "introns")
	    } intron;
	} details;
    };

struct gpFx *gpFxPredEffect(struct variant *variant, struct genePred *pred,
			    struct dnaSeq *transcriptSequence, struct lm *lm);
// return the predicted effect(s) of a variation list on a genePred

// number of bases up or downstream that we flag
#define GPRANGE 5000

#endif /* GPFX_H */
