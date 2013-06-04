/* annoGratorGpVar -- integrate pgSNP or VCF with gene pred and make gpFx predictions */

#ifndef ANNOGRATORGPVAR_H
#define ANNOGRATORGPVAR_H

#include "annoGrator.h"

struct annoGratorGpVarFuncFilter
    {
    boolean intergenic;		// Include variants not close to any gene
    boolean upDownstream;	// Include variants upstream or downstream of gene
    boolean utr;		// Include variants in 5' or 3' UTR
    boolean cdsSyn;		// Include synonymous variants in CDS
    boolean cdsNonSyn;		// Include non-synonymous variants in CDS
    boolean intron;		// Include variants in intron
    boolean splice;		// Include variants in splice site or splice region
    boolean nonCodingExon;	// Include variants in exons of non-coding genes
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
 * NOTE: After calling this, call gpVar->setOverlapRule() because implementation
 * of that depends on filter settings. */

struct gpFx *annoGratorGpVarGpFxFromRow(struct annoStreamer *gpVar, struct annoRow *row,
					struct lm *lm);
// Turn the string array representation back into a real gpFx.
// I know this is inefficient and am thinking about a better way.

#endif /* ANNOGRATORGPVAR_H*/
