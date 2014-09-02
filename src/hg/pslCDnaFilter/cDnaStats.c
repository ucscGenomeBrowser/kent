/* cDnaStats - collect statistics on alignment filtering */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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
updateCounter(&stats->blackListCnts);
}

static void verbStats(FILE* fh, char *label, struct cDnaCnts *cnts, boolean always)
/* output one stats row */
{
if ((cnts->aligns > 0) || always)
    fprintf(fh, "%18s:\t%d\t%d\n", label, cnts->queries, cnts->aligns);
}

void cDnaStatsPrint(struct cDnaStats *stats, FILE* fh)
/* print filter stats to file  */
{
fprintf(fh, "%18s \tseqs\taligns\n", "");
verbStats(fh, "total", &stats->totalCnts, TRUE);
verbStats(fh, "weird over", &stats->weirdOverCnts, FALSE);
verbStats(fh, "drop invalid", &stats->badDropCnts, FALSE);
verbStats(fh, "drop minAlnSize", &stats->minAlnSizeDropCnts, FALSE);
verbStats(fh, "drop minNonRepSize", &stats->minNonRepSizeDropCnts, FALSE);
verbStats(fh, "drop maxRepMatch", &stats->maxRepMatchDropCnts, FALSE);
verbStats(fh, "drop min size", &stats->minQSizeDropCnts, FALSE);
verbStats(fh, "drop minIdent", &stats->minIdDropCnts, FALSE);
verbStats(fh, "drop minCover", &stats->minCoverDropCnts, FALSE);
verbStats(fh, "drop overlap", &stats->overlapDropCnts, FALSE);
verbStats(fh, "drop minSpan", &stats->minSpanDropCnts, FALSE);
verbStats(fh, "drop localBest", &stats->localBestDropCnts, FALSE);
verbStats(fh, "drop globalBest", &stats->globalBestDropCnts, FALSE);
verbStats(fh, "drop maxAligns", &stats->maxAlignsDropCnts, FALSE);
verbStats(fh, "drop nonUnique", &stats->nonUniqueMap, FALSE);
verbStats(fh, "drop blackList", &stats->blackListCnts, FALSE);
verbStats(fh, "drop weird", &stats->weirdDropCnts, FALSE);
verbStats(fh, "kept weird", &stats->weirdKeptCnts, FALSE);
verbStats(fh, "kept", &stats->keptCnts, TRUE);
}
