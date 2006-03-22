/* cDnaStats - collect statistics on alignment filtering */
#include "common.h"
#include "cDnaStats.h"

static void updateCounter(struct cDnaCnts *cnts)
/* update counts after processing a query */
{
if (cnts->aligns > cnts->prevAligns)
    {
    /* at least one alignment dropped from query */
    cnts->queries++;
    }
cnts->prevAligns = cnts->aligns;
}

void cDnaStatsUpdate(struct cDnaStats *stats)
/* update counters after processing a one cDNAs worth of alignments */
{
updateCounter(&stats->totalCnts);
updateCounter(&stats->badCnts);
updateCounter(&stats->keptCnts);
updateCounter(&stats->weirdOverCnts);
updateCounter(&stats->weirdKeptCnts);
updateCounter(&stats->minQSizeCnts);
updateCounter(&stats->overlapCnts);
updateCounter(&stats->minIdCnts);
updateCounter(&stats->minCoverCnts);
updateCounter(&stats->minAlnSizeCnts);
updateCounter(&stats->minNonRepSizeCnts);
updateCounter(&stats->maxRepMatchCnts);
updateCounter(&stats->maxAlignsCnts);
updateCounter(&stats->localBestCnts);
updateCounter(&stats->globalBestCnts);
updateCounter(&stats->minSpanCnts);
}

static void verbStats(int level, char *label, struct cDnaCnts *cnts, boolean always)
/* output stats */
{
if ((cnts->aligns > 0) || always)
    verbose(level, "%18s:\t%d\t%d\n", label, cnts->queries, cnts->aligns);
}

void cDnaStatsPrint(struct cDnaStats *stats, int level)
/* print stats if at verbose level or above */
{
verbose(level,"%18s \tseqs\taligns\n", "");
verbStats(level, "total", &stats->totalCnts, FALSE);
verbStats(level, "drop invalid", &stats->badCnts, FALSE);
verbStats(level, "drop minAlnSize", &stats->minAlnSizeCnts, FALSE);
verbStats(level, "drop minNonRepSize", &stats->minNonRepSizeCnts, FALSE);
verbStats(level, "drop maxRepMatch", &stats->maxRepMatchCnts, FALSE);
verbStats(level, "drop min size", &stats->minQSizeCnts, FALSE);
verbStats(level, "drop minIdent", &stats->minIdCnts, FALSE);
verbStats(level, "drop minCover", &stats->minCoverCnts, FALSE);
verbStats(level, "weird over", &stats->weirdOverCnts, FALSE);
verbStats(level, "kept weird", &stats->weirdKeptCnts, FALSE);
verbStats(level, "drop overlap", &stats->overlapCnts, FALSE);
verbStats(level, "drop minSpan", &stats->minSpanCnts, FALSE);
verbStats(level, "drop localBest", &stats->localBestCnts, FALSE);
verbStats(level, "drop globalBest", &stats->globalBestCnts, FALSE);
verbStats(level, "drop maxAligns", &stats->maxAlignsCnts, FALSE);
verbStats(level, "kept", &stats->keptCnts, TRUE);
}
