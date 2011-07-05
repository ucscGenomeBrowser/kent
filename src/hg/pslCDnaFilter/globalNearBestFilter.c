#include "common.h"
#include "globalNearBestFilter.h"
#include "cDnaAligns.h"
#include "cDnaReader.h"

static float getAlnScore(struct cDnaAlign *refAln)
/* get the score for an alignment, getting best score for a refAln that is
 * haplotype aligns, */
{
float best = refAln->score;
struct cDnaAlignRef *hapAln;
for (hapAln = refAln->hapAlns; hapAln != NULL; hapAln = hapAln->next)
    best = max(best, hapAln->ref->score);
return best;
}

static float getBestScore(struct cDnaQuery *cdna)
/* find best score for alignments that have not been dropped */
{
float best = -1.0;
struct cDnaAlignRef *hapSetAln;
for (hapSetAln = cdna->hapSets; hapSetAln != NULL; hapSetAln = hapSetAln->next)
    {
    if (!hapSetAln->ref->drop)
        {
        float score = getAlnScore(hapSetAln->ref);
        best = max(score, best);
        }
    }
return best;
}

void globalNearBestFilter(struct cDnaQuery *cdna, float globalNearBest)
/* global near best in genome filter. */
{
float best = getBestScore(cdna);
float thresh = best*(1.0-globalNearBest);
struct cDnaAlignRef *hapSetAln;
for (hapSetAln = cdna->hapSets; hapSetAln != NULL; hapSetAln = hapSetAln->next)
    if (!hapSetAln->ref->drop)
        {
        float score = getAlnScore(hapSetAln->ref);
        if (score < thresh)
            cDnaAlignDrop(hapSetAln->ref, TRUE, &hapSetAln->ref->cdna->stats->globalBestDropCnts, "global near best %0.4g, best=%0.4g, th=%0.4g",
                          score, best, thresh);
        }
}

