#ifndef LOCALNEARBESTFILTER_H
#define LOCALNEARBESTFILTER_H
struct cDnaQuery;

void localNearBestFilter(struct cDnaQuery *cdna, float localNearBest, int minLocalBestCnt);
/* Local near best in genome filter. Algorithm is based on
 * pslReps.  This avoids dropping exons in drafty genomes.  See source for
 * detailed doc. */
#endif
