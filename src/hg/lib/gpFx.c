
#include "common.h"
#include "genePred.h"
#include "gpFx.h"

struct gpFx *gpFxInCodingExon(struct variant *variant, struct genePred *pred, 
    int exonNum, char **returnTranscript, char **returnCoding)
{
//if ((pred->optFields & genePredExonFramesFld) == 0)
 //   genePredAddExonFrames(pred);

return NULL;
}

struct gpFx *gpFxInExon(struct variant *variant, struct genePred *pred, 
    int exonNum, char **returnTranscript, char **returnCoding)
{
struct gpFx *effectsList = NULL;

if (positiveRangeIntersection(pred->cdsStart, pred->cdsEnd,
	variant->chromStart, variant->chromEnd))
    {
    // we're in CDS
    effectsList = slCat(effectsList, gpFxInCodingExon(variant, pred, exonNum,
	    returnTranscript, returnCoding));
    }

if (positiveRangeIntersection(pred->txStart, pred->cdsStart,
	variant->chromStart, variant->chromEnd))
    {
    // we're in 5' UTR ( or UTR intron )
    }

if (positiveRangeIntersection(pred->txStart, pred->cdsStart,
	variant->chromStart, variant->chromEnd))
    {
    // we're in 3' UTR
    }

return effectsList;
}


static struct gpFx *gpFxCheckExons(struct variant *variant, 
    struct genePred *pred, char **returnTranscript, char **returnCoding)
// check to see if the variant is in an exon
{
int ii;
struct gpFx *effectsList = NULL;

for(; variant ; variant = variant->next)
    {
    for(ii=0; ii < pred->exonCount; ii++)
	{
	// check if in an exon
	if (positiveRangeIntersection(pred->exonStarts[ii], pred->exonEnds[ii], 
		variant->chromStart, variant->chromEnd))
	    {
	    // we know we've changed the transcript, start to track it

	    effectsList = slCat(effectsList, gpFxInExon(variant, pred, ii,
		returnTranscript, returnCoding));
	    }
	}
    }

return effectsList;
}

static struct gpFx *gpFxCheckIntrons(struct variant *variant, 
    struct genePred *pred)
// check to see if a single variant is in an exon or an intron
{
int ii;
struct gpFx *effectsList = NULL;

for(ii=0; ii < pred->exonCount - 1; ii++)
    {
    // check if in intron
    if (positiveRangeIntersection(pred->exonEnds[ii], 
	    pred->exonStarts[ii+1],
	    variant->chromStart, variant->chromEnd))
	{
	struct gpFx *effects;
	AllocVar(effects);
	effects->gpFxType = gpFxIntron;
	effects->gpFxNumber = ii;
	slAddHead(&effectsList, effects);
	}
    }

return effectsList;
}


static struct gpFx *gpFxCheckBackground(struct variant *variant, 
    struct genePred *pred)
// check to see if the variant is up or downstream or in intron of the gene 
{
struct gpFx *effectsList = NULL, *effects;

for(; variant ; variant = variant->next)
    {
    // is this variant in an intron
    effectsList = slCat(effectsList, gpFxCheckIntrons(variant, pred));

    if (positiveRangeIntersection(pred->txStart - GPRANGE, pred->txStart,
	    variant->chromStart, variant->chromEnd))
	{
	AllocVar(effects);
	if (*pred->strand == '+')
	    effects->gpFxType = gpFxUpstream;
	else
	    effects->gpFxType = gpFxDownstream;
	effectsList = slCat(effectsList, effects);
	}

    if (positiveRangeIntersection(pred->txEnd, pred->txEnd + GPRANGE,
	    variant->chromStart, variant->chromEnd))
	{
	AllocVar(effects);
	if (*pred->strand == '+')
	    effects->gpFxType = gpFxDownstream;
	else
	    effects->gpFxType = gpFxUpstream;
	effectsList = slCat(effectsList, effects);
	}
    }

return effectsList;
}

struct gpFx *gpFxPredEffect(struct variant *variant, struct genePred *pred,
    char **returnTranscript, char **returnCoding)
// return the predicted effect(s) of a variation list on a genePred
{
struct gpFx *effectsList = NULL;

// check to see if SNP is up or downstream in intron 
effectsList = slCat(effectsList, gpFxCheckBackground(variant, pred));

// check to see if SNP is in the transcript
effectsList = slCat(effectsList, gpFxCheckExons(variant, pred, 
    returnTranscript, returnCoding));

if (effectsList != NULL)
    return effectsList;

// default is no effect
struct gpFx *noEffect;

AllocVar(noEffect);
noEffect->next = NULL;
noEffect->gpFxType = gpFxNone;

return noEffect;
}
