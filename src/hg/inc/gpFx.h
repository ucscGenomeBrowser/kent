#ifndef GPFX_H
#define GPFX_H

#include "variant.h"
#include "soterm.h"

// a single gpFx variant effect call
struct gpFx
{
struct gpFx *next;

struct soCall so;
};

struct gpFx *gpFxPredEffect(struct variant *variant, struct genePred *pred,
    struct dnaSeq *transcriptSequence);
// return the predicted effect(s) of a variation list on a genePred

// number of bases up or downstream that we flag
#define GPRANGE 500

#define gpFxIsCodingChange(gpfx) (gpfx->so.soNumber == inframe_deletion || \
				  gpfx->so.soNumber == frameshift_variant || \
				  gpfx->so.soNumber == synonymous_variant || \
				  gpfx->so.soNumber == non_synonymous_variant)

#endif /* GPFX_H */
