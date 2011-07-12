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
updateCounter(&stats->keptCnts);
updateCounter(&stats->weirdOverCnts);
updateCounter(&stats->weirdKeptCnts);
updateCounter(&stats->badDropCnts);
updateCounter(&stats->weirdDropCnts);
updateCounter(&stats->minQSizeDropCnts);
updateCounter(&stats->overlapDropCnts);
updateCounter(&stats->minIdDropCnts);
updateCounter(&stats->minCoverDropCnts);
updateCounter(&stats->minAlnSizeDropCnts);
updateCounter(&stats->minNonRepSizeDropCnts);
updateCounter(&stats->maxRepMatchDropCnts);
updateCounter(&stats->maxAlignsDropCnts);
updateCounter(&stats->localBestDropCnts);
updateCounter(&stats->globalBestDropCnts);
updateCounter(&stats->minSpanDropCnts);
updateCounter(&stats->nonUniqueMap);
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
verbStats(level, "total", &stats->totalCnts, TRUE);
verbStats(level, "weird over", &stats->weirdOverCnts, FALSE);
verbStats(level, "drop invalid", &stats->badDropCnts, FALSE);
verbStats(level, "drop minAlnSize", &stats->minAlnSizeDropCnts, FALSE);
verbStats(level, "drop minNonRepSize", &stats->minNonRepSizeDropCnts, FALSE);
verbStats(level, "drop maxRepMatch", &stats->maxRepMatchDropCnts, FALSE);
verbStats(level, "drop min size", &stats->minQSizeDropCnts, FALSE);
verbStats(level, "drop minIdent", &stats->minIdDropCnts, FALSE);
verbStats(level, "drop minCover", &stats->minCoverDropCnts, FALSE);
verbStats(level, "drop weird", &stats->weirdDropCnts, FALSE);
verbStats(level, "drop overlap", &stats->overlapDropCnts, FALSE);
verbStats(level, "drop minSpan", &stats->minSpanDropCnts, FALSE);
verbStats(level, "drop localBest", &stats->localBestDropCnts, FALSE);
verbStats(level, "drop globalBest", &stats->globalBestDropCnts, FALSE);
verbStats(level, "drop maxAligns", &stats->maxAlignsDropCnts, FALSE);
verbStats(level, "drop nonUnique", &stats->nonUniqueMap, FALSE);
verbStats(level, "kept weird", &stats->weirdKeptCnts, FALSE);
verbStats(level, "kept", &stats->keptCnts, TRUE);
}
