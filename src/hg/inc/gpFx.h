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
    char **returnTranscript, char **returnCoding);
// return the predicted effect(s) of a variation list on a genePred

// number of bases up or downstream that we flag
#define GPRANGE 500

#endif /* GPFX_H */
