#include "common.h"
#include "localNearBestFilter.h"
#include "cDnaAligns.h"
#include "cDnaReader.h"
#include "psl.h"

static void baseScoreUpdate(struct cDnaQuery *cdna, struct cDnaAlign *aln,
                            float *baseScores)
/* update best local query scores for an alignment psl.*/
{
struct psl *psl = aln->psl;
int iBlk, i;

for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    struct range q = cDnaQueryBlk(cdna, psl, iBlk);
    for (i = q.start; i < q.end; i++)
        baseScores[i] = max(aln->score, baseScores[i]);
    }
}

static void computeLocalScores(struct cDnaQuery *cdna, float *baseScores)
/* compute local per-base score array for a cDNA. */
{
struct cDnaAlign *aln;

for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop)
        baseScoreUpdate(cdna, aln, baseScores);
    }
}

static void localNearBestVerb(struct cDnaAlign *aln, boolean pass,
                              float nearScore, int topCnt, int chkCnt)
/* verbose tracing for isLocalNearBest */
{
cDnaAlignVerb(5, aln, "isLocalNearBest: %s nearScore: %0.3f %d in %d",
              (pass ? "yes" : "no "), nearScore, topCnt, chkCnt);
}

static boolean isLocalNearBest(struct cDnaQuery *cdna, struct cDnaAlign *aln,
                               float localNearBest, int minLocalBestCnt, float *baseScores)
/* check if aligned blocks pass local near-beast filter. */
{
float alnNearScore = cDnaAlignHapSetScore(aln) * (1.0+localNearBest);
struct psl *psl = aln->psl;
int iBlk, i;
int chkCnt = 0, topCnt = 0;

for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    struct range q = cDnaQueryBlk(cdna, psl, iBlk);
    for (i = q.start; i < q.end; i++)
        {
        chkCnt++;
        if (alnNearScore >= baseScores[i])
            {
            if (++topCnt >= minLocalBestCnt)
                {
                localNearBestVerb(aln, TRUE, alnNearScore, topCnt, chkCnt);
                return TRUE;
                }
            }
        }
    }
localNearBestVerb(aln, FALSE, alnNearScore, topCnt, chkCnt);
return FALSE;
}

void localNearBestFilter(struct cDnaQuery *cdna, float localNearBest, int minLocalBestCnt)
/* Local near best in genome filter. Algorithm is based on
 * pslReps.  This avoids dropping exons in drafty genomes.
 *
 * 1) Assign a score to each alignment that is weight heavily by percent id
 *    and has a bonus for apparent introns.
 * 2) Create a per-mRNA base vector of near-best scores:
 *    - foreach alignment:
 *      - foreach base of the alignment:
 *        - if the alignment's score is greater than the score at the position
 *          in the vector, replaced the vector score.
 * 3) Foreach alignment:
 *    - count the number of bases within the near-top range of the best
 *     score for the base.
 * 4) If an alignment has at least 30 of it's bases within near-top, keep the
 *    alignment.
 */
{
float baseScores[cDnaQuerySize(cdna)];
zeroBytes(baseScores,  cDnaQuerySize(cdna)*sizeof(float));
computeLocalScores(cdna, baseScores);
struct cDnaAlignRef *hapSetAln;

for (hapSetAln = cdna->hapSets; hapSetAln != NULL; hapSetAln = hapSetAln->next)
    {
    if (!hapSetAln->ref->drop && !isLocalNearBest(cdna, hapSetAln->ref, localNearBest, minLocalBestCnt, baseScores))
        cDnaAlignDrop(hapSetAln->ref, TRUE, &cdna->stats->localBestDropCnts, "local near best %0.4g", hapSetAln->ref->score);
    }
}

