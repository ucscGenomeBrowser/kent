#include "common.h"
#include "blatStats.h"


float divAsPercent(double a, double b)
/* Return a/b * 100. */
{
if (b == 0)
   return 0.0;
return 100.0 * a / b;
}

void reportStat(FILE *f, char *name, struct stat *stat, boolean reportPercentId)
/* Print out one set of stats. */
{
char buf[64];
fprintf(f, "%-15s ", name);
sprintf(buf, "%4.1f%% (%d/%d)", 
	divAsPercent(stat->basesPainted, stat->basesTotal),
	stat->basesPainted, stat->basesTotal);
fprintf(f, "%-25s ", buf);
sprintf(buf, "%4.1f%% (%d/%d)", 
	divAsPercent(stat->hits, stat->features),
	stat->hits, stat->features);
fprintf(f, "%-20s ", buf);
if (reportPercentId)
    fprintf(f, "%4.1f%%", divAsPercent(stat->cumIdRatio, stat->basesPainted));
fprintf(f, "\n");
}

void reportStats(FILE *f, struct blatStats *stats, char *name, boolean reportPercentId)
/* Print out stats. */
{
fprintf(f, "%s stats:\n", name);
fprintf(f, "region         bases hit           features hit ");
if (reportPercentId)
    fprintf(f, "percent identity");
fprintf(f, "\n");
fprintf(f, "------------------------------------------------");
if (reportPercentId)
    fprintf(f, "---------------------");
fprintf(f, "\n");
reportStat(f, "upstream 100", &stats->upstream100, reportPercentId);
reportStat(f, "upstream 200", &stats->upstream200, reportPercentId);
reportStat(f, "upstream 400", &stats->upstream400, reportPercentId);
reportStat(f, "upstream 800", &stats->upstream800, reportPercentId);
reportStat(f, "downstream 200", &stats->downstream200, reportPercentId);
reportStat(f, "mrna total", &stats->mrnaTotal, reportPercentId);
reportStat(f, "5' UTR", &stats->utr5, reportPercentId);
reportStat(f, "3' UTR", &stats->utr3, reportPercentId);
reportStat(f, "first CDS", &stats->firstCds, reportPercentId);
reportStat(f, "middle CDS", &stats->middleCds, reportPercentId);
reportStat(f, "end CDS", &stats->endCds, reportPercentId);
reportStat(f, "only CDS", &stats->onlyCds, reportPercentId);
reportStat(f, "5' splice", &stats->splice5, reportPercentId);
reportStat(f, "3' splice", &stats->splice3, reportPercentId);
reportStat(f, "first intron", &stats->firstIntron, reportPercentId);
reportStat(f, "middle intron", &stats->middleIntron, reportPercentId);
reportStat(f, "end intron", &stats->endIntron, reportPercentId);
reportStat(f, "only intron", &stats->onlyIntron, reportPercentId);
fprintf(f, "\n");
}

void addStat(struct stat *a, struct stat *acc)
/* Add stat from a into acc. */
{
acc->features += a->features;
acc->hits += a->hits;
acc->basesTotal += a->basesTotal;
acc->basesPainted += a->basesPainted;
acc->cumIdRatio += a->cumIdRatio;
}

struct blatStats *addStats(struct blatStats *a, struct blatStats *acc)
/* Add stats in a to acc. */
{
addStat(&a->upstream100, &acc->upstream100);
addStat(&a->upstream200, &acc->upstream200);
addStat(&a->upstream400, &acc->upstream400);
addStat(&a->upstream800, &acc->upstream800);
addStat(&a->mrnaTotal, &acc->mrnaTotal);
addStat(&a->utr5, &acc->utr5);
addStat(&a->firstCds, &acc->firstCds);
addStat(&a->firstIntron, &acc->firstIntron);
addStat(&a->middleCds, &acc->middleCds);
addStat(&a->onlyCds, &acc->onlyCds);
addStat(&a->middleIntron, &acc->middleIntron);
addStat(&a->onlyIntron, &acc->onlyIntron);
addStat(&a->endCds, &acc->endCds);
addStat(&a->endIntron, &acc->endIntron);
addStat(&a->utr3, &acc->utr3);
addStat(&a->splice5, &acc->splice5);
addStat(&a->splice3, &acc->splice3);
addStat(&a->downstream200, &acc->downstream200);
}

struct blatStats *sumStatsList(struct blatStats *list)
/* Return sum of all stats. */
{
struct blatStats *el, *stats;
AllocVar(stats);
for (el = list; el != NULL; el = el->next)
    addStats(el, stats);
return stats;
}

