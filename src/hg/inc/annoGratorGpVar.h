/* annoGratorGpVar -- integrate pgSNP or VCF with gene pred and make gpFx predictions */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef ANNOGRATORGPVAR_H
#define ANNOGRATORGPVAR_H

#include "annoGrator.h"

struct annoGratorGpVarFuncFilter
    {
    boolean intergenic;		// Include variants not close to any gene
    boolean upDownstream;	// Include variants upstream or downstream of gene
    boolean nmdTranscript;	// Include variants in transcripts already subject to NMD
    boolean exonLoss;		// Include variants that cause exon loss
    boolean utr;		// Include variants in 5' or 3' UTR
    boolean cdsSyn;		// Include synonymous variants in CDS
    boolean cdsNonSyn;		// Include non-synonymous variants in CDS
    boolean intron;		// Include variants in intron
    boolean splice;		// Include variants in splice site or splice region
    boolean nonCodingExon;	// Include variants in exons of non-coding genes
    boolean noVariation;	// Include "variants" that are actually not variants
    };

struct annoGrator *annoGratorGpVarNew(struct annoStreamer *mySource);
/* Make a subclass of annoGrator that combines genePreds from mySource with
 * pgSnp rows from primary source to predict functional effects of variants
 * on genes.
 * mySource becomes property of the new annoGrator (don't close it, close annoGrator). */

void annoGratorGpVarSetFuncFilter(struct annoGrator *gpVar,
				  struct annoGratorGpVarFuncFilter *funcFilter);
/* If funcFilter is non-NULL, it specifies which functional categories
 * to include in output; if NULL, by default intergenic variants are excluded and
 * all other categories are included.
 * NOTE: This calls gSelf->setOverlapRule() with the currently set overlap rule because
 * overlapRule is affected by filter settings.  */

struct gpFx *annoGratorGpVarGpFxFromRow(struct annoStreamer *gpVar, struct annoRow *row,
					struct lm *lm);
// Turn the string array representation back into a real gpFx.
// I know this is inefficient and am thinking about a better way.

void annoGratorGpVarSetHgvsOutOptions(struct annoGrator *gSelf, uint hgvsOutOptions);
/* Import the HGVS output options described in hgHgvs.h */

#endif /* ANNOGRATORGPVAR_H*/
