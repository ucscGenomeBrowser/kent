
#include "common.h"
#include "pgSnp.h"
#include "genePred.h"
#include "gpFx.h"

/* remember!
getGPsWithFrames
*/

struct gpFx *gpFxInCodingExon(struct pgSnp *pgSnp, struct genePred *pred, 
    int exonNum)
{
return NULL;
}

struct gpFx *gpFxInExon(struct pgSnp *pgSnp, struct genePred *pred, 
    int exonNum)
{
struct gpFx *effectsList = NULL, *effects;

if (positiveRangeIntersection(pred->cdsStart, pred->cdsEnd,
	pgSnp->chromStart, pgSnp->chromEnd))
    {
    // we're in CDS
    effects = gpFxInCodingExon(pgSnp, pred, exonNum);
    if (effects)
	slAddHead(&effectsList, effects);
    }

if (positiveRangeIntersection(pred->txStart, pred->cdsStart,
	pgSnp->chromStart, pgSnp->chromEnd))
    {
    // we're in 5' UTR
    }

if (positiveRangeIntersection(pred->txStart, pred->cdsStart,
	pgSnp->chromStart, pgSnp->chromEnd))
    {
    // we're in 3' UTR
    }

return effectsList;
}

struct gpFx *gpFxCheckExons(struct pgSnp *pgSnp, struct genePred *pred)
// check to see if the pgSnp is in an exon or an intron
{
int ii;
struct gpFx *effectsList = NULL, *effects;

for(ii=0; ii < pred->exonCount; ii++)
    {
    // check if in an exon
    if (positiveRangeIntersection(pred->exonStarts[ii], pred->exonEnds[ii], 
	    pgSnp->chromStart, pgSnp->chromEnd))
	{
	effects = gpFxInExon(pgSnp, pred, ii);
	if (effects)
	    slAddHead(&effectsList, effects);
	}

    // check if in intron
    if (ii < pred->exonCount - 1)
	{
	if (positiveRangeIntersection(pred->exonEnds[ii], 
		pred->exonStarts[ii+1],
		pgSnp->chromStart, pgSnp->chromEnd))
	    {
	    AllocVar(effects);
	    effects->gpFxType = gpFxIntron;
	    effects->gpFxNumber = ii;
	    slAddHead(&effectsList, effects);
	    }
	}
    }

return effectsList;
}

struct gpFx *gpFxPredEffect(struct pgSnp *pgSnp, struct genePred *pred)
// return the predicted effect(s) of a variation on a genePred
{
// first check to see if SNP is in an exon
struct gpFx *effects = gpFxCheckExons(pgSnp, pred);

if (effects != NULL)
    return effects;

// default is no effect
static struct gpFx noEffect;
noEffect.next = NULL;
noEffect.gpFxType = gpFxNone;

return &noEffect;
}
