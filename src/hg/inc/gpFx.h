#ifndef GPFX_H
#define GPFX_H

#include "variant.h"
#include "soTerm.h"

// a single gpFx variant effect call
struct gpFx
    {
    struct gpFx *next;
    char *allele;		// Allele sequence used to determine functional effect
    struct soCall so;		// Sequence Ontology ID of effect and plus associated data
    };

struct gpFx *gpFxPredEffect(struct variant *variant, struct genePred *pred,
    struct dnaSeq *transcriptSequence);
// return the predicted effect(s) of a variation list on a genePred

// number of bases up or downstream that we flag
#define GPRANGE 5000

#define gpFxIsCodingChange(gpfx) (gpfx->so.soNumber == inframe_deletion || \
				  gpfx->so.soNumber == inframe_insertion || \
				  gpfx->so.soNumber == frameshift_variant || \
				  gpfx->so.soNumber == synonymous_variant || \
				  gpfx->so.soNumber == missense_variant || \
				  gpfx->so.soNumber == stop_gained)

#endif /* GPFX_H */
